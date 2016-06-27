
#include <boost/python.hpp>
#include <boost/shared_ptr.hpp>
#include "quotesource/quotesource.h"
#include "quotesource/quotesourceclient.h"
#include "broker/broker.h"
#include "broker/brokerclient.h"
#include "broker/brokerserver.h"
#include "cppio/iolinemanager.h"
#include "goldmine/exceptions.h"

using namespace boost::python;
using namespace goldmine;

static std::string serializeOrderState(Order::State state)
{
	switch(state)
	{
	case Order::State::Cancelled:
		return "cancelled";
	case Order::State::Executed:
		return "executed";
	case Order::State::PartiallyExecuted:
		return "partially-executed";
	case Order::State::Rejected:
		return "rejected";
	case Order::State::Submitted:
		return "submitted";
	case Order::State::Unsubmitted:
		return "unsubmitted";
	case Order::State::Error:
		return "error";
	}
	return "unknown";
}

std::shared_ptr<cppio::IoLineManager> makeIoLineManager()
{
	return cppio::createLineManager();
}

static Order::Ptr createOrder(int clientAssignedId, const std::string& account,
		const std::string& security, double price, int quantity, const std::string& encodedOperation,
		const std::string& encodedOrderType)
{
	Order::Operation operation;
	if(encodedOperation == "buy")
		operation = Order::Operation::Buy;
	else if(encodedOperation == "sell")
		operation = Order::Operation::Sell;
	else
		BOOST_THROW_EXCEPTION(ParameterError() << errinfo_str("Invalid operation specified: " + encodedOperation));

	Order::OrderType orderType;
	if(encodedOrderType == "market")
		orderType = Order::OrderType::Market;
	else if(encodedOrderType == "limit")
		orderType = Order::OrderType::Limit;
	else
		BOOST_THROW_EXCEPTION(ParameterError() << errinfo_str("Invalid orderType specified: " + encodedOrderType));

	return std::make_shared<Order>(clientAssignedId, account, security, price, quantity, operation, orderType);
}

static std::string Order_state(const std::shared_ptr<Order>& self)
{
	return serializeOrderState(self->state());
}

static std::string Order_operation(const std::shared_ptr<Order>& self)
{
	switch(self->operation())
	{
		case Order::Operation::Buy:
			return "buy";
		case Order::Operation::Sell:
			return "sell";
	}
	return "???";
}

static std::string Order_orderType(const std::shared_ptr<Order>& self)
{
	switch(self->type())
	{
		case Order::OrderType::Market:
			return "market";
		case Order::OrderType::Limit:
			return "limit";
	}
	return "???";
}

static void Trade_setOperation(Trade& trade, const std::string& operation)
{
	Order::Operation op;
	if(operation == "buy")
		op = Order::Operation::Buy;
	else if(operation == "sell")
		op = Order::Operation::Sell;
	else
		BOOST_THROW_EXCEPTION(ParameterError() << errinfo_str("Invalid operation specified: " + operation));

	trade.operation = op;
}

static std::string Trade_getOperation(Trade& trade)
{
	switch(trade.operation)
	{
		case Order::Operation::Buy:
			return "buy";
		case Order::Operation::Sell:
			return "sell";
	}
	return "???";
}

