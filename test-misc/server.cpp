
#include "quotesource/quotesource.h"
#include "goldmine/data.h"

#include "cppio/iolinemanager.h"

#include <thread>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/normal_distribution.hpp>

#include <signal.h>
#include <cstdlib>
#include <ctime>

using namespace goldmine;

int main(int argc, char** argv)
{
	if(argc < 2)
	{
		std::cerr << "Usage ./server <quotesource-endpoint>" << '\n';
		return 1;
	}
	signal(SIGPIPE, SIG_IGN);
	auto man = cppio::createLineManager();
	QuoteSource source(man, argv[1]);
	source.start();

	boost::random::mt19937 gen;
	boost::random::normal_distribution<> dist(0, 1);
	double price = 100;
	while(true)
	{
		double dp = (100. + dist(gen)) / 100.;
		if((rand() % 10) == 0)
		{
			goldmine::Tick tick;
			tick.datatype = (int)goldmine::Datatype::Price;
			tick.value = price;
			tick.volume = rand() % 100 + 1;
			time_t t;
			time(&t);
			tick.timestamp = t;
			tick.useconds = 0;
			source.incomingTick("FOOBAR", tick);
		}
		price = price * dp;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

