
#include "inproc.h"

#include <cstring>

#include <mutex>
#include <condition_variable>
#include <list>
#include <vector>

namespace goldmine
{
namespace io
{
	RingBuffer::RingBuffer(size_t bufferSizePower) : m_data(bufferSizePower),
		m_wrptr(0),
		m_rdptr(0)
	{
	}

	RingBuffer::~RingBuffer()
	{
	}

	size_t RingBuffer::read(void* buffer, size_t buflen)
	{
		if(m_wrptr == m_rdptr)
		{
			return 0;
		}
		else if(m_rdptr < m_wrptr)
		{
			size_t tocopy = std::min(m_wrptr - m_rdptr, buflen);
			memcpy(buffer, m_data.data() + m_rdptr, tocopy);
			m_rdptr += tocopy;
			return tocopy;
		}
		else
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
					return tocopy + read((char*)buffer + tocopy, buflen - tocopy);
			}

			return tocopy;
		}
	}

	size_t RingBuffer::write(void* buffer, size_t buflen)
	{
		if(m_rdptr <= m_wrptr)
		{
			size_t tocopy = std::min(m_data.size() - m_wrptr, buflen);
			if((m_rdptr == 0) && (tocopy == m_data.size() - m_wrptr))
				tocopy--;
			memcpy(m_data.data() + m_wrptr, buffer, tocopy);
			m_wrptr += tocopy;

			if(m_wrptr == m_data.size())
				m_wrptr = 0;

			if(tocopy < buflen)
			{
				if(m_rdptr == 0)
					return tocopy;
				else if(tocopy == 0)
					return 0;
				else
					return tocopy + write((char*)buffer + tocopy, buflen - tocopy);
			}
			return tocopy;
		}
		else
		{
			size_t tocopy = std::min(m_rdptr - 1 - m_wrptr, buflen);

			memcpy(m_data.data() + m_wrptr, buffer, tocopy);
			m_wrptr += tocopy;
			return tocopy;
		}
	}

	size_t RingBuffer::availableReadSize() const
	{
		if(m_rdptr == m_wrptr)
			return 0;
		else if(m_rdptr < m_wrptr)
		{
			return m_wrptr - m_rdptr;
		}
		else
		{
			return m_data.size() - m_rdptr + m_wrptr;
		}
	}

	size_t RingBuffer::availableWriteSize() const
	{
		if(m_rdptr == m_wrptr)
			return m_data.size() - 1;
		else if(m_rdptr < m_wrptr)
		{
			return m_data.size() - m_wrptr + m_rdptr - 1;
		}
		else
		{
			return m_rdptr - m_wrptr - 1;
		}
	}

	DataQueue::DataQueue(size_t bufferSize) : m_buffer(bufferSize)
	{
	}

	DataQueue::~DataQueue()
	{
	}

	size_t DataQueue::read(void* buffer, size_t buflen)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_writeCondition.notify_all();
		if(m_buffer.availableReadSize() == 0)
			m_readCondition.wait(lock);
		return m_buffer.read(buffer, buflen);
	}

	size_t DataQueue::readWithTimeout(void* buffer, size_t buflen, const std::chrono::milliseconds& timeout)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_writeCondition.notify_all();
		if(m_buffer.availableReadSize() == 0)
		{
			bool rc = m_readCondition.wait_for(lock, timeout, [&]() { return m_buffer.availableReadSize() > 0; });
			if(!rc)
				return 0;
		}
		return m_buffer.read(buffer, buflen);
	}

	size_t DataQueue::write(void* buffer, size_t buflen)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		if(buflen >= m_buffer.size())
			return 0;
		m_readCondition.notify_all();
		if(m_buffer.availableWriteSize() < buflen)
			m_writeCondition.wait(lock, [&]() { return m_buffer.availableWriteSize() >= buflen; });
		return m_buffer.write(buffer, buflen);
	}

	size_t DataQueue::availableReadSize() const
	{
		return m_buffer.availableReadSize();
	}

	size_t DataQueue::availableWriteSize() const
	{
		return m_buffer.availableWriteSize();
	}

	static std::mutex gs_mutex;
	static std::condition_variable gs_cond;
	static std::list<std::weak_ptr<InprocAcceptor>> gs_acceptors;
	static std::list<std::weak_ptr<InprocLine>> gs_connectQueue;

	InprocLine::InprocLine(const std::shared_ptr<InprocLine>& other) : m_address(other->address())
	{
		m_in = std::make_shared<DataQueue>(65536);
		m_out = std::make_shared<DataQueue>(65536);

		std::unique_lock<std::mutex> lock(other->m_mutex);

		other->m_out = m_in;
		other->m_in = m_out;

		other->m_condition.notify_one();
	}

	InprocLine::InprocLine(const std::string& address) : m_address(address)
	{
	}

	InprocLine::~InprocLine()
	{
	}

	ssize_t InprocLine::read(void* buffer, size_t buflen)
	{
		if(m_readTimeout > 0)
			return m_in->readWithTimeout(buffer, buflen, std::chrono::milliseconds(m_readTimeout));
		else
			return m_in->read(buffer, buflen);
	}

	ssize_t InprocLine::write(void* buffer, size_t buflen)
	{
		return m_out->write(buffer, buflen);
	}

	void InprocLine::setOption(LineOption option, void* data)
	{
		switch(option)
		{
			case LineOption::ReceiveTimeout:
				m_readTimeout = *reinterpret_cast<uint32_t*>(data);
				break;
			case LineOption::SendTimeout:
				throw UnsupportedOption("");
			default:
				throw UnsupportedOption("");
		}
	}

	void InprocLine::waitForConnection()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		if(m_in && m_out)
			return;

		m_condition.wait(lock, [&]() { return m_in && m_out; });
	}

	InprocAcceptor::InprocAcceptor(const std::string& address) : m_address(address)
	{
	}

	InprocAcceptor::~InprocAcceptor()
	{
	}

	std::shared_ptr<IoLine> InprocAcceptor::waitConnection(const std::chrono::milliseconds& timeout)
	{
		std::unique_lock<std::mutex> lock(gs_mutex);
		auto start = std::chrono::steady_clock::now();
		while(true)
		{
			auto it = gs_connectQueue.begin();
			while(it != gs_connectQueue.end())
			{
				auto line = it->lock();
				if(!line)
				{
					it = gs_connectQueue.erase(it);
				}
				else
				{
					if(line->address() == address())
					{
						it = gs_connectQueue.erase(it);
						auto otherLine = std::make_shared<InprocLine>(line);
						return otherLine;
					}
				}
			}

			auto current = std::chrono::steady_clock::now();
			auto elapsed = current - start;
			auto left = timeout - elapsed;
			if(left < std::chrono::milliseconds::zero())
				return std::shared_ptr<IoLine>();

			gs_cond.wait_for(lock, left);
		}
	}


	InprocLineFactory::InprocLineFactory()
	{
	}

	InprocLineFactory::~InprocLineFactory()
	{
	}

	bool InprocLineFactory::supportsScheme(const std::string& scheme)
	{
		return scheme == "inproc";
	}

	std::shared_ptr<IoLine> InprocLineFactory::createClient(const std::string& address)
	{
		std::shared_ptr<InprocLine> line;
		{
			std::unique_lock<std::mutex> lock(gs_mutex);
			line = std::make_shared<InprocLine>(address);
			gs_connectQueue.push_back(std::weak_ptr<InprocLine>(line));
		}

		line->waitForConnection();

		return line;
	}

	std::shared_ptr<IoAcceptor> InprocLineFactory::createServer(const std::string& address)
	{
		std::unique_lock<std::mutex> lock(gs_mutex);
		for(auto it = gs_acceptors.begin(); it != gs_acceptors.end(); ++it)
		{
			auto acceptor = it->lock();
			if(acceptor)
			{
				if(acceptor->address() == address)
					throw IoException("Acceptor with address " + address + " already exists");
			}
		}
		auto acceptor = std::make_shared<InprocAcceptor>(address);
		gs_acceptors.push_back(acceptor);
		return acceptor;
	}
}
}

