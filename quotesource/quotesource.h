/*
 * quotesource.h
 */

#ifndef QUOTESOURCE_QUOTESOURCE_H_
#define QUOTESOURCE_QUOTESOURCE_H_

#include "goldmine/data.h"

#include "exceptions.h"

#include "json/json.h"
#include "zmqpp/zmqpp.hpp"

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

	QuoteSource(zmqpp::context& ctx, const std::string& endpoint);
	virtual ~QuoteSource();

	void addReactor(const Reactor::Ptr& reactor);
	void removeReactor(const Reactor::Ptr& reactor);

	void incomingTick(const goldmine::Tick& tick);
	void incomingBar(const goldmine::Summary& bar);

	void start();
	void stop() noexcept;

private:
	void eventLoop();

	void handleSocket(zmqpp::socket& control, zmqpp::message& msg);
	void handleControl(const std::string& peerId, zmqpp::socket& control, const Json::Value& root);

private:
	zmqpp::context& m_ctx;
	std::string m_endpoint;
	boost::thread m_thread;
	bool m_run;

	std::vector<Reactor::Ptr> m_reactors;
};

} /* namespace goldmine */

#endif /* QUOTESOURCE_QUOTESOURCE_H_ */
