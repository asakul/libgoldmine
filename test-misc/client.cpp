
#include "quotesource/quotesourceclient.h"
#include "goldmine/data.h"

#include "cppio/iolinemanager.h"

#include <thread>
#include <iostream>

#include <cstdlib>
#include <ctime>

using namespace goldmine;

class Sink : public QuoteSourceClient::Sink
{
public:
	void incomingTick(const std::string& ticker, const Tick& tick)
	{
		std::cout << "Incoming tick: " << ticker << ": " << tick.value.toDouble() << '\n';
	}
};

int main(int argc, char** argv)
{
	if(argc < 2)
	{
		std::cerr << "Usage ./client <quotesource-endpoint>" << '\n';
		return 1;
	}
	auto man = cppio::createLineManager();
	QuoteSourceClient client(man, argv[1]);
	auto sink = std::make_shared<Sink>();
	client.registerSink(sink);
	client.startStream("t:FOOBAR");

	while(true)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

