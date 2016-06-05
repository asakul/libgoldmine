
#ifndef WIN32_SOCKET_H
#define WIN32_SOCKET_H

#include "io/ioline.h"

#include <windows.h>

namespace goldmine
{
namespace io
{

class WinSocket : public IoLine
{
public:
	WinSocket(const std::string& address);
	WinSocket(SOCKET fd, const std::string& address);
	virtual ~WinSocket();

	virtual void connect();

	virtual ssize_t read(void* buffer, size_t buflen);
	virtual ssize_t write(void* buffer, size_t buflen);
	virtual void setOption(LineOption option, void* data);

private:
	std::string m_address;
	SOCKET m_socket;
};

class WinSocketAcceptor : public IoAcceptor
{
public:
	WinSocketAcceptor(const std::string& address);
	virtual ~WinSocketAcceptor();

	virtual std::shared_ptr<IoLine> waitConnection(const std::chrono::milliseconds& timeout);

private:
	std::string m_address;
	SOCKET m_socket;
};

class WinSocketFactory : public IoLineFactory
{
public:
	WinSocketFactory();
	virtual ~WinSocketFactory();
	virtual bool supportsScheme(const std::string& scheme);
	virtual std::shared_ptr<IoLine> createClient(const std::string& address);
	virtual std::shared_ptr<IoAcceptor> createServer(const std::string& address);
};

}
}

#endif /* ifndef WIN32_SOCKET_H */
