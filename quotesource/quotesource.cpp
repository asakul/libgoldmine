/*
 * quotesource.cpp
 */

#include "quotesource.h"

#include <functional>

namespace goldmine
{

QuoteSource::QuoteSource(zmq::context_t& ctx, const std::string& endpoint) : m_ctx(ctx),
	m_endpoint(endpoint), m_run(false)
{
}

QuoteSource::~QuoteSource()
{
}

void QuoteSource::addReactor(const Reactor::Ptr& reactor)
{
}

void QuoteSource::removeReactor(const Reactor::Ptr& reactor)
{
}


void QuoteSource::incomingTick(const goldmine::Tick& tick)
{
}

void QuoteSource::incomingBar(const goldmine::Summary& bar)
{
}

void QuoteSource::start()
{
	m_thread = boost::thread(std::bind(&QuoteSource::eventLoop, this));
}

void QuoteSource::stop()
{
	m_run = false;
	m_thread.interrupt();
	if(m_thread.joinable())
		m_thread.try_join_for(boost::chrono::milliseconds(200));
}


void QuoteSource::eventLoop()
{
	m_run = true;

	zmq::socket_t controlSocket(m_ctx, m_endpoint);

	while(m_run)
	{
	}
}


} /* namespace goldmine */
