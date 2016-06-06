
#include "catch.hpp"

#include "io/iolinemanager.h"
#include "io/win32/pipes.h"

#include <numeric>
#include <thread>
#ifdef __MINGW32__
#include "mingw.thread.h"
#endif
#include <unistd.h>

using namespace goldmine::io;

static void checkIo(const std::shared_ptr<IoLineManager>& manager, const std::string& endpoint)
{
	std::array<char, 1024> buf;
	std::iota(buf.begin(), buf.end(), 0);

	auto server = manager->createServer(endpoint);

	auto client = manager->createClient(endpoint);
	REQUIRE(client);

	auto socket = server->waitConnection(std::chrono::milliseconds(100));

	int rc = client->write(buf.data(), 1024);
	REQUIRE(rc == 1024);

	std::array<char, 1024> recv_buf;
	socket->read(recv_buf.data(), 1024);

	REQUIRE(buf == recv_buf);

	std::fill(recv_buf.begin(), recv_buf.end(), 0);

	socket->write(buf.data(), 1024);

	rc = client->read(recv_buf.data(), 1024);
	REQUIRE(rc == 1024);

	REQUIRE(buf == recv_buf);
}

static void threadedCheckIo(const std::shared_ptr<IoLineManager>& manager, const std::string& endpoint)
{
	std::array<char, 1024> buf;
	std::iota(buf.begin(), buf.end(), 0);

	std::array<char, 1024> recv_buf;

	std::thread serverThread([&]() {
			auto server = manager->createServer(endpoint);
			auto socket = server->waitConnection(std::chrono::milliseconds(100));
			socket->read(recv_buf.data(), 1024);
			});


	std::thread clientThread([&]() {
			usleep(10000);
			auto client = manager->createClient(endpoint);
			REQUIRE(client);

			int rc = client->write(buf.data(), 1024);
			REQUIRE(rc == 1024);
			});


	serverThread.join();
	clientThread.join();

	REQUIRE(buf == recv_buf);
}

TEST_CASE("Win32: Named pipes", "[io]")
{
	auto manager = std::make_shared<IoLineManager>();
	manager->registerFactory(std::unique_ptr<NamedPipeLineFactory>(new NamedPipeLineFactory));

	threadedCheckIo(manager, "local://foo");
}

