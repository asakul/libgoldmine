
#include "catch.hpp"

#include "io/message.h"
#include "io/iolinemanager.h"
#include "io/common/inproc.h"

#include <thread>
#include <cstring>

using namespace goldmine::io;

TEST_CASE("MessageProtocol", "[io]")
{
	IoLineManager manager;
	manager.registerFactory(std::unique_ptr<InprocLineFactory>(new InprocLineFactory()));

	SECTION("Small message")
	{
		Message msg;
		msg.addFrame(Frame("\x01\x02\x03\x04", 4));
		msg.addFrame(Frame("\x05\x06", 2));
		Message recv_msg;

		auto acceptor = manager.createServer("inproc://foo");
		std::thread clientThread([&](){
				auto client = manager.createClient("inproc://foo");
				MessageProtocol proto(client);
				proto.sendMessage(msg);
				});

		std::thread serverThread([&](){
				auto server = acceptor->waitConnection(std::chrono::milliseconds(100));
				MessageProtocol proto(server);
				proto.readMessage(recv_msg);

				});

		clientThread.join();
		serverThread.join();

		REQUIRE(recv_msg.size() == 2);
		REQUIRE(recv_msg.frame(0).size() == 4);
		REQUIRE(memcmp(recv_msg.frame(0).data(), "\x01\x02\x03\x04", 4) == 0);
		REQUIRE(recv_msg.frame(1).size() == 2);
		REQUIRE(memcmp(recv_msg.frame(1).data(), "\x05\x06", 2) == 0);
	}

	SECTION("Bigger messages")
	{
		for(int i = 0; i < 100; i++)
		{
			int frames = rand() % 10;
			Message msg;
			for(int frame = 0; frame < frames; frame++)
			{
				std::vector<char> frameData;
				std::generate_n(std::back_inserter(frameData), rand() % 200, rand);
				msg.addFrame(Frame(std::move(frameData)));
			}
			Message recv_msg;

			auto acceptor = manager.createServer("inproc://foo");
			std::thread clientThread([&](){
					auto client = manager.createClient("inproc://foo");
					MessageProtocol proto(client);
					proto.sendMessage(msg);
					});

			std::thread serverThread([&](){
					auto server = acceptor->waitConnection(std::chrono::milliseconds(100));
					MessageProtocol proto(server);
					proto.readMessage(recv_msg);

					});

			clientThread.join();
			serverThread.join();

			REQUIRE(recv_msg.size() == msg.size());
			for(size_t frame = 0; frame < msg.size(); frame++)
			{
				REQUIRE(recv_msg.frame(frame) == msg.frame(frame));
			}
		}
	}

	SECTION("Big read/write with delays")
	{
		std::vector<char> buf(10 * 1024 * 1024);
		std::vector<char> recv_buf(10 * 1024 * 1024);
		std::iota(buf.begin(), buf.end(), 0);

		const int chunkSize = 1024;
		int totalChunks = buf.size() / chunkSize;

		auto acceptor = manager.createServer("inproc://foo");
		std::thread clientThread([&](){
				auto client = manager.createClient("inproc://foo");
				MessageProtocol proto(client);
				for(size_t i = 0; i < totalChunks; i++)
				{
					if((rand() % 100) == 0)
						std::this_thread::sleep_for(std::chrono::milliseconds(20));
					auto start = buf.data() + chunkSize * i;
					Message msg;
					msg.addFrame(Frame(start, chunkSize));
					proto.sendMessage(msg);
				}
			});

		std::thread serverThread([&](){
				auto server = acceptor->waitConnection(std::chrono::milliseconds(100));
				MessageProtocol proto(server);
				for(size_t i = 0; i < totalChunks; i++)
				{
					if((rand() % 100) == 0)
						std::this_thread::sleep_for(std::chrono::milliseconds(20));
					auto start = recv_buf.data() + chunkSize * i;
					Message msg;
					proto.readMessage(msg);
					auto frame = msg.frame(0);
					memcpy(start, frame.data(), frame.size());
				}
			});

		clientThread.join();
		serverThread.join();

		REQUIRE(std::equal(buf.begin(), buf.end(), recv_buf.begin()));
	}

	SECTION("Big read/write with delays and timeouts")
	{
		std::vector<char> buf(10 * 1024 * 1024);
		std::vector<char> recv_buf(10 * 1024 * 1024);
		std::iota(buf.begin(), buf.end(), 0);

		const int chunkSize = 1024;
		int totalChunks = buf.size() / chunkSize;

		auto acceptor = manager.createServer("inproc://foo");
		std::thread clientThread([&](){
				auto client = manager.createClient("inproc://foo");
				MessageProtocol proto(client);
				for(size_t i = 0; i < totalChunks; i++)
				{
					if((rand() % 100) == 0)
						std::this_thread::sleep_for(std::chrono::milliseconds(20));
					auto start = buf.data() + chunkSize * i;
					Message msg;
					msg.addFrame(Frame(start, chunkSize));
					proto.sendMessage(msg);
				}
			});

		std::thread serverThread([&](){
				auto server = acceptor->waitConnection(std::chrono::milliseconds(100));
				int timeout = 100;
				server->setOption(LineOption::ReceiveTimeout, &timeout);
				MessageProtocol proto(server);
				for(size_t i = 0; i < totalChunks; i++)
				{
					if((rand() % 100) == 0)
						std::this_thread::sleep_for(std::chrono::milliseconds(20));
					auto start = recv_buf.data() + chunkSize * i;
					Message msg;
					proto.readMessage(msg);
					auto frame = msg.frame(0);
					memcpy(start, frame.data(), frame.size());
				}
			});

		clientThread.join();
		serverThread.join();

		REQUIRE(std::equal(buf.begin(), buf.end(), recv_buf.begin()));
	}
}

