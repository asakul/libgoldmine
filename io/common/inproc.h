
#ifndef INPROC_H
#define INPROC_H

#include "io/ioline.h"

#include <cstddef>

#include <vector>
#include <mutex>
#include <condition_variable>

namespace goldmine
{
namespace io
{

class RingBuffer
{
public:
	RingBuffer(size_t bufferSize);
	~RingBuffer();

	size_t read(void* buffer, size_t buflen);
	size_t write(void* buffer, size_t buflen);

	size_t readPointer() const { return m_rdptr; }
	size_t writePointer() const { return m_wrptr; }

	size_t availableReadSize() const;
	size_t availableWriteSize() const;

	size_t size() const { return m_data.size(); }
private:
	std::vector<char> m_data;
	size_t m_wrptr;
	size_t m_rdptr;
};

class DataQueue
{
public:
	DataQueue(size_t bufferSize);
	~DataQueue();

	size_t read(void* buffer, size_t buflen);
	size_t write(void* buffer, size_t buflen);

	size_t readWithTimeout(void* buffer, size_t buflen, const std::chrono::milliseconds& timeout);

	size_t readPointer() const { return m_buffer.readPointer(); }
	size_t writePointer() const { return m_buffer.writePointer(); }

	size_t availableReadSize() const;
	size_t availableWriteSize() const;

private:
	RingBuffer m_buffer;

	std::mutex m_mutex;
	std::condition_variable m_readCondition;
	std::condition_variable m_writeCondition;
};

class InprocLine : public IoLine
{
public:
	InprocLine(const std::shared_ptr<InprocLine>& other);
	InprocLine(const std::string& address);
	virtual ~InprocLine();

	virtual ssize_t read(void* buffer, size_t buflen) override;
	virtual ssize_t write(void* buffer, size_t buflen) override;
	virtual void setOption(LineOption option, void* data);

	std::string address() const { return m_address; }

	void waitForConnection();

private:
	std::string m_address;
	std::mutex m_mutex;
	std::condition_variable m_condition;

	std::shared_ptr<DataQueue> m_in;
	std::shared_ptr<DataQueue> m_out;

	int m_readTimeout;
};

class InprocAcceptor : public IoAcceptor
{
public:
	InprocAcceptor(const std::string& address);
	virtual ~InprocAcceptor();

	virtual std::shared_ptr<IoLine> waitConnection(const std::chrono::milliseconds& timeout) override;

	std::string address() const { return m_address; }


private:
	std::string m_address;
};

class InprocLineFactory : public IoLineFactory
{
public:
	InprocLineFactory();
	virtual ~InprocLineFactory();

	virtual bool supportsScheme(const std::string& scheme) override;
	virtual std::shared_ptr<IoLine> createClient(const std::string& address) override;
	virtual std::shared_ptr<IoAcceptor> createServer(const std::string& address) override;
};

}
}

#endif /* ifndef INPROC_H */
