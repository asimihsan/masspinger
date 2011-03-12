// ---------------------------------------------------------------------------
// Copyright (c) 2011 Asim Ihsan (asim dot ihsan at gmail dot com)
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
// ---------------------------------------------------------------------------

#ifndef HOST_HPP
#define HOST_HPP

#include <boost/asio.hpp>
#include <log4cxx/logger.h>

using boost::asio::ip::icmp;

typedef boost::shared_ptr<boost::asio::deadline_timer> shared_ptr_deadline_timer;
typedef boost::weak_ptr<boost::asio::deadline_timer> weak_ptr_deadline_timer;

class Host
{
public:
    Host(boost::asio::io_service& io_service, std::string& host, log4cxx::LoggerPtr logger);

    inline const std::string& get_host_ip_address() { return Host::host_ip_address; }

    inline unsigned short get_sequence_number() { return Host::sequence_number; }
    inline void increment_sequence_number() { Host::sequence_number++; }

    inline const boost::posix_time::ptime& get_time_sent() { return Host::time_sent; }
    inline void set_time_sent(const boost::posix_time::ptime& time_sent) { Host::time_sent = time_sent; }

    inline const icmp::endpoint& get_destination() { return Host::destination; }

    inline weak_ptr_deadline_timer get_deadline_timer() { return weak_ptr_deadline_timer(Host::timer); }

private:
    std::string host_ip_address;
    shared_ptr_deadline_timer timer;    
    unsigned short sequence_number;
    boost::posix_time::ptime time_sent;    
    std::size_t num_replies;    
    icmp::endpoint destination;
}; // class Host

#endif // HOST_HPP
