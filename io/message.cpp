
#include "message.h"

#include <cstring>
#include <stdexcept>

namespace goldmine
{
namespace io
{

Frame::Frame()
{
}

Frame::Frame(const void* data, size_t len) : m_data((char*)data, (char*)data + len)
{
}

Message::Message()
{
}

Message::~Message()
{
}

Message Message::readMessage(const void* buffer, size_t bufferLength)
{
	char* start = (char*)buffer;
	char* current = (char*)buffer;
	size_t frames = *((uint32_t*)current);
	current += 4;

	Message msg;

	for(size_t i = 0; i < frames; i++)
	{
		size_t currentFrameLength = *((uint32_t*)current);
		current += 4;
		if(current + currentFrameLength - start > (ssize_t)bufferLength)
			throw std::length_error("Unable to construct Message: end of buffer");

		msg.addFrame(Frame(current, currentFrameLength));
		current += currentFrameLength;
	}

	return msg;
}

void Message::addFrame(const Frame& frame)
{
	m_frames.push_back(frame);
}

void Message::addFrame(Frame&& frame)
{
	m_frames.push_back(frame);
}

size_t Message::messageSize() const
{
	size_t totalSize = 0;
	for(const auto& frame : m_frames)
	{
		totalSize += frame.size() + 4;
	}
	totalSize += 4;
	return totalSize;
}

void Message::writeMessage(void* buffer)
{
	char* b = (char*)buffer;
	*((uint32_t*)b) = m_frames.size();

	b += 4;
	for(const auto& frame : m_frames)
	{
		*((uint32_t*)b) = frame.size();
		b += 4;
		memcpy(b, frame.data(), frame.size());
		b += frame.size();
	}
}

}
}

