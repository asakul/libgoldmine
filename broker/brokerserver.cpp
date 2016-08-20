
#include "brokerserver.h"

#include "cppio/iolinemanager.h"
#include "cppio/message.h"
#include "cppio/errors.h"
#include "json/json.h"

#include "goldmine/exceptions.h"

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <array>
#include <queue>

namespace goldmine
{

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

static std::string serializeExecutionTime(time_t timestamp, int useconds)
{
	using namespace boost::posix_time;
	using namespace boost::gregorian;
	auto timePoint = from_time_t(timestamp) + microseconds(useconds);
	std::array<char, 256> buf;
	snprintf(buf.data(), 256, "%d-%02d-%02d %02d:%02d:%02d.%03d",
			(int)timePoint.date().year(),
			(int)timePoint.date().month(),
			(int)timePoint.date().day(),
			(int)timePoint.time_of_day().hours(),
			(int)timePoint.time_of_day().minutes(),
			(int)timePoint.time_of_day().seconds(),
			(int)(1000ull * (double)timePoint.time_of_day().fractional_seconds() / timePoint.time_of_day().ticks_per_second()));

	return std::string(buf.data());
}


struct BrokerServer::Impl : public Broker::Reactor
{
	class Client
	{
	public:
		using Ptr = std::shared_ptr<Client>;

		Client(const std::shared_ptr<cppio::IoLine>& ioLine, Impl* i) : line(ioLine),
			impl(i),
			run(false)
		{
		}

		void start()
		{
			thread = boost::thread(std::bind(&Client::eventLoop, this));
		}

		void stop()
		{
			run = false;
		}

		void join()
		{
			if(thread.joinable())
				thread.join();
		}

		void eventLoop()
		{
			run = true;
			cppio::MessageProtocol proto(line.get());

			while(run)
			{
				try
				{
					cppio::Message incoming;
					ssize_t rc = proto.readMessage(incoming);

					if(rc > 0)
					{
						handleMessage(incoming, proto);
					}
					else if(rc != cppio::eTimeout)
					{
						run = false;
					}
				}
				catch(const LibGoldmineException& e)
				{
					Json::Value response;
					response["result"] = "error";
					auto errmsg = boost::get_error_info<errinfo_str>(e);
					if(errmsg)
						response["reason"] = *errmsg;

					Json::FastWriter writer;
					cppio::Message outgoing;
					outgoing << (uint32_t)MessageType::Control;
					outgoing << writer.write(response);

					proto.sendMessage(outgoing);
				}
			}
		}

