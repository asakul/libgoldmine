
#include "iolinemanager.h"

#include <vector>

namespace goldmine
{
namespace io
{

struct IoLineManager::Impl
{
	std::vector<std::unique_ptr<IoLineFactory>> factories;
};

IoLineManager::IoLineManager() : m_impl(new Impl)
{
}

IoLineManager::~IoLineManager()
{
}

std::shared_ptr<IoLine> IoLineManager::createClient(const std::string& address)
{
	auto delimiter = address.find_first_of("://");
	if(delimiter == std::string::npos)
		return std::shared_ptr<IoLine>();

	auto scheme = address.substr(0, delimiter);
	for(const auto& factory : m_impl->factories)
	{
		if(factory->supportsScheme(scheme))
		{
			auto baseAddress = address.substr(delimiter + 3);
			return factory->createClient(baseAddress);
		}
	}
	return std::shared_ptr<IoLine>();
}

std::shared_ptr<IoAcceptor> IoLineManager::createServer(const std::string& address)
{
	auto delimiter = address.find_first_of("://");
	if(delimiter == std::string::npos)
		return std::shared_ptr<IoAcceptor>();

	auto scheme = address.substr(0, delimiter);
	for(const auto& factory : m_impl->factories)
	{
		if(factory->supportsScheme(scheme))
		{
			auto baseAddress = address.substr(delimiter + 3);
			return factory->createServer(baseAddress);
		}
	}
	return std::shared_ptr<IoAcceptor>();
}


void IoLineManager::registerFactory(std::unique_ptr<IoLineFactory> factory)
{
	return m_impl->factories.push_back(std::move(factory));
}

}
}

