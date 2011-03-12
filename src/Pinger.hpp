// ---------------------------------------------------------------------------
// Copyright (c) 2011 Asim Ihsan (asim dot ihsan at gmail dot com)
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
// ---------------------------------------------------------------------------

#ifndef PINGER_HPP
#define PINGER_HPP

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>
namespace posix_time = boost::posix_time;

#include <log4cxx/logger.h>

#include <string>
#include <set>

#include "icmp_header.hpp"
#include "ipv4_header.hpp"
#include "Host.hpp"

/*
#include <istream>
#include <iostream>
#include <ostream>
#include <sstream>
#include <vector>
#include <string>
#include <set>
#include <cstdlib>
*/

class Pinger
{
public:
    Pinger(boost::asio::io_service& io_service, std::vector< std::string >& hosts, log4cxx::LoggerPtr logger);
    ~Pinger();
    
private:
    std::map< std::string, boost::shared_ptr<Host> > hosts_lookup;    
    icmp::socket socket;
    boost::asio::streambuf reply_buffer;
    log4cxx::LoggerPtr logger;

    void start_receive();

    void handle_receive(std::size_t length);

    void start_send(boost::shared_ptr<Host> host);

    unsigned short get_identifier();
  
}; // class Pinger

#endif // PINGER_HPP