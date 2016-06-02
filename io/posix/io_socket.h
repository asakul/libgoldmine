
#ifndef IO_SOCKET_H
#define IO_SOCKET_H 

#include "io/ioline.h"

namespace goldmine
{
namespace io
{

class UnixSocket : public IoLine
{
public:
	UnixSocket(const std::string& address);
	UnixSocket(int fd, const std::string& address);
	virtual ~UnixSocket();

	virtual void connect();

	virtual ssize_t read(void* buffer, size_t buflen);
	virtual ssize_t write(void* buffer, size_t buflen);
	virtual void setOption(LineOption option, void* data);

private:
	std::string m_address;
	int m_socket;
};

class UnixSocketAcceptor : public IoAcceptor
{
public:
	UnixSocketAcceptor(const std::string& address);
	virtual ~UnixSocketAcceptor();

	virtual std::shared_ptr<IoLine> waitConnection(const std::chrono::milliseconds& timeout);

private:
	std::string m_address;
	int m_socket;
};

class UnixSocketFactory : public IoLineFactory
{
public:
	virtual bool supportsScheme(const std::string& scheme);
	virtual std::shared_ptr<IoLine> createClient(const std::string& address);
	virtual std::shared_ptr<IoAcceptor> createServer(const std::string& address);
};

class TcpSocket : public IoLine
{
public:
	TcpSocket(const std::string& address);
	TcpSocket(int fd, const std::string& address);
	virtual ~TcpSocket();

	virtual void connect();

	virtual ssize_t read(void* buffer, size_t buflen);
	virtual ssize_t write(void* buffer, size_t buflen);

	virtual void setOption(LineOption option, void* data);
private:
	std::string m_address;
	int m_socket;
};

class TcpSocketAcceptor : public IoAcceptor
{
public:
	TcpSocketAcceptor(const std::string& address);
	virtual ~TcpSocketAcceptor();

	virtual std::shared_ptr<IoLine> waitConnection(const std::chrono::milliseconds& timeout);

private:
	std::string m_address;
	int m_socket;
};

class TcpSocketFactory : public IoLineFactory
{
public:
	virtual bool supportsScheme(const std::string& scheme);
	virtual std::shared_ptr<IoLine> createClient(const std::string& address);
	virtual std::shared_ptr<IoAcceptor> createServer(const std::string& address);
};
}
}

#endif /* ifndef IO_SOCKET_H */
