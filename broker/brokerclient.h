
#ifndef BROKERCLIENT_H
#define BROKERCLIENT_H

#include "broker.h"

#include "cppio/iolinemanager.h"

#include <memory>
#include <string>

namespace goldmine
{

class BrokerClient
{
public:
	class Reactor
	{
	public:
		using Ptr = std::shared_ptr<Reactor>;

		virtual ~Reactor() {}
		virtual void orderCallback(const Order::Ptr& order) = 0;
		virtual void tradeCallback(const Trade& trade) = 0;
	};

	using Ptr = std::shared_ptr<BrokerClient>;

	BrokerClient(const std::shared_ptr<cppio::IoLineManager>& manager, const std::string& address);
	virtual ~BrokerClient();

	void registerReactor(const Reactor::Ptr& reactor);
	void unregisterReactor(const Reactor::Ptr& reactor);

	void start();
	void stop();
	
	void submitOrder(const Order::Ptr& order);
	void cancelOrder(const Order::Ptr& order);

	void setIdentity(const std::string& id);
	std::string identity() const;

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
};

}

#endif /* ifndef BROKERCLIENT_H */
