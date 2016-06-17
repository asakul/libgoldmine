
#include "catch.hpp"

#include "quotesource/quotesourceclient.h"
#include "goldmine/data.h"
#include "io/common/inproc.h"
#include "quotesource/quotesource.h"

#include <thread>
#ifdef __MINGW32__
#include "mingw.thread.h"
#endif

using namespace goldmine;
using namespace goldmine::io;

class TickSink : public QuoteSourceClient::Sink
{
public:
	virtual ~TickSink()
	{
	}

	void incomingTick(const std::string& ticker, const Tick& tick) override
	{
		ticks.push_back(std::make_pair(ticker, tick));
	}

	std::vector<std::pair<std::string, Tick>> ticks;
};

static void sendControlMessage(const Json::Value& root, MessageProtocol& line)
{
	Json::FastWriter writer;
	auto json = writer.write(root);

	Message msg;
	msg << (uint32_t)goldmine::MessageType::Control;
	msg << json;

	line.sendMessage(msg);
}

static bool receiveControlMessage(Json::Value& root, MessageProtocol& line)
{
	root.clear();
	Message recvd;
	line.readMessage(recvd);

	uint32_t incomingMessageType = recvd.get<uint32_t>(0);
	REQUIRE(incomingMessageType == (int)goldmine::MessageType::Control);

	auto json = recvd.get<std::string>(1);
	Json::Reader reader;
	bool parseOk = reader.parse(json, root);
	if(!parseOk)
		return false;

	return true;
}


TEST_CASE("QuotesourceClient", "[quotesourceclient]")
{
	auto manager = std::make_shared<IoLineManager>();
	manager->registerFactory(std::unique_ptr<InprocLineFactory>(new InprocLineFactory()));

	QuoteSourceClient client(manager, "inproc://quotesource");
	auto sink = std::make_shared<TickSink>();
	client.registerSink(sink);

	QuoteSource source(manager, "inproc://quotesource");
	source.start();

	client.startStream("t:FOO");

	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	Tick tick;
	tick.timestamp = 12;
	tick.useconds = 0;
	tick.datatype = (int)Datatype::Price;
	tick.value = decimal_fixed(42, 0);
	tick.volume = 100;
	source.incomingTick("FOO", tick);

	client.stop();
	source.stop();

	REQUIRE(sink->ticks.size() == 1);
	REQUIRE(sink->ticks.front().second == tick);

}

