
#ifndef QUOTESOURCECLIENT_H
#define QUOTESOURCECLIENT_H

#include "io/iolinemanager.h"
#include "goldmine/data.h"

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

	QuoteSourceClient(io::IoLineManager& manager, const std::string& address);
	virtual ~QuoteSourceClient();

	void startStream(const std::string& streamId);
	void stop();

	void registerSink(const std::shared_ptr<Sink>& sink);

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};
}

#endif /* ifndef QUOTESOURCECLIENT_H */
