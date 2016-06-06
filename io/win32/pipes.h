
#ifndef WIN32_PIPES_H
#define WIN32_PIPES_H

#include "io/ioline.h"

#include <atomic>
#include <windows.h>

namespace goldmine
{
namespace io
{

class NamedPipeLine : public IoLine
{
public:
	NamedPipeLine(const std::string& address);
	NamedPipeLine(HANDLE fd, const std::string& address);
	virtual ~NamedPipeLine();

	virtual ssize_t read(void* buffer, size_t buflen);
	virtual ssize_t write(void* buffer, size_t buflen);
	virtual void setOption(LineOption option, void* data);

private:
	std::string m_address;
	HANDLE m_pipe;
};

class NamedPipeAcceptor : public IoAcceptor
{
public:
	NamedPipeAcceptor(const std::string& address);
	virtual ~NamedPipeAcceptor();

	virtual std::shared_ptr<IoLine> waitConnection(const std::chrono::milliseconds& timeout);

private:
	std::string m_address;
	HANDLE m_pipe;

	HANDLE m_waitingPipe;
	std::atomic_int m_counter;
};

class NamedPipeLineFactory : public IoLineFactory
{
public:
	NamedPipeLineFactory();
	virtual ~NamedPipeLineFactory();
	virtual bool supportsScheme(const std::string& scheme);
	virtual std::shared_ptr<IoLine> createClient(const std::string& address);
	virtual std::shared_ptr<IoAcceptor> createServer(const std::string& address);
};

}
}
#endif /* ifndef WIN32_PIPES_H */
