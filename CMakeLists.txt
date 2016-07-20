cmake_minimum_required(VERSION 3.0.0 FATAL_ERROR)

project(goldmine)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/3rdparty/cmake)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(WORKSPACE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)


set (Boost_USE_STATIC_LIBS OFF)
set (Boost_USE_MULTITHREADED ON)

add_definitions(-DBOOST_THREAD_USE_LIB)

if(WIN32)
find_package(Boost COMPONENTS system thread_win32-mt date_time chrono REQUIRED)
else(WIN32)
set(Python_ADDITIONAL_VERSIONS 2.7)
FIND_PACKAGE(PythonInterp)
FIND_PACKAGE(PythonLibs)

find_package(Boost COMPONENTS system thread date_time chrono REQUIRED python)
endif(WIN32)

if(WIN32)
	set(PLATFORM_CXX_FLAGS -mwindows)
endif(WIN32)

find_package(Threads)

SET(CMAKE_CXX_FLAGS  "-Wall -fno-strict-aliasing -fexceptions -g --std=gnu++11 -mtune=generic ${PLATFORM_CXX_FLAGS} -fno-omit-frame-pointer")

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/catch)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/jsoncpp)

set(goldmine-sources
		${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/jsoncpp/jsoncpp.cpp

		broker/broker.cpp
		broker/brokerclient.cpp
		broker/brokerserver.cpp

		quotesource/quotesource.cpp
		quotesource/quotesourceclient.cpp
	)

add_library(goldmine SHARED ${goldmine-sources})
target_link_libraries(goldmine ${Boost_LIBRARIES} ${PYTHON_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} -L../libcppio -lcppio)

if(WIN32)
target_link_libraries(goldmine -lws2_32)
endif(WIN32)

if(Boost_PYTHON_FOUND)

include_directories(${PYTHON_INCLUDE_DIRS})

message("Python bindings enabled")
add_library(pygoldmine SHARED ${goldmine-sources} python/wrappers.cpp)
target_link_libraries(pygoldmine ${Boost_LIBRARIES} ${PYTHON_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} -L../libcppio -lcppio)
if(WIN32)
	target_link_libraries(pygoldmine -lwsock32 -lws2_32)
endif(WIN32)
set_target_properties(pygoldmine PROPERTIES PREFIX "")

else(Boost_PYTHON_FOUND)
message("Python bindings are NOT enabled")
endif(Boost_PYTHON_FOUND)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/../libcppio/include
	${CMAKE_CURRENT_SOURCE_DIR}/3rdparty
	${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/catch
	${CMAKE_CURRENT_BINARY_DIR}
	)

set(test-sources
		tests/libgoldmine/quotesource_test.cpp
		tests/libgoldmine/quotesourceclient_test.cpp
		tests/libgoldmine/brokerclient_test.cpp
		tests/libgoldmine/brokerserver_test.cpp
	)

add_executable(libgoldmine-tests tests/libgoldmine/tests.cpp
	${test-sources}
	${goldmine-sources})

target_compile_definitions(libgoldmine-tests PRIVATE)

target_link_libraries(libgoldmine-tests ${Boost_LIBRARIES} ${PYTHON_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} -L../libcppio -lcppio)

set_target_properties(libgoldmine-tests PROPERTIES COMPILE_FLAGS "-O0 -g -fprofile-arcs -ftest-coverage")
set_target_properties(libgoldmine-tests PROPERTIES LINK_FLAGS "-fprofile-arcs -lgcov")

if(WIN32)
target_link_libraries(libgoldmine-tests -lws2_32)
endif(WIN32)

add_executable(quotesource-server test-misc/quotesource-server.cpp)
target_link_libraries(quotesource-server ${Boost_LIBRARIES} ${PYTHON_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} -L../libcppio -lcppio goldmine)

add_executable(quotesource-client test-misc/quotesource-client.cpp)
target_link_libraries(quotesource-client ${Boost_LIBRARIES} ${PYTHON_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} -L../libcppio -lcppio goldmine)

add_executable(broker-server test-misc/broker-server.cpp)
target_link_libraries(broker-server ${Boost_LIBRARIES} ${PYTHON_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} -L../libcppio -lcppio goldmine)

add_executable(broker-client test-misc/broker-client.cpp)
target_link_libraries(broker-client ${Boost_LIBRARIES} ${PYTHON_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} -L../libcppio -lcppio goldmine)

include(CodeCoverage)
setup_target_for_coverage(libgoldmine-coverage libgoldmine-tests coverage)

