/*
 * quotesource.cpp
 */

#include "quotesource.h"

#include <atomic>
#include <functional>
#include <unordered_set>
#include <boost/lockfree/spsc_queue.hpp>

#include "json/json.h"
#include "cppio/iolinemanager.h"

namespace goldmine
{
using namespace cppio;

class Client;
struct QuoteSource::Impl
{
	Impl(const std::shared_ptr<IoLineManager>& m) : manager(m),
		run(false)
	{
	}

	void removeClient(Client* client)
	{
		boost::unique_lock<boost::mutex> lock(clientMutex);
		clients.erase(std::remove_if(clients.begin(), clients.end(), [&](const std::unique_ptr<Client>& c)
					{ return c.get() == client; }), clients.end());
	}

	std::shared_ptr<IoLineManager> manager;
	std::string endpoint;
	boost::thread acceptThread;
	std::atomic<bool> run;
	std::vector<Reactor::Ptr> reactors;

	boost::mutex clientMutex;
	std::vector<std::unique_ptr<Client>> clients;
};

class Client
{
public:
	Client(const std::shared_ptr<IoLine>& line, QuoteSource::Impl* impl) :
		m_quotesource(impl),
		m_proto(line),
		m_run(false),
		m_manualMode(false),
		m_tickQueue(1024),
		m_nextTickMessages(0)
	{
		int timeout = 100;
		line->setOption(LineOption::ReceiveTimeout, &timeout);
	}

	Client(Client&& other) : m_proto(std::move(other.m_proto)),
		m_clientThread(std::move(other.m_clientThread)),
		m_run(other.m_run.load()),
		m_tickQueue(1024),
		m_nextTickMessages(0)
	{
	}

	virtual ~Client()
	{
	}

	void start()
	{
		m_clientThread = boost::thread(std::bind(&Client::eventLoop, this));
	}

	void stop()
	{
		m_tickQueueCondition.notify_all();
		m_run = false;
		m_clientThread.interrupt();
		if(m_clientThread.joinable())
			m_clientThread.try_join_for(boost::chrono::milliseconds(200));
	}

	void eventLoop();

	Message handle(const Message& incomingMessage)
	{
		uint32_t messageType;
		incomingMessage.get(messageType, 0);

		if(messageType == (int)MessageType::Control)
		{
			return handleControlMessage(incomingMessage);
		}
		else if(messageType == (int)MessageType::Service)
		{
			return handleServiceMessage(incomingMessage);
		}

		BOOST_THROW_EXCEPTION(ProtocolError() << errinfo_str("Invalid message type"));
	}

	Message handleControlMessage(const Message& incomingMessage)
	{
		std::string json;
		incomingMessage.get(json, 1);

		Json::Value root;
		Json::Reader reader;
		if(!reader.parse(json, root))
			BOOST_THROW_EXCEPTION(ProtocolError() << errinfo_str("Invalid json"));

		if(root["command"] == "request-capabilities")
		{
			return makeCapabilitiesMessage();
		}
		else if(root["command"] == "start-stream")
		{
			std::vector<std::string> tickers;
			auto& tickersArray = root["tickers"];
			for(size_t i = 0; i < tickersArray.size(); i++)
			{
				auto t = tickersArray[(int)i].asString();
				tickers.push_back(t);
			}
			m_manualMode = root["manual-mode"].asBool();
			startStream(tickers);
			if(m_manualMode)
			{
				m_senderThread = boost::thread(std::bind(&Client::sendStreamThread, this));
			}

			for(const auto& reactor : m_quotesource->reactors)
			{
				for(const auto& ticker : tickers)
					reactor->clientRequestedStream("", ticker);
			}

			return makeOkMessage();
		}
		BOOST_THROW_EXCEPTION(ProtocolError() << errinfo_str("Invalid control command"));
	}

	Message handleServiceMessage(const Message& incomingMessage)
	{
		int serviceMessageType = incomingMessage.get<uint32_t>(1);
		if(serviceMessageType == (int)ServiceDataType::NextTick)
		{
			m_nextTickMessages.fetch_add(1);
			m_tickQueueCondition.notify_one();
		}

		Message msg;
		return msg;
	}

	Message makeCapabilitiesMessage()
	{
		Json::Value root;
		root["node-type"] = "quotesource";
		root["protocol-version"] = 2;
		Json::FastWriter writer;

		Message outgoing;
		outgoing << (uint32_t)MessageType::Control;
		outgoing << writer.write(root);
		return outgoing;
	}

	Message makeOkMessage()
	{
		Json::Value root;
		root["result"] = "success";
		Json::FastWriter writer;

		Message outgoing;
		outgoing << (uint32_t)MessageType::Control;
		outgoing << writer.write(root);
		return outgoing;
	}

	void incomingTick(const std::string& ticker, const Tick& tick)
	{
		if(!m_manualMode)
		{
			if(m_tickers.find(ticker) != m_tickers.end())
			{
				sendTick(ticker, tick);
			}
		}
		else
		{
			m_tickQueue.push(std::make_pair(ticker, tick));
			m_tickQueueCondition.notify_one();
		}
	}

