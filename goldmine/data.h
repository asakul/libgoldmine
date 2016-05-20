
#ifndef GOLDMINE_DATA_H_
#define GOLDMINE_DATA_H_

#include <cstdint>
#include <cmath>
#include <utility>

namespace goldmine
{
	enum class MessageType
	{
		Control = 0x01,
		Data = 0x02,
		Service = 0x03,
		Event = 0x04
	};

	enum class PacketType
	{
		Tick = 0x01,
		Summary = 0x02
	};

	enum class EventId
	{
		StreamEnd = 0x01
	};

	enum class Datatype
	{
		Price = 0x01,
		OpenInterest = 0x03,
		BestBid = 0x04,
		BestOffer = 0x05,
		Depth = 0x06,
		TheoryPrice = 0x07,
		Volatility = 0x08,
		TotalSupply = 0x09,
		TotalDemand = 0x0a
	};

#pragma pack(push, 1)
	struct decimal_fixed
	{
		int64_t value;
		int32_t fractional; // 1e-9 parts

		decimal_fixed(int64_t int_part, int32_t nano) : value(int_part), fractional(nano)
		{
		}

		decimal_fixed(double v) : value(::floor(v)), fractional((v - floor(v)) * 1e9)
		{

		}

		decimal_fixed(const decimal_fixed& other) = default;
		decimal_fixed& operator=(const decimal_fixed& other) = default;
		decimal_fixed(decimal_fixed&& other) = default;
		decimal_fixed& operator=(decimal_fixed&& other) = default;

		double toDouble() const
		{
			return (double)value + (double)fractional / 1e9;
		}

		bool operator==(const decimal_fixed& other) const
		{
			return (value == other.value) && (fractional == other.fractional);
		}

		bool operator<(const decimal_fixed& other) const
		{
			if(value < other.value)
				return true;
			else if(value == other.value)
				return fractional < other.fractional;
			else
				return false;
		}

		bool operator<=(const decimal_fixed& other) const
		{
			return (*this < other) || (*this == other);
		}

		bool operator>=(const decimal_fixed& other) const
		{
			return !(*this < other);
		}

		bool operator>(const decimal_fixed& other) const
		{
			return !(*this <= other);
		}
	} __attribute__((packed,aligned(1)));

	struct Tick
	{
		Tick() : packet_type((int)PacketType::Tick), timestamp(0), useconds(0), datatype(0), value(0), volume(0) {}
		uint32_t packet_type; // = 0x01
		uint64_t timestamp;
		uint32_t useconds;
		uint32_t datatype;
		decimal_fixed value;
		int32_t volume;
	} __attribute__((packed));

	inline bool operator==(const Tick& t1, const Tick& t2)
	{
		return (t1.packet_type == t2.packet_type) && (t1.timestamp == t2.timestamp) &&
			(t1.useconds == t2.useconds) && (t1.datatype == t2.datatype) && (t1.value == t2.value) &&
			(t1.volume == t2.volume);
	}

	struct Summary
	{
		uint32_t packet_type; // = 0x02
		uint64_t timestamp;
		uint32_t useconds;
		uint32_t datatype;
		decimal_fixed open;
		decimal_fixed high;
		decimal_fixed low;
		decimal_fixed close;
		int32_t volume;
		uint32_t summary_period_seconds;
	} __attribute__((packed));

	struct Event
	{
		uint32_t packet_type; // = 0x03
		uint32_t event_id;
	} __attribute__((packed));

#pragma pack(pop)
}

#endif /* GOLDMINE_DATA_H_ */
