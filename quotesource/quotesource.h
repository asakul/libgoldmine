/*
 * quotesource.h
 */

#ifndef QUOTESOURCE_QUOTESOURCE_H_
#define QUOTESOURCE_QUOTESOURCE_H_

#include "goldmine/data.h"

#include "exceptions.h"

#include "json/json.h"
#include "cppio/message.h"
#include "cppio/iolinemanager.h"

#include <boost/thread.hpp>

#include <string>
#include <memory>

namespace goldmine
{

class QuoteSource
{
public:

	class Reactor
	{
	public:
		using Ptr = std::shared_ptr<Reactor>;
		virtual ~Reactor() {}

		virtual void exception(const LibGoldmineException& e) = 0;
		virtual void clientConnected(int fd) = 0;
		virtual void clientRequestedStream(const std::string& identity, const std::string& streamId) = 0;
	};

public:
	using Ptr = std::shared_ptr<QuoteSource>;

	QuoteSource(const std::shared_ptr<cppio::IoLineManager>& manager, const std::string& endpoint);
	virtual ~QuoteSource();

	void addReactor(const Reactor::Ptr& reactor);
	void removeReactor(const Reactor::Ptr& reactor);

	void start();
	void stop() noexcept;

	void incomingTick(const std::string& ticker, const Tick& tick);

private:
	void eventLoop();

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
	friend class Client;
};

} /* namespace goldmine */

#endif /* QUOTESOURCE_QUOTESOURCE_H_ */
