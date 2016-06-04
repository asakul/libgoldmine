
#ifndef IOLINE_H
#define IOLINE_H

#include <cstddef>
#include <stdexcept>
#include <memory>
#include <chrono>

namespace goldmine
{
namespace io
{

class IoException : public std::runtime_error
{
public:
	IoException(const std::string& errmsg) : std::runtime_error(errmsg) {}
};

class TimeoutException : public IoException
{
public:
	TimeoutException(const std::string& errmsg) : IoException(errmsg) {}
};

class UnsupportedOption : public IoException
{
public:
	UnsupportedOption(const std::string& errmsg) : IoException(errmsg) {}
};

enum class LineOption
{
	ReceiveTimeout = 1,
	SendTimeout = 2
};

class IoLine
{
public:
	virtual ~IoLine() = 0;

	virtual ssize_t read(void* buffer, size_t buflen) = 0;
	virtual ssize_t write(void* buffer, size_t buflen) = 0;

	virtual void setOption(LineOption option, void* data) = 0;
};

inline IoLine::~IoLine() {}

class IoAcceptor
{
public:
	virtual ~IoAcceptor() = 0;

	virtual std::shared_ptr<IoLine> waitConnection(const std::chrono::milliseconds& timeout) = 0;
};

inline IoAcceptor::~IoAcceptor() {}

class IoLineFactory
{
public:
	virtual ~IoLineFactory() = 0;

	virtual bool supportsScheme(const std::string& scheme) = 0;
	virtual std::shared_ptr<IoLine> createClient(const std::string& address) = 0;
	virtual std::shared_ptr<IoAcceptor> createServer(const std::string& address) = 0;
};

inline IoLineFactory::~IoLineFactory() {}

}
}

#endif /* ifndef IOLINE_H */