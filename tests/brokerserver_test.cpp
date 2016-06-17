
#include "catch.hpp"

#include "broker/brokerserver.h"

#include "json/json.h"
#include "io/message.h"
#include "io/ioline.h"
#include "io/common/inproc.h"
#include "io/iolinemanager.h"

using namespace goldmine;
using namespace goldmine::io;

class TestBroker : public Broker
{
public:
	TestBroker(const std::string& acct) : account(acct)
	{
	}

	~TestBroker()
	{
	}

	void submitOrder(const Order::Ptr& order)
	{
		order->updateState(Order::State::Submitted);
		submittedOrders.push_back(order);
		for(const auto& reactor : reactors)
		{
			reactor->orderCallback(order);
		}
	}

	void cancelOrder(const Order::Ptr& order)
	{
		auto it = std::find_if(submittedOrders.begin(), submittedOrders.end(), [&](const Order::Ptr& other) { return order->localId() == other->localId(); });
		if(it != submittedOrders.end())
		{
			order->updateState(Order::State::Cancelled);
			for(const auto& reactor : reactors)
			{
				reactor->orderCallback(order);
			}
			submittedOrders.erase(it);
		}
	}

	void registerReactor(const std::shared_ptr<Reactor>& reactor)
	{
		reactors.push_back(reactor);
	}

	void unregisterReactor(const std::shared_ptr<Reactor>& reactor)
	{
	}

	Order::Ptr order(int localId)
	{
		return Order::Ptr();
	}

	std::list<std::string> accounts()
	{
		std::list<std::string> result;
		result.push_back(account);
		return result;
	}

	bool hasAccount(const std::string& acct)
	{
		return account == acct;
	}

	std::list<Position> positions()
	{
		return std::list<Position>();
	}

