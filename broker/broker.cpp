/*
 * broker.cpp
 */

#include "broker.h"
#include <atomic>
#include <sstream>

namespace goldmine
{

static std::atomic_int gs_id;

static std::string opString(Order::Operation op)
{
	if(op == Order::Operation::Buy)
		return "buy";
	else if(op == Order::Operation::Sell)
		return "sell";
	else
		return "???";
}

Order::Order(int clientAssignedId, const std::string& account, const std::string& security,
		double price, int quantity, Operation operation, OrderType type) :
	m_id(gs_id.fetch_add(1)),
	m_clientAssignedId(clientAssignedId),
	m_account(account),
	m_security(security),
	m_price(price),
	m_quantity(quantity),
	m_executedQuantity(0),
	m_operation(operation),
	m_type(type),
	m_state(State::Unsubmitted)
{

}
Order::~Order()
{

}

void Order::updateState(State state)
{
	m_state = state;
}

std::string Order::stringRepresentation() const
{
	std::stringstream ss;

	ss << "LocalID: " << m_id << "; ClientAssignedID: " << clientAssignedId() << "; " << 
			opString(m_operation) << " " << quantity() << " of " << security();

	if(m_type == Order::OrderType::Limit)
		ss << " at " << m_price;

	ss << " (";
	switch(m_state)
	{
		case State::Submitted:
			ss << "submitted";
			break;

		case State::Cancelled:
			ss << "cancelled";
			break;

		case State::Rejected:
			ss << "rejected";
			break;

		case State::Executed:
			ss << "executed";
			break;

		case State::PartiallyExecuted:
			ss << "partially executed";
			break;

		case State::Unsubmitted:
			ss << "unsubmitted";
			break;
	}

	ss << ")";

	return ss.str();
}

}