		void handleMessage(const cppio::Message& incoming, cppio::MessageProtocol& proto)
		{
			int messageType = incoming.get<uint32_t>(0);
			if(messageType == (int)MessageType::Control)
			{
				Json::Value root;
				Json::Reader reader;
				if(!reader.parse(incoming.get<std::string>(1), root))
					BOOST_THROW_EXCEPTION(ProtocolError() << errinfo_str("Unable to parse incoming JSON: " + reader.getFormattedErrorMessages()));

				Json::Value command = root["command"];
				if(!command.isNull())
				{
					if(command.asString() == "get-identity")
					{
						identity = impl->generateNewIdentity();

						Json::Value response;
						response["identity"] = identity;
						Json::FastWriter writer;
						cppio::Message outgoing;
						outgoing << (uint32_t)MessageType::Control;
						outgoing << writer.write(response);

						proto.sendMessage(outgoing);
					}
				}
				else
				{
					Json::Value order = root["order"];
					if(!order.isNull())
					{
						if(identity.empty())
						{
							BOOST_THROW_EXCEPTION(ProtocolError() << errinfo_str("No identity is set"));
						}
						else
						{
							auto orderObject = deserializeOrder(order);

							{
								boost::unique_lock<boost::mutex> lock(orderListMutex);
								auto it = std::find_if(clientOrders.begin(), clientOrders.end(), [&](const Order::Ptr& other) { return other->clientAssignedId() == orderObject->clientAssignedId();});
								if(it != clientOrders.end())
									BOOST_THROW_EXCEPTION(ProtocolError() << errinfo_str("Order with given id already exists: " + std::to_string(orderObject->clientAssignedId())));
								clientOrders.push_back(orderObject);
							}

							Json::Value response;
							response["result"] = "success";

							Json::FastWriter writer;
							cppio::Message outgoing;
							outgoing << (uint32_t)MessageType::Control;
							outgoing << writer.write(response);

							proto.sendMessage(outgoing);

							impl->submitOrder(orderObject);
						}
					}
					else if(!root["cancel-order"].isNull())
					{
						if(identity.empty())
						{
							BOOST_THROW_EXCEPTION(ProtocolError() << errinfo_str("No identity is set"));
						}
						else
						{
							int clientAssignedId = root["cancel-order"]["id"].asInt();
							std::vector<Order::Ptr>::iterator it = clientOrders.end();
							{
								boost::unique_lock<boost::mutex> lock(orderListMutex);
								it = std::find_if(clientOrders.begin(), clientOrders.end(), [&](const Order::Ptr& other) { return other->clientAssignedId() == clientAssignedId;});
								if(it == clientOrders.end())
									BOOST_THROW_EXCEPTION(ProtocolError() << errinfo_str("Order with given id does not exists: " + std::to_string(clientAssignedId)));
							}

							Json::Value response;
							response["result"] = "success";
							Json::FastWriter writer;
							cppio::Message outgoing;
							outgoing << (uint32_t)MessageType::Control;
							outgoing << writer.write(response);

							proto.sendMessage(outgoing);
							impl->cancelOrder(*it);
						}
					}
				}
			}
		}


		void tradeNotification(const Order::Ptr& order, const Trade& trade)
		{
			Json::Value root;
			serializeTrade(trade, root);

			Json::FastWriter writer;
			cppio::Message message;
			message << (uint32_t)MessageType::Control;
			message << writer.write(root);

			cppio::MessageProtocol proto(line.get());
			proto.sendMessage(message);

			order->setExecutedQuantity(order->executedQuantity() + trade.quantity);

			if(order->executedQuantity() == order->quantity())
			{
				boost::unique_lock<boost::mutex> lock(orderListMutex);
				auto it = std::find(clientOrders.begin(), clientOrders.end(), order);
				if(it != clientOrders.end())
				{
					clientOrders.erase(it);
					retiredOrders.push_back(order);
				}
				order->updateState(Order::State::Executed);
			}
			else if(order->executedQuantity() < order->quantity())
			{
				order->updateState(Order::State::PartiallyExecuted);
			}
			else
			{
				order->updateState(Order::State::Error);
			}

			orderStateChanged(order);
		}

		Order::Ptr findOrderById(int id)
		{
			boost::unique_lock<boost::mutex> lock(orderListMutex);
			auto it = std::find_if(clientOrders.begin(), clientOrders.end(), [=](const Order::Ptr& order)
					{
						return order->localId() == id;
					});
			if(it == clientOrders.end())
				return Order::Ptr();
			return *it;
		}

		Order::Ptr deserializeOrder(const Json::Value& order)
		{
			int id = order["id"].asInt();
			auto account = order["account"].asString();
			auto security = order["security"].asString();
			auto price = order["price"].asDouble();
			auto quantity = order["quantity"].asInt();
			auto operationString = order["operation"].asString();
			auto typeString = order["type"].asString();

			auto strategyId = order["strategy"].asString();
			auto signalId = order["signal-id"].asString();
			auto comment = order["comment"].asString();

			Order::Operation operation;
			if(operationString == "buy")
				operation = Order::Operation::Buy;
			else if(operationString == "sell")
				operation = Order::Operation::Sell;
			else
				BOOST_THROW_EXCEPTION(ParameterError() << errinfo_str("Unknown operation specified: " + operationString));

			Order::OrderType type;
			if(typeString == "market")
				type = Order::OrderType::Market;
			else if(typeString == "limit")
			{
				type = Order::OrderType::Limit;
				if(order["price"].isNull())
					BOOST_THROW_EXCEPTION(ParameterError() << errinfo_str("No price specified for limit order"));
			}
			else
				BOOST_THROW_EXCEPTION(ParameterError() << errinfo_str("Unknown order type specified: " + typeString));

			auto brokerOrder = std::make_shared<Order>(id, account, security, price, quantity, operation, type);
			brokerOrder->setSignalId(SignalId {strategyId, signalId, comment});
			return brokerOrder;
		}

