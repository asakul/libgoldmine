

#include "catch.hpp"

#include "io/message.h"

#include <cstring>

using namespace goldmine::io;

TEST_CASE("Message", "[io]")
{
	Message msg;
	msg.addFrame(Frame("\x01\x02\x03\x04", 4));

	REQUIRE(msg.size() == 1);
	REQUIRE(msg.messageSize() == 12);

	std::array<char, 12> buf;
	msg.writeMessage(buf.data());

	REQUIRE(strncmp("\x01\x00\x00\x00\x04\x00\x00\x00\x01\x02\x03\x04", buf.data(), 12) == 0);
}

