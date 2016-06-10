
#include "quotesourceclient.h"
#include "io/message.h"
#include "exceptions.h"

#include "json/json.h"

#include <boost/algorithm/string.hpp>

#include <thread>

namespace goldmine
{

struct QuoteSourceClient::Impl
{
	Impl(const std::shared_ptr<io::IoLineManager>& m, const std::string& a) : manager(m),
		address(a),
		run(false)
	{
	}

	std::vector<std::shared_ptr<QuoteSourceClient::Sink>> sinks;
	std::vector<boost::shared_ptr<QuoteSourceClient::Sink>> boostSinks;
	std::vector<QuoteSourceClient::Sink*> rawSinks;
	std::shared_ptr<io::IoLineManager> manager;
	std::string address;
	std::shared_ptr<io::IoLine> line;
	std::thread streamThread;
	bool run;

	void eventLoop(const std::string& streamId)
	{
		run = true;
		line = manager->createClient(address);
		if(line)
		{
			int timeout = 200;
			line->setOption(io::LineOption::ReceiveTimeout, &timeout);
			io::MessageProtocol proto(line);
			io::Message msg;
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

			io::Message response;
			proto.readMessage(response);

			// TODO check
			while(run)
			{
				try
				{
					io::Message incoming;
					proto.readMessage(incoming);
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
				catch(const io::TimeoutException& ex)
				{
					// Timeout, do nothing
				}
				catch(const LibGoldmineException& ex)
				{
				}
			}
		}
	}
};

QuoteSourceClient::Sink::~Sink()
{
}

QuoteSourceClient::QuoteSourceClient(const std::shared_ptr<io::IoLineManager>& manager, const std::string& address) :
	m_impl(new Impl(manager, address))
{
}

QuoteSourceClient::~QuoteSourceClient()
{
}

void QuoteSourceClient::startStream(const std::string& streamId)
{
	m_impl->streamThread = std::thread(std::bind(&Impl::eventLoop, m_impl.get(), streamId));
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

