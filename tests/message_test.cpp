

#include "catch.hpp"

#include "io/message.h"

#include <cstring>

using namespace goldmine::io;

TEST_CASE("Message serialization", "[io]")
{
	Message msg;
	msg.addFrame(Frame("\x01\x02\x03\x04", 4));

	REQUIRE(msg.size() == 1);
	REQUIRE(msg.messageSize() == 12);

	std::array<char, 12> buf;
	msg.writeMessage(buf.data());

	REQUIRE(memcmp("\x01\x00\x00\x00\x04\x00\x00\x00\x01\x02\x03\x04", buf.data(), 12) == 0);
}

TEST_CASE("Message deserialization", "[io]")
{
	Message msg = Message::readMessage("\x02\x00\x00\x00\x04\x00\x00\x00\x01\x02\x03\x04\x02\x00\x00\x00\x55\xaa", 18);

	REQUIRE(msg.size() == 2);

	auto firstFrame = msg.frame(0);
	REQUIRE(firstFrame.size() == 4);
	REQUIRE(memcmp(firstFrame.data(), "\x01\x02\x03\x04", 4) == 0);

	auto secondFrame = msg.frame(1);
	REQUIRE(memcmp(secondFrame.data(), "\x55\xaa", 2) == 0);
}

