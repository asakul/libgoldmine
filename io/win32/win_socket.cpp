
#include "win_socket.h"

namespace goldmine
{
namespace io
{
WinSocket::WinSocket(const std::string& address) : m_address(address)
{
	m_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(m_socket == INVALID_SOCKET)
		throw IoException("Unable to make socket: " + address);
}

WinSocket::WinSocket(SOCKET fd, const std::string& address) : m_address(address),
	m_socket(fd)
{
}

WinSocket::~WinSocket()
{
	closesocket(m_socket);
}

void WinSocket::connect()
{
	auto semicolon = m_address.find(':');
	auto host = m_address.substr(0, semicolon);
	auto port = atoi(m_address.substr(semicolon + 1).c_str());
	SOCKADDR_IN addr;

	addr.sin_family = PF_INET;
	addr.sin_addr.s_addr = inet_addr(host.c_str());
	addr.sin_port = htons(port);

	int retVal = ::connect(m_socket,(LPSOCKADDR)&addr, sizeof(addr));
    if(retVal == SOCKET_ERROR)
		throw IoException("Unable to connect to address: " + m_address);
}

ssize_t WinSocket::read(void* buffer, size_t buflen)
{
	ssize_t rc = ::recv(m_socket, (char*)buffer, buflen, 0);
	return rc;
}

ssize_t WinSocket::write(void* buffer, size_t buflen)
{
	ssize_t rc = ::send(m_socket, (char*)buffer, buflen, 0);
	return rc;
}

void WinSocket::setOption(LineOption option, void* data)
{
	switch(option)
	{
		case LineOption::ReceiveTimeout:
			{
				DWORD value = *(int*)data;
				setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&value, sizeof(value));
				break;
			}
		case LineOption::SendTimeout:
			{
				DWORD value = *(int*)data;
				setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, (char*)&value, sizeof(value));
				break;
			}

		default:
			throw UnsupportedOption("");
	}

}

WinSocketAcceptor::WinSocketAcceptor(const std::string& address) : m_address(address)
{
	m_socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(m_socket == INVALID_SOCKET)
		throw IoException(std::string("Unable to create socket: " + m_address));

	BOOL enable = 1;
	if(setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&enable, sizeof(enable)) < 0)
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
		closesocket(m_socket);
		throw IoException(std::string("Unable to listen socket: " + std::to_string(rc)));
	}
}

WinSocketAcceptor::~WinSocketAcceptor()
{
	closesocket(m_socket);
}

std::shared_ptr<IoLine> WinSocketAcceptor::waitConnection(const std::chrono::milliseconds& timeout)
{
	int newsock = accept(m_socket, NULL, NULL);
	if(newsock > 0)
	{
		return std::make_shared<WinSocket>(newsock, "");
	}
	return std::shared_ptr<IoLine>();
}

WinSocketFactory::WinSocketFactory()
{
	WORD sockVer;
    WSADATA wsaData;
    int retVal;
 
    sockVer = MAKEWORD(2,2);
    
    WSAStartup(sockVer, &wsaData);
}

WinSocketFactory::~WinSocketFactory()
{
	WSACleanup();
}

bool WinSocketFactory::supportsScheme(const std::string& scheme)
{
	return scheme == "tcp";
}

std::shared_ptr<IoLine> WinSocketFactory::createClient(const std::string& address)
{
	auto socket = std::make_shared<WinSocket>(address);
	if(socket)
		socket->connect();
	return socket;
}

std::shared_ptr<IoAcceptor> WinSocketFactory::createServer(const std::string& address)
{
	return std::make_shared<WinSocketAcceptor>(address);
}
}
}
