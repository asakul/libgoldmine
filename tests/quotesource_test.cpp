/*
 * quotesource_test.cpp
 */

#include "catch.hpp"

#include "quotesource/quotesource.h"

#include "zmq.hpp"

using namespace goldmine;

TEST_CASE("QuoteSource", "[quotesource]")
{
	zmq::context_t context;
	QuoteSource source(context, "inproc://control");
	source.start();

	zmq::socket_t socket(context, zmq::socket_type::dealer);
	socket.connect("inproc://control");
}
