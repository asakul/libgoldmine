#include "io_socket.h"

#include <cstdlib>
#include <cstring>

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

namespace goldmine
{
namespace io
{

UnixSocket::UnixSocket(const std::string& address) : m_address(address)
{
	m_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	if(m_socket < 0)
		throw IoException(std::string("Unable to create socket: " + std::to_string(m_socket)));
}

UnixSocket::UnixSocket(int fd, const std::string& address)
{
	m_socket = fd;
	m_address = address;
}

UnixSocket::~UnixSocket()
{
	close(m_socket);
	unlink(m_address.c_str());
}

void UnixSocket::connect()
{
	sockaddr serverName;
	serverName.sa_family = AF_UNIX;
	strncpy(serverName.sa_data, m_address.c_str(), sizeof(serverName.sa_data));

	int rc = ::connect(m_socket, &serverName, strlen(serverName.sa_data) + sizeof(serverName.sa_family));
	if(rc < 0)
		throw IoException(std::string("Unable to bind socket: " + std::to_string(rc)));
}


ssize_t UnixSocket::read(void* buffer, size_t buflen)
{
	ssize_t rc = ::read(m_socket, buffer, buflen);
	if(rc < 0)
	{
		if((errno == ECONNRESET) || (errno == ENOTCONN))
			throw ConnectionLost("");
		return 0;
	}
	else if(rc == 0)
	{
		if(errno != ETIMEDOUT)
			throw ConnectionLost("");
	}
	return rc;
}

ssize_t UnixSocket::write(void* buffer, size_t buflen)
{
	ssize_t rc = ::write(m_socket, buffer, buflen);
	return rc;
}

void UnixSocket::setOption(LineOption option, void* data)
{
	switch(option)
	{
		case LineOption::ReceiveTimeout:
			{
				int msecs = *(int*)data;
				int secs = msecs / 1000;
				int restMsecs = msecs - secs * 1000;
				struct timeval timeout;
				timeout.tv_sec = secs;
				timeout.tv_usec = restMsecs * 1000;

				setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout,
							sizeof(timeout));
			}
			break;

		case LineOption::SendTimeout:
			{
				int msecs = *(int*)data;
				int secs = msecs / 1000;
				int restMsecs = msecs - secs * 1000;
				struct timeval timeout;
				timeout.tv_sec = secs;
				timeout.tv_usec = restMsecs * 1000;

				setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout,
							sizeof(timeout));
			}
			break;
		default:
			throw UnsupportedOption("");
	}
}

UnixSocketAcceptor::UnixSocketAcceptor(const std::string& address) : m_address(address)
{
	m_socket = socket(AF_UNIX, SOCK_STREAM, 0);
	if(m_socket < 0)
		throw IoException(std::string("Unable to create socket: " + std::to_string(m_socket)));

	sockaddr serverName;
	serverName.sa_family = AF_UNIX;
	strncpy(serverName.sa_data, m_address.c_str(), sizeof(serverName.sa_data));

	int rc = bind(m_socket, &serverName, strlen(serverName.sa_data) + sizeof(serverName.sa_family));
	if(rc < 0)
		throw IoException(std::string("Unable to bind socket: " + std::to_string(rc)));

	rc = listen(m_socket, 10);
	if(rc < 0)
	{
		close(m_socket);
		unlink(m_address.c_str());
		throw IoException(std::string("Unable to listen socket: " + std::to_string(rc)));
	}
}

UnixSocketAcceptor::~UnixSocketAcceptor()
{
	close(m_socket);
	unlink(m_address.c_str());
}

std::shared_ptr<IoLine> UnixSocketAcceptor::waitConnection(const std::chrono::milliseconds& timeout)
{
	sockaddr addr;
	socklen_t clen = sizeof(addr);
	int newsock = accept(m_socket, &addr, &clen);
	if(newsock > 0)
	{
		return std::make_shared<UnixSocket>(newsock, "");
	}
	return std::shared_ptr<IoLine>();
}

bool UnixSocketFactory::supportsScheme(const std::string& scheme)
{
	return scheme == "local";
}

std::shared_ptr<IoLine> UnixSocketFactory::createClient(const std::string& address)
{
	auto socket = std::make_shared<UnixSocket>(address);
	if(socket)
		socket->connect();
	return socket;
}

std::shared_ptr<IoAcceptor> UnixSocketFactory::createServer(const std::string& address)
{
	return std::make_shared<UnixSocketAcceptor>(address);
}

///////

