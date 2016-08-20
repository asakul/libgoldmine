
#include "broker/brokerclient.h"
#include "goldmine/data.h"

#include "cppio/iolinemanager.h"

#include <boost/thread.hpp>

#include <signal.h>
#include <cstdlib>
#include <ctime>

using namespace goldmine;

class Reactor : public BrokerClient::Reactor
{
public:
	virtual void orderCallback(const Order::Ptr& order) override
	{
		std::cout << "Order callback: " << order->stringRepresentation() << '\n';
		if(order->state() == Order::State::Error)
		{
			std::cout << "Error: " << order->message() << '\n';
		}
	}

	virtual void tradeCallback(const Trade& trade) override
	{
		std::cout << "Trade: " << trade.signalId.strategyId << "/" << trade.signalId.signalId << "/" << trade.signalId.comment << '\n';
	}
};

int main(int argc, char** argv)
{
	if(argc < 3)
	{
		std::cerr << "Usage ./broker-client <brokerserver-endpoint> <account>" << '\n';
		return 1;
	}
#ifndef WIN32
	signal(SIGPIPE, SIG_IGN);
#endif
	auto man = std::shared_ptr<cppio::IoLineManager>(cppio::createLineManager());
	BrokerClient client(man, argv[1]);
	std::string account(argv[2]);

	auto reactor = std::make_shared<Reactor>();
	client.registerReactor(reactor);
	client.start();

	int id = 1;
	int balance = 0;
	std::string strategy = "foo_strategy";
	std::string signal = std::string("signal ") + std::to_string(rand());
	std::string comment = "my id: " + std::to_string(id);
	while(true)
	{
		boost::this_thread::sleep_for(boost::chrono::milliseconds(500));
		Order::Ptr order;
		if(balance == 0)
		{
			strategy = "foo_strategy";
			signal = std::string("signal ") + std::to_string(rand());
			comment = "my id: " + std::to_string(id);
			if((rand() % 5) == 0)
			{
				std::cout << "Submitting market order" << '\n';
				int shares = rand() % 10 + 1;
				auto op = rand() % 2 == 0 ? Order::Operation::Buy : Order::Operation::Sell;
				order = std::make_shared<Order>(id, account, "FOO", 0, shares, op, Order::OrderType::Market);
				if(op == Order::Operation::Buy)
					balance = shares;
				else
					balance = -shares;
				id++;
			}
			else if((rand() % 5) == 0)
			{
				std::cout << "Submitting limit order" << '\n';
				int shares = rand() % 10 + 1;
				auto op = rand() % 2 == 0 ? Order::Operation::Buy : Order::Operation::Sell;
				order = std::make_shared<Order>(id, account, "FOO", 1000, shares, op, Order::OrderType::Limit);
				if(op == Order::Operation::Buy)
					balance = shares;
				else
					balance = -shares;
				id++;
			}
		}
		else
		{
			if((rand() % 5) == 0)
			{
				std::cout << "Exiting from trade" << '\n';
				order = std::make_shared<Order>(id, account, "FOO", 0, balance > 0 ? balance : -balance, balance > 0 ? Order::Operation::Sell : Order::Operation::Buy, Order::OrderType::Market);
				balance = 0;
				id++;
			}
		}

		if(order)
		{
			order->setSignalId({strategy, signal, comment});
			client.submitOrder(order);
		}

	}
}

