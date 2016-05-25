/*
 * quotesource.cpp
 */

#include "quotesource.h"


#include <functional>

namespace goldmine
{

QuoteSource::Sink::Sink(zmqpp::context& ctx, const std::string& endpoint) : m_socket(ctx, zmqpp::socket_type::push)
{
	m_socket.connect(endpoint);
}

void QuoteSource::Sink::incomingBar(const goldmine::Summary& bar)
{
}

void QuoteSource::Sink::incomingTick(const std::string& ticker, const goldmine::Tick& tick)
{
	const char* t = reinterpret_cast<const char*>(&tick);
	std::string rawTick(t, t + sizeof(tick));
	zmqpp::message msg;
	msg.add(0, 0);
	msg << (uint32_t) goldmine::MessageType::Data;
	msg << ticker;
	msg << rawTick;
	m_socket.send(msg);
}

QuoteSource::QuoteSource(zmqpp::context& ctx, const std::string& endpoint) : m_ctx(ctx),
	m_endpoint(endpoint), m_run(false)
{
}

QuoteSource::~QuoteSource()
{
	if(m_run)
		stop();
}

void QuoteSource::addReactor(const Reactor::Ptr& reactor)
{
	m_reactors.push_back(reactor);
}

void QuoteSource::removeReactor(const Reactor::Ptr& reactor)
{
}

std::unique_ptr<QuoteSource::Sink> QuoteSource::makeTickSink()
{
	return std::unique_ptr<QuoteSource::Sink>(new Sink(m_ctx, "inproc://quotesource-sink"));
}

void QuoteSource::start()
{
	m_thread = boost::thread(std::bind(&QuoteSource::eventLoop, this));
}

void QuoteSource::stop() noexcept
{
	try
	{
		m_run = false;
		m_thread.interrupt();
		if(m_thread.joinable())
			m_thread.try_join_for(boost::chrono::milliseconds(200));
	}
	catch(const std::exception& e)
	{
		// Oh my
		m_thread.detach();
	}
}


void QuoteSource::eventLoop()
{
	m_run = true;

	zmqpp::socket controlSocket(m_ctx, zmqpp::socket_type::router);
	controlSocket.bind(m_endpoint);

	zmqpp::socket sinkSocket(m_ctx, zmqpp::socket_type::pull);
	sinkSocket.bind("inproc://quotesource-sink");

	zmqpp::poller poller;
	poller.add(controlSocket);
	poller.add(sinkSocket);

	while(m_run)
	{
		try
		{
			if(poller.poll(100))
			{
				zmqpp::message recvd;
				if(poller.has_input(controlSocket))
				{
					controlSocket.receive(recvd);
					handleSocket(controlSocket, recvd);
				}
				if(poller.has_input(sinkSocket))
				{
					handleSinkSocket(controlSocket, sinkSocket);
				}
			}
		}
		catch(const LibGoldmineException& e)
		{
			for(const auto& reactor : m_reactors)
			{
				reactor->exception(e);
			}
		}
	}
}

void QuoteSource::handleSocket(zmqpp::socket& control, zmqpp::message& msg)
{
	std::string peerId = msg.get<std::string>(0);

	uint32_t messageType = msg.get<uint32_t>(2);

	switch(goldmine::MessageType(messageType))
	{
		case goldmine::MessageType::Control:
			{

				std::string json = msg.get<std::string>(3);

				Json::Value root;
				Json::Reader reader;
				if(!reader.parse(json, root))
					BOOST_THROW_EXCEPTION(ParameterError() << errinfo_str("Unable to parse incomning JSON"));

				handleControl(peerId, control, root);
			}
		case goldmine::MessageType::Data:
		case goldmine::MessageType::Service:
		case goldmine::MessageType::Event:
			break;
		default:
			BOOST_THROW_EXCEPTION(ProtocolError() << errinfo_str("Invalid MessageType"));
	}

}

void QuoteSource::handleControl(const std::string& peerId, zmqpp::socket& control, const Json::Value& root)
{
	if(root["command"] == "request-capabilities")
	{
		Json::Value responseJson;
		responseJson["node-type"] = "quotesource";
		responseJson["protocol-version"] = 2;
		Json::FastWriter writer;
		auto json = writer.write(responseJson);

		zmqpp::message response;
		response << peerId;
		response.push_back(0, 0);
		response.add((uint32_t)goldmine::MessageType::Control);
		response.add(json);

		control.send(response);
	}
	else if(root["command"] == "start-stream")
	{
		Client newClient;
		newClient.peerId = peerId;
		m_clients[peerId] = newClient;
	}
}

void QuoteSource::handleSinkSocket(zmqpp::socket& control, zmqpp::socket& sink)
{
	for(const auto& clientPair: m_clients)
	{
		auto peerId = clientPair.first;
		zmqpp::message tickMessage;
		sink.receive(tickMessage);

		zmqpp::message clientMessage;
		clientMessage << peerId;
		clientMessage.add(0, 0);
		clientMessage << tickMessage.get<uint32_t>(2);
		clientMessage << tickMessage.get<std::string>(3);
		clientMessage << tickMessage.get<std::string>(4);

		control.send(clientMessage);
	}
}

} /* namespace goldmine */
