
#ifndef BROKERSERVER_H
#define BROKERSERVER_H

#include "broker.h"

#include "cppio/iolinemanager.h"

#include <memory>

namespace goldmine
{

class BrokerServer
{
public:
	using Ptr = std::shared_ptr<BrokerServer>;

	BrokerServer(const std::shared_ptr<cppio::IoLineManager>& manager, const std::string& endpoint);
	virtual ~BrokerServer();

	void registerBroker(const Broker::Ptr& broker);
	void unregisterBroker(const Broker::Ptr& broker);

	void start();
	void stop();

private:
	struct Impl;
	std::shared_ptr<Impl> m_impl;
};

}

#endif /* ifndef BROKERSERVER_H */
