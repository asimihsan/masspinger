// ---------------------------------------------------------------------------
// Copyright (c) 2011 Asim Ihsan (asim dot ihsan at gmail dot com)
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
// ---------------------------------------------------------------------------

#include "Pinger.hpp"

Pinger::Pinger(boost::asio::io_service& io_service, std::vector< std::string >& hosts, log4cxx::LoggerPtr logger)
        : socket(io_service, icmp::v4()),
          logger(logger)
{
    BOOST_FOREACH( std::string host, hosts )
    {
        boost::shared_ptr<Host> h1 = boost::shared_ptr<Host>(new Host (io_service, host, logger));
        hosts_lookup[h1->get_host_ip_address()] = h1;
        LOG4CXX_DEBUG(logger, "Pinger host: " << h1->get_host_ip_address());
        start_send(h1);            
    } // BOOST_FOREACH( std::string host, hosts )
    start_receive();
} // Pinger::Pinger(boost::asio::io_service& io_service, std::vector< std::string >& hosts, log4cxx::LoggerPtr logger)

Pinger::~Pinger()
{
    socket.close();
} // Pinger::~Pinger()

void Pinger::start_receive()
{
    // Discard any data already in the buffer.
    reply_buffer.consume(reply_buffer.size());

    // Wait for a reply. We prepare the buffer to receive up to 64KB.   
    socket.async_receive(reply_buffer.prepare(65536),
                         boost::bind(&Pinger::handle_receive, this, _2));
} // void Pinger::start_receive()

void Pinger::handle_receive(std::size_t length)
{
    // The actual number of bytes received is committed to the buffer so that we
    // can extract it using a std::istream object.
    reply_buffer.commit(length);

    // Decode the reply packet.
    std::istream is(&reply_buffer);
    ipv4_header ipv4_hdr;
    icmp_header icmp_hdr;
    is >> ipv4_hdr >> icmp_hdr;

    // We can receive all ICMP packets received by the host, so we need to
    // filter out only the echo replies that match the our identifier and
    // expected sequence number.
    std::stringstream ost_ipv4_address;    
    ost_ipv4_address << ipv4_hdr.source_address();            

    LOG4CXX_DEBUG(logger, "ICMP ping from " << ipv4_hdr.source_address()
                          << ", icmp_seq=" << icmp_hdr.sequence_number());

    if (is && icmp_hdr.type() == icmp_header::echo_reply
           && hosts_lookup.find(ost_ipv4_address.str()) != hosts_lookup.end()
           && icmp_hdr.identifier() == get_identifier())
           //&& icmp_hdr.sequence_number() == sequence_number_)
    {
        //std::cout << "Handling source address: " << ipv4_hdr.source_address() << std::endl;
        //std::cout << "ICMP destination: " << destination_.address() << std::endl;    
        //boost::shared_ptr<Host> matching_host = hosts_lookup.find

        // Print out some information about the reply packet.
        posix_time::ptime now = posix_time::microsec_clock::universal_time();
        LOG4CXX_DEBUG(logger, length - ipv4_hdr.header_length()
                              << " bytes from " << ipv4_hdr.source_address()
                              << ": icmp_seq=" << icmp_hdr.sequence_number()
                              << ", ttl=" << ipv4_hdr.time_to_live());
    }
    start_receive();
} // void Pinger::handle_receive(std::size_t length)    

void Pinger::start_send(boost::shared_ptr<Host> host)
{
    LOG4CXX_DEBUG(logger, "start_send.  host: " << host->get_host_ip_address());    
    std::string body("\"Hello!\" from Asio ping.");

    // Create an ICMP header for an echo request.
    icmp_header echo_request;
    echo_request.type(icmp_header::echo_request);
    echo_request.code(0);
    echo_request.identifier(get_identifier());
    echo_request.sequence_number(host->get_sequence_number());
    host->increment_sequence_number();
    compute_checksum(echo_request, body.begin(), body.end());

    // Encode the request packet.
    boost::asio::streambuf request_buffer;
    std::ostream os(&request_buffer);
    os << echo_request << body;

    // Send the request.
    host->set_time_sent(posix_time::microsec_clock::universal_time());
    socket.send_to(request_buffer.data(), host->get_destination());

    if (shared_ptr_deadline_timer timer = host->get_deadline_timer().lock())
    {
        timer->expires_at(host->get_time_sent() + posix_time::seconds(1));
        timer->async_wait(boost::bind(&Pinger::start_send, this, host));
    } // if (shared_ptr_deadline_timer timer = host->get_deadline_timer().lock())
    else
    {
        LOG4CXX_ERROR(logger, "Cannot get deadline timer for host: " << host->get_host_ip_address());
    }
    
} // void Pinger::start_send(boost::shared_ptr<Host> host)
  
unsigned short Pinger::get_identifier()
{
    #if defined(BOOST_WINDOWS)
        return static_cast<unsigned short>(::GetCurrentProcessId());
    #else
        return static_cast<unsigned short>(::getpid());
    #endif
} // unsigned short Pinger::get_identifier()  
