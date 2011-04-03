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
#include <boost/exception/all.hpp> 
namespace posix_time = boost::posix_time;

#include <log4cxx/logger.h>

#include <zmq.hpp>    
#include <cstdio>
#if BOOST_WINDOWS
    #define snprintf sprintf_s 
#endif 
#include <yaml.h>

#include <string>
#include <set>

#include "icmp_header.hpp"
#include "ipv4_header.hpp"
#include "Host.hpp"

typedef boost::shared_ptr<zmq::context_t> zmq_context_ptr_t;
typedef boost::shared_ptr<zmq::socket_t> zmq_socket_ptr_t;

class Pinger
{
public:
    Pinger(boost::asio::io_service& io_service,
           std::vector< std::string >& hosts,
           std::vector< std::string >& zeromq_binds,
           log4cxx::LoggerPtr logger);
    ~Pinger();
    
private:
    std::map< std::string, boost::shared_ptr<Host> > hosts_lookup;    
    icmp::socket socket;
    boost::asio::streambuf reply_buffer;

    log4cxx::LoggerPtr logger;

    zmq_context_ptr_t context_ptr;
    zmq_socket_ptr_t publisher_ptr;

    void start_receive();

    void handle_receive(std::size_t length);

    void start_send(boost::shared_ptr<Host> host);

    unsigned short get_identifier();

    void set_host_responsive(boost::shared_ptr<Host> host);

    void set_host_unresponsive(boost::shared_ptr<Host> host, const boost::system::error_code& error);

    bool is_network_unreachable;
  
}; // class Pinger

#endif // PINGER_HPP