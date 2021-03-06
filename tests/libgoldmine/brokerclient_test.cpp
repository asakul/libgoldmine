
#include "catch.hpp"

#include "broker/brokerclient.h"

#include "json/json.h"
#include "cppio/message.h"
#include "cppio/ioline.h"
#include "cppio/iolinemanager.h"

#include <boost/thread.hpp>

using namespace goldmine;
using namespace cppio;

class TestReactor : public BrokerClient::Reactor
{
public:
	virtual ~TestReactor()
	{
	}

	virtual void orderCallback(const Order::Ptr& order) override
	{
		orders.push_back(order);
	}

	virtual void tradeCallback(const Trade& trade) override
	{
		trades.push_back(trade);
	}

	std::vector<Order::Ptr> orders;
	std::vector<Trade> trades;
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

TEST_CASE("BrokerClient", "[broker]")
{
	auto manager = std::shared_ptr<IoLineManager>(createLineManager());

	auto client = std::make_shared<BrokerClient>(manager, "inproc://brokerclient");
	auto reactor = std::make_shared<TestReactor>();
	client->registerReactor(reactor);

	auto acceptor = std::unique_ptr<IoAcceptor>(manager->createServer("inproc://brokerclient"));

	client->start();
	auto server = std::unique_ptr<IoLine>(acceptor->waitConnection(100));
	int timeout = 100;
	server->setOption(LineOption::ReceiveTimeout, &timeout);

	REQUIRE(server);
	MessageProtocol proto(server.get());

	SECTION("If no identity is set, requests identity")
	{
		Json::Value root;
		receiveControlMessage(root, proto);

		REQUIRE(root["command"] == "get-identity");

		root.clear();
		root["identity"] = "foo";
		sendControlMessage(root, proto);

		SECTION("Order submission")
		{
			{
				auto order = std::make_shared<Order>(1, "TEST_ACCOUNT", "FOOBAR", 19.74, 2, Order::Operation::Buy, Order::OrderType::Limit);

				client->submitOrder(order);
			}

			Message msg;
			proto.readMessage(msg);

			REQUIRE(msg.get<uint32_t>(0) == (int)MessageType::Control);
			Json::Reader reader;
			Json::Value root;
			bool rc = reader.parse(msg.get<std::string>(1), root);
			REQUIRE(rc);

			auto order = root["order"];
			REQUIRE(!order.isNull());
			REQUIRE(order["id"] == 1);
			REQUIRE(order["account"] == "TEST_ACCOUNT");
			REQUIRE(order["security"] == "FOOBAR");
			REQUIRE(order["price"].asDouble() == Approx(19.74));
			REQUIRE(order["quantity"] == 2);
			REQUIRE(order["operation"] == "buy");
			REQUIRE(order["type"] == "limit");
		}

		SECTION("Order cancellation")
		{
			{
				auto order = std::make_shared<Order>(1, "TEST_ACCOUNT", "FOOBAR", 19.74, 2, Order::Operation::Buy, Order::OrderType::Limit);

				client->cancelOrder(order);
			}

			Message msg;
			proto.readMessage(msg);

			REQUIRE(msg.get<uint32_t>(0) == (int)MessageType::Control);
			Json::Reader reader;
			Json::Value root;
			bool rc = reader.parse(msg.get<std::string>(1), root);
			REQUIRE(rc);

			auto order = root["cancel-order"];
			REQUIRE(!order.isNull());
			REQUIRE(order["id"] == 1);
		}

		SECTION("Order state change")
		{
			auto order = std::make_shared<Order>(1, "TEST_ACCOUNT", "FOOBAR", 19.74, 2, Order::Operation::Buy, Order::OrderType::Limit);
			client->submitOrder(order);

			Message msg;
			proto.readMessage(msg);
			REQUIRE(msg.get<uint32_t>(0) == (int)MessageType::Control);

			Json::Value orderJson;
			orderJson["id"] = 1;
			orderJson["new-state"] = "submitted";
			Json::Value root;
			root["order"] = orderJson;
			sendControlMessage(root, proto);

			boost::this_thread::sleep_for(boost::chrono::milliseconds(10));

			REQUIRE(reactor->orders.front()->clientAssignedId() == 1);
			REQUIRE(reactor->orders.front()->account() == "TEST_ACCOUNT");
			REQUIRE(reactor->orders.front()->security() == "FOOBAR");

			SECTION("Trade notification")
			{
				Json::Value tradeJson;
				tradeJson["order-id"] = 1;
				tradeJson["price"] = 19.74;
				tradeJson["quantity"] = 2;
				tradeJson["operation"] = "buy";
				tradeJson["account"] = "TEST_ACCOUNT";
				tradeJson["security"] = "FOOBAR";
				tradeJson["execution-time"] = "1970-01-01 00:00:00.000";
				Json::Value root;
				root["trade"] = tradeJson;
				sendControlMessage(root, proto);

				boost::this_thread::sleep_for(boost::chrono::milliseconds(10));

				auto trade = reactor->trades.front();

				REQUIRE(trade.orderId == 1);
				REQUIRE(trade.price == Approx(19.74));
				REQUIRE(trade.quantity == 2);
				REQUIRE(trade.operation == Order::Operation::Buy);
				REQUIRE(trade.account == "TEST_ACCOUNT");
				REQUIRE(trade.security == "FOOBAR");
				REQUIRE(trade.timestamp == 0);
				REQUIRE(trade.useconds == 0);
			}

			SECTION("Trade notification - non-trivial time")
			{
				Json::Value tradeJson;
				tradeJson["order-id"] = 1;
				tradeJson["price"] = 19.74;
				tradeJson["quantity"] = 2;
				tradeJson["operation"] = "buy";
				tradeJson["account"] = "TEST_ACCOUNT";
				tradeJson["security"] = "FOOBAR";
				tradeJson["execution-time"] = "1970-01-01 00:00:10.009";
				Json::Value root;
				root["trade"] = tradeJson;
				sendControlMessage(root, proto);

				boost::this_thread::sleep_for(boost::chrono::milliseconds(10));

				auto trade = reactor->trades.front();

				REQUIRE(trade.orderId == 1);
				REQUIRE(trade.price == Approx(19.74));
				REQUIRE(trade.quantity == 2);
				REQUIRE(trade.operation == Order::Operation::Buy);
				REQUIRE(trade.account == "TEST_ACCOUNT");
				REQUIRE(trade.security == "FOOBAR");
				REQUIRE(trade.timestamp == 10);
				REQUIRE(trade.useconds == 9000);
			}
		}
	}

	client->stop();
}

