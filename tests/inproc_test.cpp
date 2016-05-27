
#include "catch.hpp"

#include "io/common/inproc.h"

#include <cstring>

using namespace goldmine::io;

TEST_CASE("DataQueue", "[io]")
{
	DataQueue queue(1024);

	SECTION("Simple case")
	{
		std::array<char, 256> buf;
		std::array<char, 512> recv_buf {};
		std::iota(buf.begin(), buf.end(), 0);

		queue.write(buf.data(), buf.size());

		size_t done = queue.read(recv_buf.data(), recv_buf.size());

		REQUIRE(done == 256);
		REQUIRE(std::equal(buf.begin(), buf.end(), recv_buf.begin()));
	}

	SECTION("If write would overflow, nothing is written. Rdptr is zero")
	{
		std::array<char, 256> buf;
		std::array<char, 512> recv_buf {};
		std::iota(buf.begin(), buf.end(), 0);

		size_t done;
		done = queue.write(buf.data(), buf.size());
		REQUIRE(done == 256);
		done = queue.write(buf.data(), buf.size());
		REQUIRE(done == 256);
		done = queue.write(buf.data(), buf.size());
		REQUIRE(done == 256);

		// Would overflow, because we didn't read anything
		done = queue.write(buf.data(), buf.size());
		REQUIRE(done == 0);
	}

	SECTION("Write: rollover")
	{
		std::array<char, 512> buf;
		std::array<char, 512> recv_buf {};
		std::iota(buf.begin(), buf.end(), 0);

		queue.write(buf.data(), buf.size());

		size_t done = queue.read(recv_buf.data(), recv_buf.size());
		REQUIRE(done == 512);

		queue.write(buf.data(), buf.size()); // Should rollover here

		std::array<char, 20> another_buf;
		std::iota(another_buf.begin(), another_buf.end(), 77);
		queue.write(another_buf.data(), another_buf.size()); // Write some data in the beginning of buffer

		done = queue.read(recv_buf.data(), recv_buf.size());
		REQUIRE(done == 512);
		REQUIRE(std::equal(buf.begin(), buf.end(), recv_buf.begin()));


		done = queue.read(recv_buf.data(), recv_buf.size());
		REQUIRE(done == 20);
		REQUIRE(std::equal(another_buf.begin(), another_buf.begin() + 20, recv_buf.begin()));

	}

	SECTION("Writing and reading through the end of buffer")
	{
		std::array<char, 512> buf;
		std::array<char, 512> recv_buf {};
		std::iota(buf.begin(), buf.end(), 0);

		queue.write(buf.data(), buf.size());
		size_t done = queue.read(recv_buf.data(), recv_buf.size());
		REQUIRE(done == 512);

		REQUIRE(std::equal(buf.begin(), buf.end(), recv_buf.begin()));

		queue.write(buf.data(), 300);
		done = queue.read(recv_buf.data(), 300);
		REQUIRE(done == 300);

		queue.write(buf.data(), buf.size());
		done = queue.read(recv_buf.data(), recv_buf.size());

		REQUIRE(done == 512);
		REQUIRE(std::equal(buf.begin(), buf.end(), recv_buf.begin()));
	}

	SECTION("Writing bigger than buffer itself is failure")
	{
		std::array<char, 10240> buf;
		size_t done = queue.write(buf.data(), buf.size());
		REQUIRE(done == 0);
		REQUIRE(queue.writePointer() == 0);
	}

	SECTION("Fuzzy test")
	{
		srand(0);

		for(int i = 0; i < 100; i++)
		{
			size_t size = rand() % 1024;
			std::vector<char> data(size, 0);
			std::vector<char> recv_data(size, 0);

			std::generate(data.begin(), data.end(), []() { return rand(); });

			size_t done = queue.write(data.data(), data.size());
			REQUIRE(done == size);

			done = queue.read(recv_data.data(), recv_data.size());
			REQUIRE(done == size);
			REQUIRE(std::equal(data.begin(), data.end(), recv_data.begin()));
		}
	}
}

