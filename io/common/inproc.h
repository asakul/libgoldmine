
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

class DataQueue
{
public:
	DataQueue(size_t bufferSize);
	~DataQueue();

	size_t read(void* buffer, size_t buflen);
	size_t write(void* buffer, size_t buflen);

	size_t readPointer() const { return m_rdptr; }
	size_t writePointer() const { return m_wrptr; }

private:
	std::vector<char> m_data;
	size_t m_wrptr;
	size_t m_rdptr;

	std::mutex m_mutex;
	std::condition_variable m_cond;
};

class InprocLine : public IoLine
{
public:
	InprocLine();
	virtual ~InprocLine();

	virtual ssize_t read(void* buffer, size_t buflen) override;
	virtual ssize_t write(void* buffer, size_t buflen) override;
};

class InprocAcceptor : public IoAcceptor
{
public:
	InprocAcceptor();
	virtual ~InprocAcceptor();

	virtual std::shared_ptr<IoLine> waitConnection(const std::chrono::milliseconds& timeout) override;
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
