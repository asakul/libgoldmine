/*
 * quotesource.cpp
 */

#include "quotesource.h"

#include <functional>

#include "io/iolinemanager.h"

namespace goldmine
{
using namespace io;

class Client
{
public:
	Client(const std::shared_ptr<IoLine>& line) : m_proto(line),
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

	void eventLoop()
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
				// TODO pass to reactors
			}
			catch(const std::runtime_error& e)
			{
				// No idea what am I supposed to do here but whatever
			}
		}
	}

	Message handle(const Message& incomingMessage)
	{
		uint32_t messageType;
		incomingMessage.get(messageType, 0);

		if(messageType == (int)MessageType::Control)
		{
			return handleControlMessage(incomingMessage);
		}

		Message emptyMessage;
		return emptyMessage;
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

	void incomingTick(const Tick& tick)
	{
	}

private:
	MessageProtocol m_proto;
	boost::thread m_clientThread;
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
				m_impl->clients.emplace_back(line);
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

} /* namespace goldmine */
