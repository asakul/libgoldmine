
#include "quotesourceclient.h"
#include "cppio/message.h"
#include "cppio/errors.h"
#include "goldmine/exceptions.h"

#include "json/json.h"

#include <boost/algorithm/string.hpp>

#include <boost/thread.hpp>

namespace goldmine
{

struct QuoteSourceClient::Impl
{
	Impl(const std::shared_ptr<cppio::IoLineManager>& m, const std::string& a) : manager(m),
		address(a),
		run(false)
	{
	}

	std::vector<std::shared_ptr<QuoteSourceClient::Sink>> sinks;
	std::vector<boost::shared_ptr<QuoteSourceClient::Sink>> boostSinks;
	std::vector<QuoteSourceClient::Sink*> rawSinks;
	std::shared_ptr<cppio::IoLineManager> manager;
	std::string address;
	std::shared_ptr<cppio::IoLine> line;
	boost::thread streamThread;
	bool run;

	void eventLoop(const std::string& streamId)
	{
		run = true;
		while(run)
		{
			line = std::shared_ptr<cppio::IoLine>(manager->createClient(address));
			if(line)
			{
				int timeout = 2000;
				line->setOption(cppio::LineOption::ReceiveTimeout, &timeout);
				cppio::MessageProtocol proto(line.get());
				cppio::Message msg;
				msg << (uint32_t)MessageType::Control;

				Json::Value root;
				root["command"] = "start-stream";
				std::vector<std::string> tickers;
				boost::split(tickers, streamId, boost::is_any_of(","));
				Json::Value tickersValue(Json::arrayValue);
				for(const auto& ticker : tickers)
				{
					tickersValue.append(ticker);
				}
				root["tickers"] = tickersValue;
				Json::FastWriter writer;
				msg << writer.write(root);

				proto.sendMessage(msg);

				cppio::Message response;
				proto.readMessage(response);

				// TODO check
				while(run)
				{
					try
					{
						cppio::Message incoming;
						ssize_t rc = proto.readMessage(incoming);
						if(rc > 0)
						{
							uint32_t messageType = incoming.get<uint32_t>(0);
							if(messageType == (int)goldmine::MessageType::Data)
							{
								auto ticker = incoming.get<std::string>(1);
								size_t size = incoming.frame(2).size();
								const void* ticks = incoming.frame(2).data();
								const Tick* tick = reinterpret_cast<const Tick*>(ticks);
								for(const auto& sink : sinks)
								{
									sink->incomingTick(ticker, *tick);
								}
								for(const auto& sink : boostSinks)
								{
									sink->incomingTick(ticker, *tick);
								}
								for(const auto& sink : rawSinks)
								{
									sink->incomingTick(ticker, *tick);
								}
							}

						}
						else if(rc != cppio::eTimeout)
						{
							break;
						}
					}
					catch(const LibGoldmineException& ex)
					{
					}
					sendHeartbeat(proto);
				}
			}
			else
			{
				boost::this_thread::sleep_for(boost::chrono::seconds(5));
			}
		}
	}

	void sendHeartbeat(cppio::MessageProtocol& proto)
	{
		cppio::Message msg;
		msg << (uint32_t)MessageType::Service;
		msg << (uint32_t)ServiceDataType::Heartbeat;

		proto.sendMessage(msg);
	}
};

QuoteSourceClient::Sink::~Sink()
{
}

QuoteSourceClient::QuoteSourceClient(const std::shared_ptr<cppio::IoLineManager>& manager, const std::string& address) :
	m_impl(new Impl(manager, address))
{
}

QuoteSourceClient::~QuoteSourceClient()
{
}

void QuoteSourceClient::startStream(const std::string& streamId)
{
	m_impl->streamThread = boost::thread(std::bind(&Impl::eventLoop, m_impl.get(), streamId));
}

void QuoteSourceClient::stop()
{
	m_impl->run = false;
	if(m_impl->streamThread.joinable())
		m_impl->streamThread.join();
}

void QuoteSourceClient::registerSink(const std::shared_ptr<Sink>& sink)
{
	m_impl->sinks.push_back(sink);
}

void QuoteSourceClient::registerBoostSink(const boost::shared_ptr<Sink>& sink)
{
	m_impl->boostSinks.push_back(sink);
}

void QuoteSourceClient::registerRawSink(Sink* sink)
{
	m_impl->rawSinks.push_back(sink);
}

}

