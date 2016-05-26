
#include "catch.hpp"

#include "io/iolinemanager.h"
#include "io/posix/io_socket.h"

#include <thread>
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

TEST_CASE("Unix socket", "[io]")
{
	auto manager = std::make_shared<IoLineManager>();
	manager->registerFactory(std::make_unique<UnixSocketFactory>());

	checkIo(manager, "local:///tmp/foo");
}

TEST_CASE("TCP socket", "[io]")
{
	auto manager = std::make_shared<IoLineManager>();
	manager->registerFactory(std::make_unique<TcpSocketFactory>());

	threadedCheckIo(manager, "tcp://127.0.0.1:6000");
}

