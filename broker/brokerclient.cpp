#include "brokerclient.h"

#include "goldmine/data.h"

#include "cppio/ioline.h"
#include "cppio/message.h"

#include "goldmine/exceptions.h"

#include "json/json.h"

#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace goldmine
{

using namespace boost::posix_time;
using namespace boost::gregorian;

std::string serializeOrderType(Order::OrderType t)
{
	switch(t)
	{
	case Order::OrderType::Limit:
		return "limit";
	case Order::OrderType::Market:
		return "market";
	default:
		return "unknown";
	}
}

std::string serializeOperation(Order::Operation op)
{
	switch(op)
	{
	case Order::Operation::Buy:
		return "buy";
	case Order::Operation::Sell:
		return "sell";
	default:
		return "unknown";
	}
}

struct BrokerClient::Impl
{
	Impl(const std::shared_ptr<cppio::IoLineManager> man, const std::string& addr) : manager(man),
		address(addr),
		run(false)
	{
	}

	void registerReactor(const Reactor::Ptr& reactor)
	{
		reactors.push_back(reactor);
	}

	void unregisterReactor(const Reactor::Ptr& reactor)
	{
	}

	void start()
	{
		eventThread = boost::thread(std::bind(&Impl::eventLoop, this));
	}

	void stop()
	{
		run = false;
		if(eventThread.joinable())
			eventThread.join();
	}

	void submitOrder(const Order::Ptr& order)
	{
		Json::Value root;
		Json::Value ord;
		ord["id"] = order->clientAssignedId();
		ord["account"] = order->account();
		ord["security"] = order->security();
		ord["type"] = serializeOrderType(order->type());
		ord["operation"] = serializeOperation(order->operation());
		ord["quantity"] = order->quantity();
		ord["price"] = order->price();
		root["order"] = ord;

		Json::FastWriter writer;
		cppio::Message msg;
		msg << (uint32_t)MessageType::Control;
		msg << writer.write(root);

		cppio::MessageProtocol proto(line);
		proto.sendMessage(msg);

		orders.push_back(order);
	}

	void cancelOrder(const Order::Ptr& order)
	{
		Json::Value root;
		Json::Value ord;
		ord["id"] = order->clientAssignedId();
		ord["account"] = order->account();
		root["cancel-order"] = ord;

		Json::FastWriter writer;
		cppio::Message msg;
		msg << (uint32_t)MessageType::Control;
		msg << writer.write(root);

		cppio::MessageProtocol proto(line);
		proto.sendMessage(msg);
	}

	void setIdentity(const std::string& identity)
	{
		id = identity;
	}

	std::string identity() const
	{
		return id;
	}

	void eventLoop()
	{
		run = true;
		while(run)
		{
			line = manager->createClient(address);
			if(line)
			{
				int timeout = 100;
				line->setOption(cppio::LineOption::ReceiveTimeout, &timeout);
				cppio::MessageProtocol proto(line);

				if(id.empty())
				{
					{
						Json::Value request;
						request["command"] = "get-identity";

						Json::FastWriter writer;
						cppio::Message msg;
						msg << (uint32_t)MessageType::Control;
						msg << writer.write(request);

						proto.sendMessage(msg);
					}

					{
						cppio::Message msg;
						proto.readMessage(msg);

						Json::Reader reader;
						auto json = msg.get<std::string>(1);
						Json::Value root;
						reader.parse(json, root);

						id = root["identity"].asString();
					}
				}

				while(run)
				{
					try
					{
						cppio::Message inMessage;
						proto.readMessage(inMessage);

						handleMessage(inMessage);
					}
					catch(const cppio::TimeoutException& e)
					{
						// Ignore timeout
					}
				}
			}
			else
			{
				boost::this_thread::sleep_for(boost::chrono::seconds(5));
			}
		}
	}

	void handleMessage(const cppio::Message& msg)
	{
		auto json = msg.get<std::string>(1);
		Json::Reader reader;
		Json::Value root;
		reader.parse(json, root);
		if(!root["order"].isNull())
		{
			int id = root["order"]["id"].asInt();

			auto it = std::find_if(orders.begin(), orders.end(), [&](const Order::Ptr& order) { return order->clientAssignedId() == id; } );
			if(it != orders.end())
			{
				for(const auto& reactor : reactors)
				{
					reactor->orderCallback(*it);
				}
			}
		}
		else if(!root["trade"].isNull())
		{
			auto trade = deserializeTrade(root["trade"]);
			for(const auto& reactor : reactors)
			{
				reactor->tradeCallback(trade);
			}
		}
	}

	Trade deserializeTrade(const Json::Value& json)
	{
		Trade trade;
		trade.orderId = json["order-id"].asInt();
		trade.price = json["price"].asDouble();
		trade.quantity = json["quantity"].asInt();
		auto opString = json["operation"].asString();
		if(opString == "buy")
			trade.operation = Order::Operation::Buy;
		else if(opString == "sell")
			trade.operation = Order::Operation::Sell;
		else
			BOOST_THROW_EXCEPTION(ParameterError() << errinfo_str("Invalid operation specified: " + opString));

		trade.account = json["account"].asString();
		trade.security = json["security"].asString();

		int year, month, day, hour, minute, second, msec;

		sscanf(json["execution-time"].asString().c_str(), "%d-%d-%d %d:%d:%d.%d",
				&year, &month, &day,
				&hour, &minute, &second, &msec);

		ptime t(date(year, month, day), time_duration(hour, minute, second));

		trade.timestamp = (t - ptime(date(1970, 1, 1), time_duration(0, 0, 0, 0))).total_seconds();
		trade.useconds = msec * 1000;

		return trade;
	}

	std::string id;
	std::shared_ptr<cppio::IoLineManager> manager;
	std::string address;
	bool run;
	boost::thread eventThread;
	std::shared_ptr<cppio::IoLine> line;
	std::vector<Reactor::Ptr> reactors;
	std::vector<Order::Ptr> orders;
};

BrokerClient::BrokerClient(const std::shared_ptr<cppio::IoLineManager>& manager, const std::string& address) :
	m_impl(new Impl(manager, address))
{
}

BrokerClient::~BrokerClient()
{
}

void BrokerClient::registerReactor(const Reactor::Ptr& reactor)
{
	m_impl->registerReactor(reactor);
}

void BrokerClient::unregisterReactor(const Reactor::Ptr& reactor)
{
	m_impl->unregisterReactor(reactor);
}

void BrokerClient::start()
{
	m_impl->start();
}

void BrokerClient::stop()
{
	m_impl->stop();
}

void BrokerClient::submitOrder(const Order::Ptr& order)
{
	m_impl->submitOrder(order);
}

void BrokerClient::cancelOrder(const Order::Ptr& order)
{
	m_impl->cancelOrder(order);
}

void BrokerClient::setIdentity(const std::string& id)
{
	m_impl->setIdentity(id);
}

std::string BrokerClient::identity() const
{
	return m_impl->identity();
}

}

