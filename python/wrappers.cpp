
#include <boost/python.hpp>
#include <boost/shared_ptr.hpp>
#include "quotesource/quotesource.h"
#include "quotesource/quotesourceclient.h"
#include "cppio/iolinemanager.h"
#include "goldmine/exceptions.h"

using namespace boost::python;
using namespace goldmine;

std::shared_ptr<cppio::IoLineManager> makeIoLineManager()
{
	return cppio::createLineManager();
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
}

