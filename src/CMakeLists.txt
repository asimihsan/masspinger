cmake_minimum_required (VERSION 2.8)
project(masspinger)

set(masspinger_VERSION_MAJOR 1)
set(masspinger_VERSION_MINOR 0)

include_directories("$ENV{BOOST_HOME}")
link_directories("$ENV{BOOST_HOME}/stage/lib")
set (EXTRA_LIBS ${EXTRA_LIBS} boost_system)
set (EXTRA_LIBS ${EXTRA_LIBS} boost_program_options)
ADD_DEFINITIONS(-D_WIN32_WINNT=0x0501)

include_directories("../deps/log4cxx/src/main/include")
include_directories("../deps/zeromq/include")

add_executable(masspinger Host.cpp ping.cpp Pinger.cpp)
target_link_libraries(masspinger ${EXTRA_LIBS})
