
#include "catch.hpp"

#include "broker/brokerclient.h"

#include "json/json.h"
#include "io/message.h"
#include "io/ioline.h"
#include "io/common/inproc.h"
#include "io/iolinemanager.h"

#include <boost/thread.hpp>

using namespace goldmine;
using namespace goldmine::io;

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
	}

	std::vector<Order::Ptr> orders;
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
	auto manager = std::make_shared<IoLineManager>();
	manager->registerFactory(std::unique_ptr<InprocLineFactory>(new InprocLineFactory()));

	auto client = std::make_shared<BrokerClient>(manager, "inproc://brokerclient");
	auto reactor = std::make_shared<TestReactor>();
	client->registerReactor(reactor);

	auto acceptor = manager->createServer("inproc://brokerclient");

	client->start();
	auto server = acceptor->waitConnection(std::chrono::milliseconds(100));
	int timeout = 100;
	server->setOption(LineOption::ReceiveTimeout, &timeout);

	REQUIRE(server);
	MessageProtocol proto(server);

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
		}
	}

	client->stop();
}

