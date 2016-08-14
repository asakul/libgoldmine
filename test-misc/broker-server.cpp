
#include "broker/brokerserver.h"
#include "goldmine/data.h"

#include "cppio/iolinemanager.h"

#include <thread>

#include <boost/thread.hpp>

#include <signal.h>
#include <cstdlib>
#include <ctime>

using namespace goldmine;

class TestBroker : public Broker
{
public:
	TestBroker()
	{
	}

	virtual ~TestBroker()
	{

	}

	void processOrders()
	{
		boost::unique_lock<boost::mutex> lock(m_mutex);
		///=if((rand() % 3) == 0)
		{
			for(const auto& orderPair : m_allOrders)
			{
				auto& order = orderPair.second;
				if(order->state() == Order::State::Unsubmitted)
				{
					order->updateState(Order::State::Submitted);
					notifyOrderStateChange(order);
					break;
				}
			}
		}
		//if((rand() % 7) == 0)
		{
			for(auto orderIt = m_allOrders.begin(); orderIt != m_allOrders.end(); ++orderIt)
			{
				if((orderIt->second->state() == Order::State::Submitted) && (orderIt->second->type() == Order::OrderType::Market))
				{
					auto& order = orderIt->second;

					Trade trade;
					trade.account = order->account();
					trade.operation = order->operation();
					trade.orderId = order->localId();
					trade.price = (double)(rand() % 10000) / 100;
					trade.quantity = order->quantity();
					trade.security = order->security();
					trade.signalId = order->signalId();
					trade.timestamp = time(0);
					trade.useconds = 0;
					m_retiredOrders.insert(std::make_pair(order->localId(), order));
					m_allOrders.erase(orderIt);
					notifyTrade(trade);
					break;
				}
			}
		}

		if((rand() % 10) == 0)
		{
			for(auto orderIt = m_allOrders.begin(); orderIt != m_allOrders.end(); ++orderIt)
			{
				if((orderIt->second->state() == Order::State::Submitted) && (orderIt->second->type() == Order::OrderType::Limit))
				{
					auto& order = orderIt->second;

					Trade trade;
					trade.account = order->account();
					trade.operation = order->operation();
					trade.orderId = order->localId();
					trade.price = order->price();
					trade.quantity = order->quantity();
					trade.security = order->security();
					trade.signalId = order->signalId();
					trade.timestamp = time(0);
					trade.useconds = 0;
					m_retiredOrders.insert(std::make_pair(order->localId(), order));
					m_allOrders.erase(orderIt);
					notifyTrade(trade);
					break;
				}
			}
		}
	}

	void notifyOrderStateChange(const Order::Ptr& order)
	{
		for(const auto& reactor : m_reactors)
		{
			reactor->orderCallback(order);
		}
	}

	void notifyTrade(const Trade& trade)
	{
		for(const auto& reactor : m_reactors)
		{
			reactor->tradeCallback(trade);
		}
	}

	virtual void submitOrder(const Order::Ptr& order) override
	{
		boost::unique_lock<boost::mutex> lock(m_mutex);
		std::cout << "Submitted order: " << order->stringRepresentation() << '\n';
		auto accountIt = m_accounts.find(order->account());
		if(accountIt == m_accounts.end())
		{
			std::cout << "Unknown account " << order->account() << ", creating" << '\n';
			auto result = m_accounts.emplace(order->account(), Account());
			if(!result.second)
			{
				order->setMessage("Invalid account");
				order->updateState(Order::State::Rejected);
				notifyOrderStateChange(order);
				return;
			}
			else
			{
				accountIt = result.first;
			}
		}

		accountIt->second.orders.insert(std::make_pair(order->clientAssignedId(), order));
		m_allOrders.insert(std::make_pair(order->localId(), order));
	}

	virtual void cancelOrder(const Order::Ptr& order) override
	{
		boost::unique_lock<boost::mutex> lock(m_mutex);
		auto accountIt = m_accounts.find(order->account());
		if(accountIt == m_accounts.end())
		{
			return;
		}
		auto& account = accountIt->second;
		auto orderIt = account.orders.find(order->clientAssignedId());
		if(orderIt == account.orders.end())
		{
			return;
		}

		order->updateState(Order::State::Cancelled);
		notifyOrderStateChange(order);
	}

	virtual void registerReactor(const std::shared_ptr<Reactor>& reactor) override
	{
		m_reactors.push_back(reactor);
	}

	virtual void unregisterReactor(const std::shared_ptr<Reactor>& reactor) override
	{
		m_reactors.remove(reactor);
	}

	virtual Order::Ptr order(int localId) override
	{
		boost::unique_lock<boost::mutex> lock(m_mutex);
		auto it = m_allOrders.find(localId);
		if(it != m_allOrders.end())
			return it->second;
		return Order::Ptr();
	}

	virtual std::list<std::string> accounts() override
	{
		std::list<std::string> result;
		for(const auto& accountPair : m_accounts)
		{
			result.push_back(accountPair.first);
		}
		return result;
	}

	virtual bool hasAccount(const std::string& account) override
	{
		return true;
	}

	virtual std::list<Position> positions() override
	{
		return std::list<Position>();
	}

private:
	using local_order_id_t = int;
	using client_assigned_order_id_t = int;
	struct Account
	{
		std::map<client_assigned_order_id_t, Order::Ptr> orders;
	};
	std::map<std::string, Account> m_accounts;
	std::map<local_order_id_t, Order::Ptr> m_allOrders;
	std::map<local_order_id_t, Order::Ptr> m_retiredOrders;
	std::list<std::shared_ptr<Reactor>> m_reactors;
	boost::mutex m_mutex;
};


int main(int argc, char** argv)
{
	if(argc < 2)
	{
		std::cerr << "Usage ./broker-server <brokerserver-endpoint>" << '\n';
		return 1;
	}
#ifndef WIN32
	signal(SIGPIPE, SIG_IGN);
#endif
	auto man = std::shared_ptr<cppio::IoLineManager>(cppio::createLineManager());
	BrokerServer server(man, argv[1]);

	auto broker = std::make_shared<TestBroker>();
	server.registerBroker(broker);
	server.start();

	while(true)
	{
		boost::this_thread::sleep_for(boost::chrono::milliseconds(1));
		broker->processOrders();
	}
}