	std::vector<Order::Ptr> submittedOrders;
	std::vector<std::shared_ptr<Reactor>> reactors;
	std::string account;
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

static void doIdentityRequest(MessageProtocol& client)
{
	Json::Value command;
	command["command"] = "get-identity";

	sendControlMessage(command, client);

	Json::Value response;
	receiveControlMessage(response, client);
}

TEST_CASE("BrokerServer", "[broker]")
{
	auto manager = std::make_shared<IoLineManager>();
	manager->registerFactory(std::unique_ptr<InprocLineFactory>(new InprocLineFactory()));

	auto server = std::make_shared<BrokerServer>(manager, "inproc://brokerserver");
	auto broker = std::make_shared<TestBroker>("TEST_ACCOUNT");
	server->registerBroker(broker);

	server->start();

	auto clientLine = manager->createClient("inproc://brokerserver");
	int timeout = 200;
	clientLine->setOption(LineOption::ReceiveTimeout, &timeout);
	MessageProtocol client(clientLine);

	SECTION("New identity")
	{
		Json::Value command;
		command["command"] = "get-identity";

		sendControlMessage(command, client);

		Json::Value response;
		receiveControlMessage(response, client);

		REQUIRE(!response["identity"].asString().empty());
	}

	SECTION("Order creation - fail if no identity specified")
	{
		Json::Value order;
		order["id"] = 1;
		order["account"] = "TEST_ACCOUNT";
		order["security"] = "FOOBAR";
		order["type"] = "limit";
		order["price"] = 19.74;
		order["quantity"] = 2;
		order["operation"] = "buy";

		Json::Value root;
		root["order"] = order;

		sendControlMessage(root, client);

		Json::Value response;
		receiveControlMessage(response, client);

		REQUIRE(response["result"] == "error");
	}

	SECTION("Order creation - limit order")
	{
		doIdentityRequest(client);

		Json::Value order;
		order["id"] = 1;
		order["account"] = "TEST_ACCOUNT";
		order["security"] = "FOOBAR";
		order["type"] = "limit";
		order["price"] = 19.74;
		order["quantity"] = 2;
		order["operation"] = "buy";

		Json::Value root;
		root["order"] = order;

		sendControlMessage(root, client);

		Json::Value response;
		receiveControlMessage(response, client);

		REQUIRE(response["result"] == "success");

		response.clear();
		receiveControlMessage(response, client);

		REQUIRE(broker->submittedOrders.size() == 1);
		REQUIRE(broker->submittedOrders.front()->clientAssignedId() == 1);
		REQUIRE(broker->submittedOrders.front()->account() == "TEST_ACCOUNT");
		REQUIRE(broker->submittedOrders.front()->security() == "FOOBAR");
		REQUIRE(broker->submittedOrders.front()->price() == Approx(19.74));
		REQUIRE(broker->submittedOrders.front()->quantity() == 2);
		REQUIRE(broker->submittedOrders.front()->operation() == Order::Operation::Buy);
		REQUIRE(broker->submittedOrders.front()->type() == Order::OrderType::Limit);
		REQUIRE(response["order"]["new-state"] == "submitted");
	}

	SECTION("Order creation - market order")
	{
		doIdentityRequest(client);

		Json::Value order;
		order["id"] = 1;
		order["account"] = "TEST_ACCOUNT";
		order["security"] = "FOOBAR";
		order["type"] = "market";
		order["quantity"] = 2;
		order["operation"] = "sell";

		Json::Value root;
		root["order"] = order;

		sendControlMessage(root, client);

		Json::Value response;
		receiveControlMessage(response, client);

		REQUIRE(response["result"] == "success");

		response.clear();
		receiveControlMessage(response, client);

		REQUIRE(broker->submittedOrders.size() == 1);
		REQUIRE(broker->submittedOrders.front()->clientAssignedId() == 1);
		REQUIRE(broker->submittedOrders.front()->account() == "TEST_ACCOUNT");
		REQUIRE(broker->submittedOrders.front()->security() == "FOOBAR");
		REQUIRE(broker->submittedOrders.front()->quantity() == 2);
		REQUIRE(broker->submittedOrders.front()->operation() == Order::Operation::Sell);
		REQUIRE(broker->submittedOrders.front()->type() == Order::OrderType::Market);
		REQUIRE(response["order"]["new-state"] == "submitted");

		SECTION("Order creation - duplicated id")
		{
			Json::Value order;
			order["id"] = 1;
			order["account"] = "TEST_ACCOUNT";
			order["security"] = "FOOBAR";
			order["type"] = "market";
			order["quantity"] = 2;
			order["operation"] = "buy";

			Json::Value root;
			root["order"] = order;

			sendControlMessage(root, client);

			Json::Value response;
			receiveControlMessage(response, client);

			REQUIRE(response["result"] == "error");

		}
	}

	SECTION("Order creation - invalid order type")
	{
		doIdentityRequest(client);

		Json::Value order;
		order["id"] = 1;
		order["account"] = "TEST_ACCOUNT";
		order["security"] = "FOOBAR";
		order["type"] = "foo";
		order["quantity"] = 2;
		order["operation"] = "sell";

		Json::Value root;
		root["order"] = order;

		sendControlMessage(root, client);

		Json::Value response;
		receiveControlMessage(response, client);

		REQUIRE(response["result"] == "error");
	}

	SECTION("Order creation - invalid operation type")
	{
		doIdentityRequest(client);

		Json::Value order;
		order["id"] = 1;
		order["account"] = "TEST_ACCOUNT";
		order["security"] = "FOOBAR";
		order["type"] = "market";
		order["quantity"] = 2;
		order["operation"] = "foo";

		Json::Value root;
		root["order"] = order;

		sendControlMessage(root, client);

		Json::Value response;
		receiveControlMessage(response, client);

		REQUIRE(response["result"] == "error");
	}

	SECTION("Order creation - limit order without price")
	{
		doIdentityRequest(client);

		Json::Value order;
		order["id"] = 1;
		order["account"] = "TEST_ACCOUNT";
		order["security"] = "FOOBAR";
		order["type"] = "limit";
		order["quantity"] = 2;
		order["operation"] = "buy";

		Json::Value root;
		root["order"] = order;

		sendControlMessage(root, client);

		Json::Value response;
		receiveControlMessage(response, client);

		REQUIRE(response["result"] == "error");
	}

	SECTION("Order cancellation")
	{
		doIdentityRequest(client);

		{
			Json::Value order;
			order["id"] = 1;
			order["account"] = "TEST_ACCOUNT";
			order["security"] = "FOOBAR";
			order["type"] = "limit";
			order["price"] = 19.74;
			order["quantity"] = 2;
			order["operation"] = "buy";

			Json::Value root;
			root["order"] = order;

			sendControlMessage(root, client);

			Json::Value response;
			receiveControlMessage(response, client);

			REQUIRE(response["result"] == "success");

			response.clear();
			receiveControlMessage(response, client);
		}
		{
			Json::Value order;
			order["id"] = 1;
			order["account"] = "TEST_ACCOUNT";

			Json::Value root;
			root["cancel-order"] = order;

			sendControlMessage(root, client);

			Json::Value response;
			receiveControlMessage(response, client);

			REQUIRE(response["result"] == "success");

			response.clear();
			receiveControlMessage(response, client);

			REQUIRE(response["order"]["new-state"] == "cancelled");
		}
	}

	server->stop();
}

