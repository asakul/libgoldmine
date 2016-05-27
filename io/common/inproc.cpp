
#include "inproc.h"

#include <cstring>

namespace goldmine
{
namespace io
{
	DataQueue::DataQueue(size_t bufferSize) : m_data(bufferSize),
		m_wrptr(0),
		m_rdptr(0)
	{
	}

	DataQueue::~DataQueue()
	{
	}

	size_t DataQueue::read(void* buffer, size_t buflen)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		if(m_wrptr < m_rdptr)
		{
			size_t tocopy = std::min(m_data.size() - m_rdptr, buflen);
			memcpy(buffer, m_data.data() + m_rdptr, tocopy);
			m_rdptr += tocopy;
			if(m_rdptr == m_data.size())
				m_rdptr = 0;

			if(tocopy < buflen)
			{
				if(m_wrptr == m_rdptr)
					return tocopy;
				else if(tocopy == 0)
					return 0;
				else
				{
					lock.unlock();
					return tocopy + read((char*)buffer + tocopy, buflen - tocopy);
				}
			}
			else
				return tocopy;
		}
		else if(m_wrptr > m_rdptr)
		{
			size_t tocopy = std::min(m_wrptr - m_rdptr, buflen);
			memcpy(buffer, m_data.data() + m_rdptr, tocopy);
			m_rdptr += tocopy;
			return tocopy;
		}
		else
		{
			m_cond.wait(lock, [&]() { return m_wrptr != m_rdptr; });
		}
	}

	size_t DataQueue::write(void* buffer, size_t buflen)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_cond.notify_one();
		if(m_rdptr <= m_wrptr)
		{
			size_t tocopy = std::min(m_data.size() - m_wrptr, buflen);
			if(m_wrptr + tocopy >= m_data.size())
			{
				size_t newwrptr = m_wrptr + tocopy - m_data.size();
				if(newwrptr >= m_rdptr)
					return 0;
			}
			memcpy(m_data.data() + m_wrptr, buffer, tocopy);
			m_wrptr += tocopy;

			if(m_wrptr == m_data.size())
			{
				m_wrptr = 0;
			}

			if(tocopy == 0)
				return 0;
			else
			{
				lock.unlock();
				return tocopy + write((char*)buffer + tocopy, buflen - tocopy);
			}
		}
		else
		{
			if(m_wrptr + buflen >= m_rdptr)
				return 0;

			memcpy(m_data.data() + m_wrptr, buffer, buflen);
			m_wrptr += buflen;
			return buflen;
		}
	}

	InprocLine::InprocLine()
	{
	}

	InprocLine::~InprocLine()
	{
	}

	ssize_t InprocLine::read(void* buffer, size_t buflen)
	{
	}

	ssize_t InprocLine::write(void* buffer, size_t buflen)
	{
	}

	InprocAcceptor::InprocAcceptor()
	{
	}

	InprocAcceptor::~InprocAcceptor()
	{
	}

	std::shared_ptr<IoLine> InprocAcceptor::waitConnection(const std::chrono::milliseconds& timeout)
	{
	}


	InprocLineFactory::InprocLineFactory()
	{
	}

	InprocLineFactory::~InprocLineFactory()
	{
	}

	bool InprocLineFactory::supportsScheme(const std::string& scheme)
	{
	}

	std::shared_ptr<IoLine> InprocLineFactory::createClient(const std::string& address)
	{
	}

	std::shared_ptr<IoAcceptor> InprocLineFactory::createServer(const std::string& address)
	{
	}
}
}

