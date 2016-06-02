/*
 * quotesource.cpp
 */

#include "quotesource.h"

#include "io/iolinemanager.h"

#include <functional>
#include <unordered_set>

namespace goldmine
{
using namespace io;

class Client
{
public:
	Client(const std::shared_ptr<IoLine>& line, QuoteSource::Impl* impl) :
		m_quotesource(impl),
		m_proto(line),
		m_run(false)
	{
	}

	Client(Client&& other) : m_proto(std::move(other.m_proto)),
		m_clientThread(std::move(other.m_clientThread)),
		m_run(other.m_run)
	{
	}

	virtual ~Client()
	{
		stop();
	}

	void start()
	{
		m_clientThread = boost::thread(std::bind(&Client::eventLoop, this));
	}

	void stop()
	{
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
			startStream(tickers);

			return makeOkMessage();
		}
		BOOST_THROW_EXCEPTION(ProtocolError() << errinfo_str("Invalid control command"));
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
		if(m_tickers.find(ticker) != m_tickers.end())
		{
			Message msg;
			msg << (uint32_t)MessageType::Data;
			msg << ticker;
			msg.addFrame(Frame(&tick, sizeof(tick)));

			m_proto.sendMessage(msg);
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

private:
	QuoteSource::Impl* m_quotesource;
	MessageProtocol m_proto;
	boost::thread m_clientThread;
	std::unordered_set<std::string> m_tickers;
	bool m_run;
};

struct QuoteSource::Impl
{
	Impl(IoLineManager& m) : manager(m),
		run(false)
	{
	}

	IoLineManager& manager;
	std::string endpoint;
	boost::thread acceptThread;
	bool run;
	std::vector<Reactor::Ptr> reactors;
	std::vector<Client> clients;
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
				m_proto.sendMessage(outgoingMessage);
		}
		catch(const IoException& e)
		{
			// meh
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
}

QuoteSource::QuoteSource(IoLineManager& manager, const std::string& endpoint) : m_impl(std::make_unique<Impl>(manager))
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
		// Oh my
		m_impl->acceptThread.detach();
	}
}


void QuoteSource::eventLoop()
{
	m_impl->run = true;

	auto controlAcceptor = m_impl->manager.createServer(m_impl->endpoint);

	while(m_impl->run)
	{
		try
		{
			auto line = controlAcceptor->waitConnection(std::chrono::milliseconds(200));
			if(line)
			{
				m_impl->clients.emplace_back(line, m_impl.get());
				auto& client = m_impl->clients.back();
				client.start();
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
	for(auto& client : m_impl->clients)
	{
		client.incomingTick(ticker, tick);
	}
}

} /* namespace goldmine */
