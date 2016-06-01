
#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstddef>
#include <vector>
#include <cstdint>
#include <memory>

namespace goldmine
{
namespace io
{

class Frame
{
public:
	Frame();
	Frame(const void* data, size_t len);
	Frame(std::vector<char>&& data);
	Frame(const Frame& other) = default;
	Frame(Frame&& other) = default;
	Frame& operator=(const Frame& other) = default;
	Frame& operator=(Frame&& other) = default;

	size_t size() const { return m_data.size(); }
	const void* data() const { return m_data.data(); }

	inline bool operator==(const Frame& other) const
	{
		return m_data == other.m_data;
	}

	static Frame fromValue(uint8_t value);
	static Frame fromValue(uint16_t value);
	static Frame fromValue(uint32_t value);
	static Frame fromValue(const std::string& value);

private:
	std::vector<char> m_data;
};

class Message
{
public:
	Message();
	Message(const Message& other) = default;
	Message(Message&& other) = default;
	Message& operator=(const Message& other) = default;
	Message& operator=(Message&& other) = default;
	virtual ~Message();

	static Message readMessage(const void* buffer, size_t bufferLength);

	void addFrame(const Frame& frame);
	void addFrame(Frame&& frame);

	size_t size() const { return m_frames.size(); }
	Frame& frame(size_t index) { return m_frames[index]; }
	const Frame& frame(size_t index) const { return m_frames[index]; }

	size_t messageSize() const;
	void writeMessage(void* buffer) const;

	template <typename T>
	Message& operator<<(const T& value)
	{
		addFrame(Frame::fromValue(value));
		return *this;
	}

	void get(uint8_t& value, size_t frameNumber) const;
	void get(uint16_t& value, size_t frameNumber) const;
	void get(uint32_t& value, size_t frameNumber) const;
	void get(std::string& value, size_t frameNumber) const;

	template <typename T>
	T get(size_t frameNumber) const
	{
		T t;
		get(t, frameNumber);
		return t;
	}

private:
	std::vector<Frame> m_frames;
};

class IoLine;
class MessageProtocol
{
public:
	MessageProtocol(const std::shared_ptr<IoLine>& line);
	MessageProtocol(MessageProtocol&& other);
	virtual ~MessageProtocol();

	void readMessage(Message& m);
	void sendMessage(const Message& m);

	IoLine* getLine() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

}
}

#endif /* ifndef MESSAGE_H */