BOOST_PYTHON_MODULE(pygoldmine)
{
	// Create GIL
	if(!PyEval_ThreadsInitialized())
	{
		PyEval_InitThreads();
	}

	register_ptr_to_python<std::shared_ptr<QuoteSource>>();
	register_ptr_to_python<std::shared_ptr<QuoteSource::Reactor>>();
	register_ptr_to_python<std::shared_ptr<QuoteSourceClient>>();
	register_ptr_to_python<boost::shared_ptr<QuoteSourceClient::Sink>>();
	register_ptr_to_python<std::shared_ptr<cppio::IoLineManager>>();

	class_<cppio::IoLineManager, std::shared_ptr<cppio::IoLineManager>, boost::noncopyable>("IoLineManager", no_init);

	def("makeIoLineManager", makeIoLineManager);

	class_<decimal_fixed>("decimal_fixed")
		.def(init<double>())
		.def(init<int64_t, int32_t>())
		.def("toDouble", &decimal_fixed::toDouble)
		.add_property("value", make_getter(&decimal_fixed::value), make_setter(&decimal_fixed::value))
		.add_property("fractional", make_getter(&decimal_fixed::fractional), make_setter(&decimal_fixed::fractional));

	class_<Tick>("Tick")
		.def(init<>())
		.add_property("timestamp", make_getter(&Tick::timestamp), make_setter(&Tick::timestamp))
		.add_property("useconds", make_getter(&Tick::useconds), make_setter(&Tick::useconds))
		.add_property("datatype", make_getter(&Tick::datatype), make_setter(&Tick::datatype))
		.add_property("value", make_getter(&Tick::value), make_setter(&Tick::value))
		.add_property("volume", make_getter(&Tick::volume), make_setter(&Tick::volume));

	class GIL
	{
	public:
		GIL()
		{
			gstate = PyGILState_Ensure();
		}

		~GIL()
		{
			PyGILState_Release(gstate);
		}

	private:
		PyGILState_STATE gstate;
	};

	struct ReactorWrap : QuoteSource::Reactor, wrapper<QuoteSource::Reactor>
	{
		void exception(const LibGoldmineException& e)
		{

		}

		void clientConnected(int fd)
		{
			GIL gil;

			this->get_override("clientConnected")(fd);
		}

		void clientRequestedStream(const std::string& identity, const std::string& streamId)
		{
			GIL gil;

			this->get_override("clientRequestedStream")(identity, streamId);
		}
	};

	class_<ReactorWrap, std::shared_ptr<ReactorWrap>, boost::noncopyable>("QuoteSourceReactor")
		.def("clientConnected", pure_virtual(&QuoteSource::Reactor::clientConnected))
		.def("clientRequestedStream", pure_virtual(&QuoteSource::Reactor::clientRequestedStream));

	class_<QuoteSource, std::shared_ptr<QuoteSource>, boost::noncopyable>("QuoteSource", no_init)
		.def(init<std::shared_ptr<cppio::IoLineManager>, std::string>(args("linemanager", "endpoint")))
		.def("start", &QuoteSource::start)
		.def("stop", &QuoteSource::stop)
		.def("incomingTick", &QuoteSource::incomingTick);

	struct SinkWrap : QuoteSourceClient::Sink, wrapper<QuoteSourceClient::Sink>
	{
		void incomingTick(const std::string& ticker, const Tick& tick)
		{
			GIL gil;

			this->get_override("incomingTick")(ticker, tick);
		}

	};

	class_<SinkWrap, boost::shared_ptr<SinkWrap>, boost::noncopyable>("QuoteSourceClientSink")
		.def("incomingTick", &SinkWrap::incomingTick);

	class_<QuoteSourceClient, std::shared_ptr<QuoteSourceClient>, boost::noncopyable>("QuoteSourceClient", no_init)
		.def(init<std::shared_ptr<cppio::IoLineManager>, std::string>(args("linemanager", "endpoint")))
		.def("startStream", &QuoteSourceClient::startStream)
		.def("stop", &QuoteSourceClient::stop)
		.def("registerSink", &QuoteSourceClient::registerBoostSink);

	def("createOrder", createOrder);

	class_<Order, std::shared_ptr<Order>, boost::noncopyable>("Order", no_init)
		.def("localId", &Order::localId)
		.def("clientAssignedId", &Order::clientAssignedId)
		.def("account", &Order::account)
		.def("security", &Order::security)
		.def("price", &Order::price)
		.def("quantity", &Order::quantity)
		.def("setExecutedQuantity", &Order::setExecutedQuantity)
		.def("executedQuantity", &Order::executedQuantity)
		.def("state", Order_state)
		.def("operation", Order_operation)
		.def("orderType", Order_orderType)
		.def("message", &Order::message);

	class_<Trade>("Trade")
		.def(init<>())
		.add_property("orderId", make_getter(&Trade::orderId), make_setter(&Trade::orderId))
		.add_property("price", make_getter(&Trade::price), make_setter(&Trade::price))
		.add_property("quantity", make_getter(&Trade::quantity), make_setter(&Trade::quantity))
		.add_property("account", make_getter(&Trade::account), make_setter(&Trade::account))
		.add_property("security", make_getter(&Trade::security), make_setter(&Trade::security))
		.add_property("operation", Trade_setOperation, Trade_getOperation)
		.add_property("timestamp", make_getter(&Trade::timestamp), make_setter(&Trade::timestamp))
		.add_property("useconds", make_getter(&Trade::useconds), make_setter(&Trade::useconds));

	
	struct BrokerClientReactorWrap : public BrokerClient::Reactor, wrapper<BrokerClient::Reactor>
	{
		virtual void orderCallback(const Order::Ptr& order)
		{
			GIL gil;

			this->get_override("orderCallback")(order);
		}

		virtual void tradeCallback(const Trade& trade)
		{
			GIL gil;

			this->get_override("tradeCallback")(trade);
		}
	};

	class_<BrokerClientReactorWrap, boost::shared_ptr<BrokerClientReactorWrap>, boost::noncopyable>("BrokerClientReactorWrap")
		.def("orderCallback", &BrokerClientReactorWrap::orderCallback)
		.def("tradeCallback", &BrokerClientReactorWrap::tradeCallback)
		;

	class_<BrokerClient, std::shared_ptr<BrokerClient>, boost::noncopyable>("BrokerClient", init<std::shared_ptr<cppio::IoLineManager>, std::string>(args("linemanager", "endpoint")))
		.def("start", &BrokerClient::start)
		.def("stop", &BrokerClient::stop)
		.def("registerReactor", static_cast<void (BrokerClient::*)(const boost::shared_ptr<BrokerClient::Reactor>&)>(&BrokerClient::registerReactor))
		.def("unregisterReactor", static_cast<void (BrokerClient::*)(const boost::shared_ptr<BrokerClient::Reactor>&)>(&BrokerClient::unregisterReactor))
		.def("submitOrder", &BrokerClient::submitOrder)
		.def("cancelOrder", &BrokerClient::cancelOrder)
		.def("setIdentity", &BrokerClient::setIdentity)
		.def("identity", &BrokerClient::identity)
		;

}

