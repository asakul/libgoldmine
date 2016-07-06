/*
 * quotesource_test.cpp
 */

#include "catch.hpp"

#include "quotesource/quotesource.h"
#include "goldmine/data.h"

#include "json/json.h"
#include "cppio/message.h"
#include "cppio/ioline.h"
#include "cppio/iolinemanager.h"

using namespace goldmine;
using namespace cppio;

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

	void clientRequestedStream(const std::string& identity, const std::string& streamId)
	{
		streamRequests.push_back(std::make_tuple(identity, streamId));
	}

	std::vector<LibGoldmineException> exceptions;
	std::vector<std::tuple<std::string, std::string>> streamRequests;
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

TEST_CASE("QuoteSource", "[quotesource]")
{
	auto manager = std::shared_ptr<IoLineManager>(createLineManager());

	auto exceptionsReactor = std::make_shared<ExceptionsReactor>();
	QuoteSource source(manager, "inproc://control-quotesource");
	source.addReactor(exceptionsReactor);
	source.start();

	auto control = std::shared_ptr<IoLine>(manager->createClient("inproc://control-quotesource"));
	int timeout = 100;
	control->setOption(LineOption::ReceiveTimeout, &timeout);
	MessageProtocol controlProto(control.get());

	SECTION("Capability request")
	{
		Json::Value root;
		root["command"] = "request-capabilities";
		sendControlMessage(root, controlProto);

		bool receiveOk = receiveControlMessage(root, controlProto);
		REQUIRE(receiveOk);

		REQUIRE(root["node-type"].asString() == "quotesource");
		REQUIRE(root["protocol-version"].asInt() == 2);
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
			sendControlMessage(root, controlProto);

			Message okMessage;
			controlProto.readMessage(okMessage);

			goldmine::Tick tick;
			tick.timestamp = 12;
			tick.useconds = 0;
			tick.packet_type = (int)goldmine::PacketType::Tick;
			tick.datatype = (int)goldmine::Datatype::Price;
			tick.value = goldmine::decimal_fixed(42, 0);

			source.incomingTick("RIM6", tick);

			Message recvd;
			controlProto.readMessage(recvd);

			REQUIRE(recvd.size() == 3);
			int messageType = recvd.get<uint32_t>(0);
			REQUIRE(messageType == (int)goldmine::MessageType::Data);
			std::string ticker = recvd.get<std::string>(1);
			REQUIRE(ticker == "RIM6");

			std::string rawTick = recvd.get<std::string>(2);
			const goldmine::Tick* recvdTick = reinterpret_cast<const goldmine::Tick*>(rawTick.data());

			REQUIRE(*recvdTick == tick);

			SECTION("Reactor notified")
			{
				auto streamRequest = exceptionsReactor->streamRequests.front();
				REQUIRE(std::get<1>(streamRequest) == "t:RIM6");
			}
		}

		SECTION("Request ticks, manual mode, without next tick message - timeout")
		{
			Json::Value tickers(Json::arrayValue);
			tickers.append("t:RIM6");
			Json::Value root;
			root["command"] = "start-stream";
			root["manual-mode"] = true;
			root["tickers"] = tickers;
			sendControlMessage(root, controlProto);

			Message okMessage;
			controlProto.readMessage(okMessage);

			goldmine::Tick tick;
			tick.timestamp = 12;
			tick.useconds = 0;
			tick.packet_type = (int)goldmine::PacketType::Tick;
			tick.datatype = (int)goldmine::Datatype::Price;
			tick.value = goldmine::decimal_fixed(42, 0);

			source.incomingTick("RIM6", tick);

			Message recvd;
			ssize_t rc = controlProto.readMessage(recvd);
			REQUIRE(rc == eTimeout);
		}

		SECTION("Request ticks, manual mode")
		{
			Json::Value tickers(Json::arrayValue);
			tickers.append("t:RIM6");
			Json::Value root;
			root["command"] = "start-stream";
			root["manual-mode"] = true;
			root["tickers"] = tickers;
			sendControlMessage(root, controlProto);

			Message okMessage;
			controlProto.readMessage(okMessage);

			Message nextTickMessage;
			nextTickMessage << (uint32_t)goldmine::MessageType::Service;
			nextTickMessage << (uint32_t)goldmine::ServiceDataType::NextTick;
			controlProto.sendMessage(nextTickMessage);

			goldmine::Tick tick;
			tick.timestamp = 12;
			tick.useconds = 0;
			tick.packet_type = (int)goldmine::PacketType::Tick;
			tick.datatype = (int)goldmine::Datatype::Price;
			tick.value = goldmine::decimal_fixed(42, 0);

			source.incomingTick("RIM6", tick);

			Message recvd;
			controlProto.readMessage(recvd);

			REQUIRE(recvd.size() == 3);
			int messageType = recvd.get<uint32_t>(0);
			REQUIRE(messageType == (int)goldmine::MessageType::Data);
			std::string ticker = recvd.get<std::string>(1);
			REQUIRE(ticker == "RIM6");

			std::string rawTick = recvd.get<std::string>(2);
			const goldmine::Tick* recvdTick = reinterpret_cast<const goldmine::Tick*>(rawTick.data());

			REQUIRE(*recvdTick == tick);
		}

		SECTION("Stream request - all tickers")
		{
			Json::Value tickers(Json::arrayValue);
			tickers.append("t:*");
			Json::Value root;
			root["command"] = "start-stream";
			root["tickers"] = tickers;
			sendControlMessage(root, controlProto);

			Message okMessage;
			controlProto.readMessage(okMessage);

			goldmine::Tick tick;
			tick.timestamp = 12;
			tick.useconds = 0;
			tick.packet_type = (int)goldmine::PacketType::Tick;
			tick.datatype = (int)goldmine::Datatype::Price;
			tick.value = goldmine::decimal_fixed(42, 0);

			std::vector<std::string> testtickers {"FOO", "BAR", "ALPHA"};
			for(const auto& ticker : testtickers)
			{
				source.incomingTick(ticker, tick);

				Message recvd;
				controlProto.readMessage(recvd);

				REQUIRE(recvd.size() == 3);
				int messageType = recvd.get<uint32_t>(0);
				REQUIRE(messageType == (int)goldmine::MessageType::Data);
				std::string recvdTicker = recvd.get<std::string>(1);
				REQUIRE(recvdTicker == ticker);

				std::string rawTick = recvd.get<std::string>(2);
				const goldmine::Tick* recvdTick = reinterpret_cast<const goldmine::Tick*>(rawTick.data());

				REQUIRE(*recvdTick == tick);
			}

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

			Message msg;
			msg << (uint32_t)0;
			msg << json;

			controlProto.sendMessage(msg);
		}

		SECTION("Invalid json")
		{
			Json::Value root;
			root["command"] = "request-capabilities";
			Json::FastWriter writer;
			auto json = writer.write(root);

			json[0] = 'x';
			json[3] = '&';

			Message msg;
			msg << (uint32_t)MessageType::Control;
			msg << json;

			controlProto.sendMessage(msg);
		}

		Json::Value root;
		bool receiveOk = receiveControlMessage(root, controlProto);
		REQUIRE(receiveOk);

		REQUIRE(root["result"] == "error");

		REQUIRE(exceptionsReactor->exceptions.size() == 1);
	}

	source.stop();
}
