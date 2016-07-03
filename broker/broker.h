/*
 * broker.h
 */

#ifndef CORE_BROKER_H_
#define CORE_BROKER_H_

#include <memory>
#include <list>
#include <string>
#include <map>
#include <functional>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "goldmine/data.h"

namespace goldmine
{

class Order
{
public:

	enum class OrderType
	{
		Market,
		Limit
	};

	enum class Operation
	{
		Buy,
		Sell
	};

	enum class State
	{
		Unsubmitted,
		Submitted,
		PartiallyExecuted,
		Executed,
		Cancelled,
		Rejected,
		Error
	};

	typedef std::shared_ptr<Order> Ptr;

	Order(int clientAssignedId, const std::string& account, const std::string& security, double price, int quantity, Operation operation, OrderType type);
	virtual ~Order();

	void updateState(State state);

	int localId() const { return m_id; }
	int clientAssignedId() const { return m_clientAssignedId; }

	std::string account() const { return m_account; }
	std::string security() const { return m_security; }
	double price() const { return m_price; }
	int quantity() const { return m_quantity; }
	void setExecutedQuantity(int q) { m_executedQuantity = q; }
	int executedQuantity() const { return m_executedQuantity; }
	Operation operation() const { return m_operation; }
	OrderType type() const { return m_type; }

	State state() const { return m_state; }

	std::string stringRepresentation() const;

	void setMessage(const std::string& message) { m_message = message; }

	std::string message() const { return m_message; }

private:
	int m_id;
	int m_clientAssignedId;

	std::string m_account;
	std::string m_security;
	double m_price;
	int m_quantity;
	int m_executedQuantity;
	Operation m_operation;
	OrderType m_type;

	State m_state;

	std::string m_message;
};

struct Trade
{
	Trade() : orderId(0), price(0), quantity(0), operation(Order::Operation::Buy),
		account(), security(), timestamp(0), useconds(0) {}
	int orderId;
	double price;
	int quantity;
	Order::Operation operation;
	std::string account;
	std::string security;
	uint64_t timestamp;
	uint32_t useconds;
};

struct Position
{
	std::string security;
	int amount;
};

class Broker
{
public:
	class Reactor
	{
	public:
		virtual ~Reactor() {}

		virtual void orderCallback(const Order::Ptr& order) = 0;
		virtual void tradeCallback(const Trade& trade) = 0;
	};

	typedef std::shared_ptr<Broker> Ptr;

	virtual ~Broker() {}

	virtual void submitOrder(const Order::Ptr& order) = 0;
	virtual void cancelOrder(const Order::Ptr& order) = 0;

	virtual void registerReactor(const std::shared_ptr<Reactor>& reactor) = 0;
	virtual void unregisterReactor(const std::shared_ptr<Reactor>& reactor) = 0;

	virtual Order::Ptr order(int localId) = 0;

	virtual std::list<std::string> accounts() = 0;
	virtual bool hasAccount(const std::string& account) = 0;

	virtual std::list<Position> positions() = 0;
};

}

#endif /* CORE_BROKER_H_ */