		bool ownsOrder(const Order::Ptr& order)
		{
			return findOrderById(order->localId()) != nullptr;
		}

		void orderStateChanged(const Order::Ptr& order)
		{
			Json::Value orderJson;
			orderJson["id"] = order->clientAssignedId();
			orderJson["new-state"] = serializeOrderState(order->state());;
			Json::Value root;
			root["order"] = orderJson;

			Json::FastWriter writer;
			cppio::Message message;
			message << (uint32_t)MessageType::Control;
			message << writer.write(root);

			cppio::MessageProtocol proto(line.get());
			proto.sendMessage(message);
		}


	private:
		std::shared_ptr<cppio::IoLine> line;
		std::vector<Order::Ptr> clientOrders;
		std::vector<Order::Ptr> retiredOrders;
		boost::mutex orderListMutex;
		Impl* impl;
		boost::thread thread;
		bool run;
		std::string identity;
	};

	Impl(const std::shared_ptr<cppio::IoLineManager>& m,
			const std::string& ep) :
		manager(m), endpoint(ep),
		run(false)
	{
	}

	~Impl()
	{
	}

	std::vector<Broker::Ptr> brokers;
	std::shared_ptr<cppio::IoLineManager> manager;
	std::string endpoint;
	boost::thread mainThread;
	boost::thread tradeSinkThread;
	bool run;
	std::vector<Client::Ptr> clients;
	boost::uuids::random_generator uuidGenerator;
	std::string tradesSinkEndpoint;

	boost::mutex tradeQueueMutex;
	boost::condition_variable tradeQueueCv;
	std::queue<Trade> tradeQueue;

	void eventLoop()
	{
		run = true;
		auto acceptor = std::unique_ptr<cppio::IoAcceptor>(manager->createServer(endpoint));
		if(!acceptor)
			throw std::runtime_error("Unable to bind acceptor to endpoint: " + endpoint);

		tradeSinkThread = boost::thread(std::bind(&Impl::tradeSink, this));

		while(run)
		{
			auto line = std::shared_ptr<cppio::IoLine>(acceptor->waitConnection(100));
			if(line)
			{
				int timeout = 200;
				line->setOption(cppio::LineOption::ReceiveTimeout, &timeout);
				auto client = std::make_shared<Client>(line, this);
				client->start();
				clients.push_back(client);
			}
		}
	}

	void tradeSink()
	{
		while(run)
		{
			std::unique_ptr<cppio::IoLine> tradesSink(manager->createClient(tradesSinkEndpoint));
			if(tradesSink)
			{
				cppio::MessageProtocol proto(tradesSink.get());
				while(run)
				{
					Trade trade;
					{
						boost::unique_lock<boost::mutex> lock(tradeQueueMutex);
						while(trade.orderId == 0)
						{
							if(!run)
								return;

							if(tradeQueue.size() > 0)
							{
								trade = tradeQueue.front();
								tradeQueue.pop();
							}
							else
							{
								tradeQueueCv.wait_for(lock, boost::chrono::milliseconds(1000));
								continue;
							}
						}
					}
					if(trade.orderId != 0)
					{
						Json::Value root;
						serializeTrade(trade, root);

						Json::FastWriter writer;
						cppio::Message message;
						message << writer.write(root);

						size_t rc = proto.sendMessage(message);
						if(rc < 0)
							break;
						printf("Sinking trade\n");
					}
				}
			}
		}
	}

