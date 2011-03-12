// ---------------------------------------------------------------------------
// Copyright (c) 2011 Asim Ihsan (asim dot ihsan at gmail dot com)
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
// ---------------------------------------------------------------------------

#include "Host.hpp"

Host::Host(boost::asio::io_service& io_service, std::string& host, log4cxx::LoggerPtr logger)
    : logger(logger),
      sequence_number(0),
      is_responsive(true)     
{
    send_timer = shared_ptr_deadline_timer(new boost::asio::deadline_timer(io_service));
    unresponsive_timer = shared_ptr_deadline_timer(new boost::asio::deadline_timer(io_service));

    icmp::resolver resolver(io_service);
    icmp::resolver::query query(icmp::v4(), host.c_str(), "");
    destination = *resolver.resolve(query);                

    std::stringstream destination_address;        
    destination_address << destination.address();
    host_ip_address = destination_address.str();   
    LOG4CXX_DEBUG(logger, "host constructor.  host_ip_address: " << host_ip_address);    
} // Host::Host(boost::asio::io_service& io_service, std::string& host, log4cxx::LoggerPtr logger)

void Host::set_responsive()
{
    if (is_responsive != true)
    {
        LOG4CXX_INFO(logger, "Host " << host_ip_address << " is responsive.");
        is_responsive = true;
    } // if (is_responsive != true)    
} // void Host::set_responsive()

void Host::set_unresponsive(const boost::system::error_code& error)
{
    if ((is_responsive != false) && (error != boost::asio::error::operation_aborted))
    {
        LOG4CXX_INFO(logger, "Host " << host_ip_address << " is unresponsive.");
        is_responsive = false;
    } // if (is_responsive != false)
} // void Host::set_unresponsive()

