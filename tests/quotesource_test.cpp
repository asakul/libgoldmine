/*
 * quotesource_test.cpp
 */

#include "catch.hpp"

#include "quotesource/quotesource.h"

#include "zmqpp/zmqpp.hpp"

using namespace goldmine;

TEST_CASE("QuoteSource", "[quotesource]")
{
	zmqpp::context context;
	QuoteSource source(context, "inproc://control");
	source.start();

	zmqpp::socket socket(context, zmqpp::socket_type::dealer);
	socket.connect("inproc://control");

	source.stop();
}
