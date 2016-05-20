
#ifndef LIBGOLDMINE_EXCEPTIONS
#define LIBGOLDMINE_EXCEPTIONS 

#include <exception>
#include <string>
#include <boost/exception/all.hpp>

namespace goldmine
{
struct LibGoldmineException : virtual public boost::exception, virtual public std::exception
{
    const char* what() const throw()
    {
        return boost::diagnostic_information_what(*this);
    }
};

struct LogicError : public LibGoldmineException
{
};

struct ProtocolError : public LibGoldmineException
{
};

struct ParameterError : public LibGoldmineException
{
};

struct FormatError : public LibGoldmineException
{
};

struct ZmqError : public LibGoldmineException
{
};

typedef boost::error_info<struct errinfo_str_, std::string> errinfo_str;
}


#endif /* ifndef GQG_EXCEPTIONS */