	void setTradeSink(const std::string& endpoint)
	{
		tradesSinkEndpoint = endpoint;
	}

	void sendTradeToSink(const Trade& trade)
	{
		boost::unique_lock<boost::mutex> lock(tradeQueueMutex);
		tradeQueue.push(trade);
		tradeQueueCv.notify_one();
	}

	static void serializeTrade(const Trade& trade, Json::Value& root)
	{
		Json::Value tradeJson;
		tradeJson["order-id"] = trade.orderId;
		tradeJson["price"] = trade.price;
		tradeJson["quantity"] = trade.quantity;
		tradeJson["operation"] = trade.operation == Order::Operation::Buy ? "buy" : "sell";
		tradeJson["volume"] = trade.volume;
		tradeJson["volume-currency"] = trade.volumeCurrency;
		tradeJson["account"] = trade.account;
		tradeJson["security"] = trade.security;
		tradeJson["execution-time"] = serializeExecutionTime(trade.timestamp, trade.useconds);
		if(!trade.signalId.strategyId.empty())
			tradeJson["strategy"] = trade.signalId.strategyId;
		if(!trade.signalId.signalId.empty())
			tradeJson["signal-id"] = trade.signalId.signalId;
		if(!trade.signalId.comment.empty())
			tradeJson["order-comment"] = trade.signalId.comment;

		root.clear();
		root["trade"] = tradeJson;
	}

	void submitOrder(const Order::Ptr& order)
	{
		for(const auto& broker : brokers)
		{
			if(broker->hasAccount(order->account()))
				broker->submitOrder(order);
		}
	}

	void cancelOrder(const Order::Ptr& order)
	{
		for(const auto& broker : brokers)
		{
			if(broker->hasAccount(order->account()))
				broker->cancelOrder(order);
		}
	}

	virtual void orderCallback(const Order::Ptr& order) override
	{
		for(const auto& client : clients)
		{
			if(client->ownsOrder(order))
				client->orderStateChanged(order);
		}
	}

	virtual void tradeCallback(const Trade& trade) override
	{
		for(const auto& client : clients)
		{
			auto order = client->findOrderById(trade.orderId);
			if(order)
			{
				Trade t(trade);
				t.orderId = order->clientAssignedId();
				t.signalId = order->signalId();
				client->tradeNotification(order, t);
				break;
			}
		}

		sendTradeToSink(trade);
	}

	std::string generateNewIdentity()
	{
		return boost::uuids::to_string(uuidGenerator());
	}
};

BrokerServer::BrokerServer(const std::shared_ptr<cppio::IoLineManager>& manager, const std::string& endpoint) :
	m_impl(new Impl(manager, endpoint))
{
}

BrokerServer::~BrokerServer()
{
}

void BrokerServer::registerBroker(const Broker::Ptr& broker)
{
	m_impl->brokers.push_back(broker);
	broker->registerReactor(m_impl);
}

void BrokerServer::unregisterBroker(const Broker::Ptr& broker)
{
	auto it = std::find(m_impl->brokers.begin(), m_impl->brokers.end(), broker);
	if(it != m_impl->brokers.end())
		m_impl->brokers.erase(it);
}

void BrokerServer::start()
{
	m_impl->mainThread = boost::thread(std::bind(&Impl::eventLoop, m_impl.get()));
}

void BrokerServer::stop()
{
	m_impl->run = false;
	for(const auto& client : m_impl->clients)
	{
		client->stop();
	}

	for(const auto& client : m_impl->clients)
	{
		client->join();
	}
	m_impl->brokers.clear();

	if(m_impl->mainThread.joinable())
		m_impl->mainThread.join();
}

void BrokerServer::setTradeSink(const std::string& endpoint)
{
	m_impl->setTradeSink(endpoint);
}

}

