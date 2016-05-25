/*
 * quotesource_test.cpp
 */

#include "catch.hpp"

#include "quotesource/quotesource.h"
#include "goldmine/data.h"

#include "zmqpp/zmqpp.hpp"
#include "json/json.h"

using namespace goldmine;
using namespace zmqpp;

class ExceptionsReactor : public QuoteSource::Reactor
{
public:
	void exception(const LibGoldmineException& e)
	{
		exceptions.push_back(e);
	}

	void clientConnected(int)
	{
	}

	void clientRequestedStream(const std::string&, const std::string&)
	{
	}


	std::vector<LibGoldmineException> exceptions;
};

static void sendControlMessage(const Json::Value& root, socket& sock)
{
	Json::FastWriter writer;
	auto json = writer.write(root);

	message msg;
	msg.push_back(0, 0);
	msg << (uint32_t)goldmine::MessageType::Control;
	msg << json;

	sock.send(msg);
}

static bool receiveControlMessage(Json::Value& root, socket& sock)
{
	root.clear();
	message recvd;
	bool receiveOk = sock.receive(recvd);
	if(!receiveOk)
		return false;

	REQUIRE(recvd.size(0) == 0);

	uint32_t incomingMessageType = recvd.get<uint32_t>(1);
	REQUIRE(incomingMessageType == (int)goldmine::MessageType::Control);

	auto json = recvd.get<std::string>(2);
	Json::Reader reader;
	bool parseOk = reader.parse(json, root);
	if(!parseOk)
		return false;

	return true;
}

TEST_CASE("QuoteSource", "[quotesource]")
{
	auto exceptionsReactor = std::make_shared<ExceptionsReactor>();
	context context;
	QuoteSource source(context, "inproc://control");
	source.addReactor(exceptionsReactor);
	source.start();

	socket control(context, socket_type::dealer);
	control.connect("inproc://control");
	control.set(socket_option::receive_timeout, 50);

	SECTION("Capability request")
	{
		SECTION("Correct sequence")
		{
			Json::Value root;
			root["command"] = "request-capabilities";
			sendControlMessage(root, control);

			bool receiveOk = receiveControlMessage(root, control);
			REQUIRE(receiveOk);

			REQUIRE(root["node-type"].asString() == "quotesource");
			REQUIRE(root["protocol-version"].asInt() == 2);
		}

	}

	SECTION("Start stream request")
	{
		SECTION("Request ticks, without selectors")
		{
			Json::Value tickers(Json::arrayValue);
			tickers.append("t:RIM6");
			Json::Value root;
			root["command"] = "start-stream";
			root["tickers"] = tickers;
			sendControlMessage(root, control);

			auto sink = source.makeTickSink();
			goldmine::Tick tick;
			tick.timestamp = 12;
			tick.useconds = 0;
			tick.packet_type = (int)goldmine::PacketType::Tick;
			tick.datatype = (int)goldmine::Datatype::Price;
			tick.value = goldmine::decimal_fixed(42, 0);

			sink->incomingTick("RIM6", tick);

			message recvd;
			bool receiveOk = control.receive(recvd);
			REQUIRE(receiveOk);

			REQUIRE(recvd.parts() == 5);
			int messageType = recvd.get<uint32_t>(2);
			REQUIRE(messageType == (int)goldmine::MessageType::Data);
			std::string ticker = recvd.get<std::string>(3);
			REQUIRE(ticker == "RIM6");

			std::string rawTick = recvd.get<std::string>(4);
			const goldmine::Tick* recvdTick = reinterpret_cast<const goldmine::Tick*>(rawTick.data());

			REQUIRE(*recvdTick == tick);
		}
	}

	SECTION("Invalid packet")
	{
		SECTION("Invalid message type")
		{
			Json::Value root;
			root["command"] = "request-capabilities";
			Json::FastWriter writer;
			auto json = writer.write(root);

			message msg;
			msg.push_back(0, 0);
			msg << (uint32_t)0;
			msg << json;

			control.send(msg);
		}

		SECTION("Invalid json")
		{
			Json::Value root;
			root["command"] = "request-capabilities";
			Json::FastWriter writer;
			auto json = writer.write(root);

			json[0] = 'x';
			json[3] = '&';

			message msg;
			msg.push_back(0, 0);
			msg << (uint32_t)MessageType::Control;
			msg << json;

			control.send(msg);
		}

		Json::Value root;
		bool receiveOk = receiveControlMessage(root, control);
		REQUIRE(!receiveOk);

		REQUIRE(exceptionsReactor->exceptions.size() == 1);
	}

	source.stop();
}
