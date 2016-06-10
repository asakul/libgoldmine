
#include "catch.hpp"

#include "quotesource/quotesourceclient.h"
#include "goldmine/data.h"
#include "io/common/inproc.h"
#include "quotesource/quotesource.h"

#include <thread>

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

