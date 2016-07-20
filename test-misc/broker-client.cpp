
#include "broker/brokerclient.h"
#include "goldmine/data.h"

#include "cppio/iolinemanager.h"

#include <thread>

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
	signal(SIGPIPE, SIG_IGN);
	auto man = std::shared_ptr<cppio::IoLineManager>(cppio::createLineManager());
	BrokerClient client(man, argv[1]);
	std::string account(argv[2]);

	auto reactor = std::make_shared<Reactor>();
	client.registerReactor(reactor);
	client.start();

	int id = 1;
	while(true)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(30));
		Order::Ptr order;
		std::string strategy = "foo_strategy";
		std::string signal = std::string("signal ") + std::to_string(rand());
		std::string comment = "my id: " + std::to_string(id);
		if((rand() % 5) == 0)
		{
			std::cout << "Submitting market order" << '\n';
			order = std::make_shared<Order>(id, account, "FOO", 0, rand() % 10 + 1, rand() % 2 == 0 ? Order::Operation::Buy : Order::Operation::Sell, Order::OrderType::Market);
			id++;
		}
		else if((rand() % 5) == 0)
		{
			std::cout << "Submitting limit order" << '\n';
			order = std::make_shared<Order>(id, account, "FOO", 1000, rand() % 10 + 1, rand() % 2 == 0 ? Order::Operation::Buy : Order::Operation::Sell, Order::OrderType::Limit);
			id++;
		}

		if(order)
		{
			order->setSignalId({strategy, signal, comment});
			client.submitOrder(order);
		}

	}
}

