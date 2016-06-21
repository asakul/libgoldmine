
#ifndef QUOTESOURCECLIENT_H
#define QUOTESOURCECLIENT_H

#include "cppio/iolinemanager.h"
#include "goldmine/data.h"

#include <boost/shared_ptr.hpp>
#include <memory>
#include <string>

namespace goldmine
{
class QuoteSourceClient
{
public:
	class Sink
	{
	public:
		virtual ~Sink();

		virtual void incomingTick(const std::string& ticker, const Tick& tick) = 0;
	};

	QuoteSourceClient(const std::shared_ptr<cppio::IoLineManager>& manager, const std::string& address);
	virtual ~QuoteSourceClient();

	void startStream(const std::string& streamId);
	void stop();

	void registerSink(const std::shared_ptr<Sink>& sink);
	void registerBoostSink(const boost::shared_ptr<Sink>& sink);
	void registerRawSink(Sink* sink);

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};
}

#endif /* ifndef QUOTESOURCECLIENT_H */
