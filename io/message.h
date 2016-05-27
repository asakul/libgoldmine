
#ifndef MESSAGE_H
#define MESSAGE_H

#include <cstddef>
#include <vector>
#include <cstdint>

namespace goldmine
{
namespace io
{

class Frame
{
public:
	Frame();
	Frame(const void* data, size_t len);
	Frame(const Frame& other) = default;
	Frame(Frame&& other) = default;
	Frame& operator=(const Frame& other) = default;
	Frame& operator=(Frame&& other) = default;

	size_t size() const { return m_data.size(); }
	const void* data() const { return m_data.data(); }

private:
	std::vector<char> m_data;
};

class Message
{
public:
	Message();
	virtual ~Message();

	void addFrame(const Frame& frame);
	void addFrame(Frame&& frame);

	size_t size() const { return m_frames.size(); }
	Frame& frame(size_t index) { return m_frames[index]; }

	size_t messageSize() const;
	void writeMessage(void* buffer);

private:
	std::vector<Frame> m_frames;
};

}
}

#endif /* ifndef MESSAGE_H */
