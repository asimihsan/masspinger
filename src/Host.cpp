// ---------------------------------------------------------------------------
// Copyright (c) 2011 Asim Ihsan (asim dot ihsan at gmail dot com)
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
// ---------------------------------------------------------------------------

#include "Host.hpp"

Host::Host(boost::asio::io_service& io_service, std::string& host, log4cxx::LoggerPtr logger)
    : num_replies(0),
      sequence_number(0)
{
    timer = shared_ptr_deadline_timer(new boost::asio::deadline_timer(io_service));

    icmp::resolver resolver(io_service);
    icmp::resolver::query query(icmp::v4(), host.c_str(), "");
    destination = *resolver.resolve(query);                

    std::stringstream destination_address;        
    destination_address << destination.address();
    host_ip_address = destination_address.str();   
    LOG4CXX_DEBUG(logger, "host constructor.  host_ip_address: " << host_ip_address);    
} // Host::Host(boost::asio::io_service& io_service, std::string& host, log4cxx::LoggerPtr logger)

