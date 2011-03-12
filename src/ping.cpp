// ---------------------------------------------------------------------------
// Copyright (c) 2011 Asim Ihsan (asim dot ihsan at gmail dot com)
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Based on the ICMP example from the Boost ASIO library, with the following
// copyright header:
//
// Copyright (c) 2003-2011 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
// ---------------------------------------------------------------------------

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/program_options.hpp>
#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/exception/all.hpp> 
namespace po = boost::program_options;

#include <stdlib.h>
#include <log4cxx/logger.h>
#include <log4cxx/consoleappender.h>
#include <log4cxx/simplelayout.h>
#include <log4cxx/logmanager.h>
#include <iostream>
#include <locale.h>

#include <istream>
#include <iostream>
#include <ostream>
#include <sstream>
#include <vector>
#include <string>
#include <set>
#include <cstdlib>

// ---------------------------------------------------------------------------
//  Includes for signal handling.
// ---------------------------------------------------------------------------
#if !defined(BOOST_WINDOWS)
    #include <signal.h>
#else
    #include <windows.h>
#endif
// ---------------------------------------------------------------------------

#include "Host.hpp"
#include "Pinger.hpp"

/**
 *   Configures console appender.
 *   @param err if true, use stderr, otherwise stdout.
 */
static void configure_logger(bool err) {
    log4cxx::ConsoleAppenderPtr appender(new log4cxx::ConsoleAppender());
    if (err) {
        appender->setTarget(LOG4CXX_STR("System.err"));
    }
    log4cxx::LayoutPtr layout(new log4cxx::SimpleLayout());
    appender->setLayout(layout);
    log4cxx::helpers::Pool pool;
    appender->activateOptions(pool);
    log4cxx::Logger::getRootLogger()->addAppender(appender);
    log4cxx::LogManager::getLoggerRepository()->setConfigured(true);
}

#if !defined(BOOST_WINDOWS)
    void stop(int s)
    {
        std::cout << "CTRL-C" << std::endl;
        s = s;
        exit(0);
    }
#else
    BOOL CtrlHandler(DWORD fdwCtrlType)
    {
        switch(fdwCtrlType)
        {
        case CTRL_C_EVENT:
            std::cout << "CTRL-C" << std::endl;
            exit(0);
        default:
            return FALSE;
        } // switch(fdwCtrlType)
    } // BOOL CtrlHandler(DWORD fdwCtrlType)
#endif

int main(int argc, char* argv[])
{
    configure_logger(false);
    log4cxx::LoggerPtr logger = log4cxx::Logger::getRootLogger();

    std::vector<std::string> hosts;
    try
    {
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help", "produce help message")
            ("host,H", po::value< std::vector<std::string> >(), "host")
            ("verbose,V", "verbose debug output");  

        po::positional_options_description positional_desc;
        positional_desc.add("host", -1);

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(desc).positional(positional_desc).run(),
                  vm);
        po::notify(vm);

        if (vm.count("help")) {
            std::cout << "Usage: ping [host1] [host2] ..." << std::endl;
            std::cout << desc;
            return 0;
        }

        logger->setLevel(log4cxx::Level::getInfo());
        if (vm.count("verbose"))
        {
            std::cout << "Verbose debug mode enabled." << std::endl;
            logger->setLevel(log4cxx::Level::getTrace());
        }

        if (!vm.count("host")) {
            std::cout << "Need to specify at least one host." << std::endl;
            std::cout << desc;
            return 1;      
        } else {
            hosts = vm["host"].as< std::vector<std::string> >();
            std::cout << "Hosts are: ";
            BOOST_FOREACH( std::string host, hosts)
            {
                std::cout << host << " ";
            }
            std::cout << std::endl;    
        }

        // ---------------------------------------------------------------------------
        //  Register signal handler for CTRL-C, that will simply call exit().  This
        //  is primarily here to allow GCC profile guided optimisation (PGO) to work,
        //  as this requires the program under instrumentation to cleanly quit.
        //
        //  While we're at it support it on Windows as well; however Microsoft Visual
        //  Studio's (MSVC) PGO is smart enough to handle CTRL-C's.
        //
        //  References:
        //  - http://msdn.microsoft.com/en-us/library/ms685049%28VS.85%29.aspx
        //  - http://stackoverflow.com/questions/1641182/how-can-i-catch-a-ctrl-c-event-c
        // ---------------------------------------------------------------------------
        #if !defined(BOOST_WINDOWS)
            struct sigaction sig_int_handler;
            sig_int_handler.sa_handler = stop;
            sigemptyset(&sig_int_handler.sa_mask);
            sig_int_handler.sa_flags = 0;
            sigaction(SIGINT, &sig_int_handler, NULL);
        #else
            SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
        #endif
        // ---------------------------------------------------------------------------

        boost::asio::io_service io_service;
        boost::shared_ptr<Pinger> ping_receiver(new Pinger(io_service, hosts, logger));
        io_service.run();
    }
    catch (boost::exception& e)
    {
        LOG4CXX_FATAL(logger, "Main exception: " << boost::diagnostic_information(e));        
    }
}
