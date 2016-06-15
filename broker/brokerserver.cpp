
#include "brokerserver.h"

#include "io/iolinemanager.h"
#include "io/message.h"
#include "json/json.h"

#include "exceptions.h"

#include <boost/thread.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

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
	}
	return "unknown";
}


struct BrokerServer::Impl : public Broker::Reactor
{
	class Client
	{
	public:
		using Ptr = std::shared_ptr<Client>;

		Client(const std::shared_ptr<io::IoLine>& ioLine, Impl* i) : line(ioLine),
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
			io::MessageProtocol proto(line);

			while(run)
			{
				try
				{
					io::Message incoming;
					proto.readMessage(incoming);

					handleMessage(incoming, proto);

				}
				catch(const io::IoException& e)
				{
				}
			}
		}

		void handleMessage(const io::Message& incoming, io::MessageProtocol& proto)
		{
			int messageType = incoming.get<uint32_t>(0);
			if(messageType == (int)MessageType::Control)
			{
				Json::Value root;
				Json::Reader reader;
				if(!reader.parse(incoming.get<std::string>(1), root))
					BOOST_THROW_EXCEPTION(ProtocolError() << errinfo_str("Unable to parse incoming JSON"));

				Json::Value command = root["command"];
				if(!command.isNull())
				{
					if(command.asString() == "get-identity")
					{
						identity = impl->generateNewIdentity();

						Json::Value response;
						response["identity"] = identity;
						Json::FastWriter writer;
						io::Message outgoing;
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
							Json::Value response;
							response["result"] = "error";
							response["reason"] = "No identity is set";

							Json::FastWriter writer;
							io::Message outgoing;
							outgoing << (uint32_t)MessageType::Control;
							outgoing << writer.write(response);

							proto.sendMessage(outgoing);
						}
						else
						{
							Json::Value response;
							response["result"] = "success";

							Json::FastWriter writer;
							io::Message outgoing;
							outgoing << (uint32_t)MessageType::Control;
							outgoing << writer.write(response);

							proto.sendMessage(outgoing);

							auto orderObject = deserializeOrder(order);
							clientOrders.push_back(orderObject);
							impl->submitOrder(orderObject);

						}
					}
				}
			}
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
				type = Order::OrderType::Limit;
			else
				BOOST_THROW_EXCEPTION(ParameterError() << errinfo_str("Unknown order type specified: " + typeString));

			return std::make_shared<Order>(id, account, security, price, quantity, operation, type);
		}

		bool ownsOrder(const Order::Ptr& order)
		{
			return std::find_if(clientOrders.begin(), clientOrders.end(), [&](const Order::Ptr& other) { return other->localId() == order->localId(); }) != clientOrders.end();
		}

		void orderStateChanged(const Order::Ptr& order)
		{
			Json::Value orderJson;
			orderJson["id"] = order->clientAssignedId();
			orderJson["new-state"] = serializeOrderState(order->state());;
			Json::Value root;
			root["order"] = orderJson;

			Json::FastWriter writer;
			io::Message message;
			message << (uint32_t)MessageType::Control;
			message << writer.write(root);

			io::MessageProtocol proto(line);
			proto.sendMessage(message);
		}


	private:
		std::shared_ptr<io::IoLine> line;
		std::vector<Order::Ptr> clientOrders;
		Impl* impl;
		boost::thread thread;
		bool run;
		std::string identity;
	};

	Impl(const std::shared_ptr<io::IoLineManager>& m,
			const std::string& ep) :
		manager(m), endpoint(ep),
		run(false)
	{
	}

	~Impl()
	{
	}

	std::vector<Broker::Ptr> brokers;
	std::shared_ptr<io::IoLineManager> manager;
	std::string endpoint;
	boost::thread mainThread;
	bool run;
	std::vector<Client::Ptr> clients;
	boost::uuids::random_generator uuidGenerator;

	void eventLoop()
	{
		run = true;
		auto acceptor = manager->createServer(endpoint);
		while(run)
		{
			auto line = acceptor->waitConnection(std::chrono::milliseconds(100));
			if(line)
			{
				int timeout = 200;
				line->setOption(io::LineOption::ReceiveTimeout, &timeout);
				auto client = std::make_shared<Client>(line, this);
				client->start();
				clients.push_back(client);
			}
		}
	}

	void submitOrder(const Order::Ptr& order)
	{
		for(const auto& broker : brokers)
		{
			if(broker->hasAccount(order->account()))
				broker->submitOrder(order);
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
	}
		
	std::string generateNewIdentity()
	{
		return boost::uuids::to_string(uuidGenerator());
	}
};

BrokerServer::BrokerServer(const std::shared_ptr<io::IoLineManager>& manager, const std::string& endpoint) :
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

}