TcpSocket::TcpSocket(const std::string& address) : m_address(address)
{
	m_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(m_socket < 0)
		throw IoException(std::string("Unable to create socket: " + std::to_string(m_socket)));
}

TcpSocket::TcpSocket(int fd, const std::string& address)
{
	m_socket = fd;
	m_address = address;
}

TcpSocket::~TcpSocket()
{
	close(m_socket);
}

void TcpSocket::connect()
{
	auto semicolon = m_address.find(':');
	auto host = m_address.substr(0, semicolon);
	auto port = atoi(m_address.substr(semicolon + 1).c_str());
	sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(host.c_str());
	addr.sin_port = htons(port);
	
	int rc = ::connect(m_socket, (sockaddr*)&addr, sizeof(addr));
	if(rc < 0)
		throw IoException(std::string("Unable to connect socket: " + std::to_string(rc)));
}


ssize_t TcpSocket::read(void* buffer, size_t buflen)
{
	ssize_t rc = ::read(m_socket, buffer, buflen);
	if(rc < 0)
	{
		if((errno == ECONNRESET) || (errno == ENOTCONN))
			throw ConnectionLost("");
		return 0;
	}
	else if(rc == 0)
	{
		if(errno != ETIMEDOUT)
			throw ConnectionLost("");
	}
	return rc;
}

ssize_t TcpSocket::write(void* buffer, size_t buflen)
{
	ssize_t rc = ::write(m_socket, buffer, buflen);
	return rc;
}

void TcpSocket::setOption(LineOption option, void* data)
{
	switch(option)
	{
		case LineOption::ReceiveTimeout:
			{
				int msecs = *(int*)data;
				int secs = msecs % 1000;
				int restMsecs = msecs - secs * 1000;
				struct timeval timeout;
				timeout.tv_sec = secs;
				timeout.tv_usec = restMsecs * 1000;

				setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout,
							sizeof(timeout));
			}
			break;

		case LineOption::SendTimeout:
			{
				int msecs = *(int*)data;
				int secs = msecs % 1000;
				int restMsecs = msecs - secs * 1000;
				struct timeval timeout;
				timeout.tv_sec = secs;
				timeout.tv_usec = restMsecs * 1000;

				setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout,
							sizeof(timeout));
			}
			break;
		default:
			throw UnsupportedOption("");
	}
}

TcpSocketAcceptor::TcpSocketAcceptor(const std::string& address) : m_address(address)
{
	m_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(m_socket < 0)
		throw IoException(std::string("Unable to create socket: " + std::to_string(m_socket)));

	int enable = 1;
	if(setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
		throw IoException(std::string("Unable to set socket option: " + std::to_string(m_socket)));


	auto semicolon = m_address.find(':');
	auto host = m_address.substr(0, semicolon);
	auto port = atoi(m_address.substr(semicolon + 1).c_str());
	sockaddr_in serverName;
	memset(&serverName, 0, sizeof(serverName));
	serverName.sin_family = AF_INET;
	if(host != "*")
		serverName.sin_addr.s_addr = inet_addr(host.c_str());
	else
		serverName.sin_addr.s_addr = INADDR_ANY;
	serverName.sin_port = htons(port);

	int rc = bind(m_socket, (sockaddr*)&serverName, sizeof(serverName));
	if(rc < 0)
		throw IoException(std::string("Unable to bind tcp socket: " + std::to_string(rc)) + "/" + std::to_string(errno));

	rc = listen(m_socket, 10);
	if(rc < 0)
	{
		close(m_socket);
		throw IoException(std::string("Unable to listen socket: " + std::to_string(rc)));
	}
}

TcpSocketAcceptor::~TcpSocketAcceptor()
{
	close(m_socket);
}

std::shared_ptr<IoLine> TcpSocketAcceptor::waitConnection(const std::chrono::milliseconds& timeout)
{
	sockaddr addr;
	socklen_t clen = sizeof(addr);
	int newsock = accept(m_socket, &addr, &clen);
	if(newsock > 0)
	{
		return std::make_shared<UnixSocket>(newsock, "");
	}
	return std::shared_ptr<IoLine>();
}

bool TcpSocketFactory::supportsScheme(const std::string& scheme)
{
	return scheme == "tcp";
}

std::shared_ptr<IoLine> TcpSocketFactory::createClient(const std::string& address)
{
	auto socket = std::make_shared<TcpSocket>(address);
	if(socket)
		socket->connect();
	return socket;
}

std::shared_ptr<IoAcceptor> TcpSocketFactory::createServer(const std::string& address)
{
	return std::make_shared<TcpSocketAcceptor>(address);
}

}
}