	void startStream(const std::vector<std::string>& tickers)
	{
		for(const auto& ticker : tickers)
		{
			if(ticker.substr(0, 2) != "t:")
				BOOST_THROW_EXCEPTION(ParameterError() << errinfo_str("Only tick data is supported now"));
			auto pureTicker = ticker.substr(2);
			m_tickers.insert(pureTicker);
		}
	}

	void sendStreamThread()
	{
		while(m_run)
		{
			if(!m_tickQueue.empty() && (m_nextTickMessages.load() > 0))
			{
				std::pair<std::string, Tick> tick;
				if(m_tickQueue.pop(tick))
				{
					sendTick(tick.first, tick.second);
					m_nextTickMessages.fetch_sub(1);
				}
			}
			else
			{
				boost::unique_lock<boost::mutex> lock(m_tickQueueMutex);
				m_tickQueueCondition.wait_for(lock, boost::chrono::milliseconds(40));
			}
		}
	}

	void sendTick(const std::string& ticker, const Tick& tick)
	{
		Message msg;
		msg << (uint32_t)MessageType::Data;
		msg << ticker;
		msg.addFrame(Frame(&tick, sizeof(tick)));

		m_proto.sendMessage(msg);
	}

private:
	QuoteSource::Impl* m_quotesource;
	MessageProtocol m_proto;
	boost::thread m_clientThread;
	std::unordered_set<std::string> m_tickers;
	std::atomic<bool> m_run;
	bool m_manualMode;

	boost::thread m_senderThread;
	boost::mutex m_tickQueueMutex;
	boost::condition_variable m_tickQueueCondition;
	boost::lockfree::spsc_queue<std::pair<std::string, Tick>> m_tickQueue;
	std::atomic_int m_nextTickMessages;
};


void Client::eventLoop()
{
	m_run = true;

	while(m_run)
	{
		try
		{
			Message incomingMessage;
			m_proto.readMessage(incomingMessage);

			Message outgoingMessage = handle(incomingMessage);
			if(outgoingMessage.size() > 0)
			{
				m_proto.sendMessage(outgoingMessage);
			}
		}
		catch(const TimeoutException& e)
		{
			// Meh
		}
		catch(const IoException& e)
		{
			m_run = false;
		}
		catch(const LibGoldmineException& e)
		{
			if(m_quotesource)
			{
				for(const auto& reactor : m_quotesource->reactors)
				{
					reactor->exception(e);
				}
			}

			Json::Value root;
			root["result"] = "error";
			Json::FastWriter writer;
			Message msg;
			msg << (uint32_t)MessageType::Control;
			msg << writer.write(root);
			m_proto.sendMessage(msg);
		}
	}
	m_quotesource->removeClient(this);
}

QuoteSource::QuoteSource(const std::shared_ptr<IoLineManager>& manager, const std::string& endpoint) : m_impl(new Impl(manager))
{
	m_impl->endpoint = endpoint;
}

QuoteSource::~QuoteSource()
{
	if(m_impl->run)
		stop();
}

void QuoteSource::addReactor(const Reactor::Ptr& reactor)
{
	m_impl->reactors.push_back(reactor);
}

void QuoteSource::removeReactor(const Reactor::Ptr& reactor)
{
}

void QuoteSource::start()
{
	m_impl->acceptThread = boost::thread(std::bind(&QuoteSource::eventLoop, this));
}

void QuoteSource::stop() noexcept
{
	try
	{
		m_impl->run = false;
		m_impl->acceptThread.interrupt();
		if(m_impl->acceptThread.joinable())
			m_impl->acceptThread.try_join_for(boost::chrono::milliseconds(200));
	}
	catch(const std::exception& e)
	{
	}
	for(const auto& client : m_impl->clients)
	{
		try
		{
			client->stop();
		}
		catch(const std::exception& e)
		{
		}
	}
}


void QuoteSource::eventLoop()
{
	m_impl->run = true;

	auto controlAcceptor = m_impl->manager->createServer(m_impl->endpoint);

	while(m_impl->run)
	{
		try
		{
			auto line = controlAcceptor->waitConnection(std::chrono::milliseconds(200));
			if(line)
			{
				boost::unique_lock<boost::mutex> lock(m_impl->clientMutex);
				m_impl->clients.push_back(std::unique_ptr<Client>(new Client(line, m_impl.get())));
				auto& client = m_impl->clients.back();
				client->start();
			}
		}
		catch(const LibGoldmineException& e)
		{
			for(const auto& reactor : m_impl->reactors)
			{
				reactor->exception(e);
			}
		}
	}
}

void QuoteSource::incomingTick(const std::string& ticker, const Tick& tick)
{
	boost::unique_lock<boost::mutex> lock(m_impl->clientMutex);
	for(const auto& client : m_impl->clients)
	{
		client->incomingTick(ticker, tick);
	}
}

} /* namespace goldmine */
