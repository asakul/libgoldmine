
#include "catch.hpp"

#include "quotesource/quotesourceclient.h"
#include "goldmine/data.h"
#include "quotesource/quotesource.h"

#include <boost/thread.hpp>

using namespace goldmine;
using namespace cppio;

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
	auto manager = std::shared_ptr<IoLineManager>(createLineManager());

	QuoteSourceClient client(manager, "inproc://quotesource");
	auto sink = std::make_shared<TickSink>();
	client.registerSink(sink);

	QuoteSource source(manager, "inproc://quotesource");
	source.start();

	client.startStream("t:FOO");

	boost::this_thread::sleep_for(boost::chrono::milliseconds(100));

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

