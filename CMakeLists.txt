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
find_package(Boost COMPONENTS system thread date_time chrono REQUIRED)
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
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/mingw-std-threads)

set(goldmine-sources
		${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/jsoncpp/jsoncpp.cpp

		io/iolinemanager.cpp
		io/message.cpp

		io/common/inproc.cpp

		quotesource/quotesource.cpp
		quotesource/quotesourceclient.cpp
	)

if(WIN32)
	list(APPEND goldmine-sources
		io/win32/win_socket.cpp
		io/win32/pipes.cpp
		)
elseif(UNIX)
	list(APPEND goldmine-sources
		io/posix/io_socket.cpp)
endif(WIN32)

add_library(goldmine SHARED ${goldmine-sources})
target_link_libraries(goldmine ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

if(WIN32)
target_link_libraries(goldmine -lws2_32)
endif(WIN32)

include_directories(
	${CMAKE_CURRENT_SOURCE_DIR}/3rdparty
	${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/catch
	${CMAKE_CURRENT_BINARY_DIR}
	)

set(test-sources
		tests/inproc_test.cpp
		tests/message_test.cpp
		tests/quotesource_test.cpp
		tests/quotesourceclient_test.cpp
	)

add_executable(libgoldmine-tests tests/tests.cpp
	${test-sources}
	${goldmine-sources})

target_compile_definitions(libgoldmine-tests PRIVATE)

target_link_libraries(libgoldmine-tests ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${ZMQ_LIBRARIES})

set_target_properties(libgoldmine-tests PROPERTIES COMPILE_FLAGS "-O0 -g -fprofile-arcs -ftest-coverage")
set_target_properties(libgoldmine-tests PROPERTIES LINK_FLAGS "-fprofile-arcs -lgcov")

if(WIN32)
target_link_libraries(libgoldmine-tests -lws2_32)
endif(WIN32)

set(tests-integration-sources
	tests-integration/inproc_integration_test.cpp
	tests-integration/messageprotocol_test.cpp
	)

if(UNIX)
	list(APPEND tests-integration-sources
		tests-integration/unixsocket_test.cpp
		)
endif(UNIX)

if(WIN32)
	list(APPEND tests-integration-sources
		tests-integration/winsocket_test.cpp
		tests-integration/pipes_test.cpp
		)
endif(WIN32)

add_executable(libgoldmine-integration-tests tests-integration/tests.cpp
	${tests-integration-sources}
	${goldmine-sources})

target_compile_definitions(libgoldmine-tests PRIVATE)

target_link_libraries(libgoldmine-integration-tests ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${ZMQ_LIBRARIES})

set_target_properties(libgoldmine-integration-tests PROPERTIES COMPILE_FLAGS "-O0 -g -fprofile-arcs -ftest-coverage")
set_target_properties(libgoldmine-integration-tests PROPERTIES LINK_FLAGS "-fprofile-arcs -lgcov")

if(WIN32)
target_link_libraries(libgoldmine-integration-tests -lws2_32)
endif(WIN32)


include(CodeCoverage)
setup_target_for_coverage(libgoldmine-coverage libgoldmine-tests libgoldmine-integration-tests coverage)

