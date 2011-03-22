// ---------------------------------------------------------------------------
// Copyright (c) 2011 Asim Ihsan (asim dot ihsan at gmail dot com)
// Distributed under the MIT/X11 software license, see the accompanying
// file license.txt or http://www.opensource.org/licenses/mit-license.php.
// ---------------------------------------------------------------------------

#include "Pinger.hpp"

Pinger::Pinger(boost::asio::io_service& io_service,
               std::vector< std::string >& hosts,
               std::vector< std::string >& zeromq_binds,
               log4cxx::LoggerPtr logger)
        : socket(io_service, icmp::v4()),
          logger(logger),
          is_network_unreachable(false)
{

    // -----------------------------------------------------------------------
    //  Set up ZeroMQ publishing on all bindings.
    // -----------------------------------------------------------------------
    context_ptr = zmq_context_ptr_t(new zmq::context_t(1));
    publisher_ptr = zmq_socket_ptr_t(new zmq::socket_t(*context_ptr, ZMQ_PUB));
    BOOST_FOREACH( std::string& zeromq_bind, zeromq_binds )
    {
        LOG4CXX_INFO(logger, "Publishing ICMP PING results to ZeroMQ address: " << zeromq_bind.c_str());
        zmq_bind(*publisher_ptr, zeromq_bind.c_str());
    } // BOOST_FOREACH( std::string& zeromq_bind, zeromq_binds )
    // -----------------------------------------------------------------------

    // -----------------------------------------------------------------------
    //  Start an asynchronous, infinite send for each host we're monitoring.
    // -----------------------------------------------------------------------
    BOOST_FOREACH( std::string& host, hosts )
    {
        boost::shared_ptr<Host> h1 = boost::shared_ptr<Host>(new Host (io_service, host, logger));
        LOG4CXX_DEBUG(logger, "Pinger host: " << h1->get_host_ip_address());
        hosts_lookup[h1->get_host_ip_address()] = h1;        

        shared_ptr_deadline_timer unresponsive_timer = h1->get_unresponsive_timer().lock();
        LOG4CXX_ASSERT(logger, unresponsive_timer, "Could not get host's unresponsive_timer.");
        unresponsive_timer->expires_from_now(posix_time::seconds(5));
        unresponsive_timer->async_wait(boost::bind(&Pinger::set_host_unresponsive, this, h1, boost::asio::placeholders::error));

        start_send(h1);            
    } // BOOST_FOREACH( std::string host, hosts )
    // -----------------------------------------------------------------------

    start_receive();
} // Pinger::Pinger(boost::asio::io_service& io_service, std::vector< std::string >& hosts, log4cxx::LoggerPtr logger)

Pinger::~Pinger()
{
    socket.close();
} // Pinger::~Pinger()

void Pinger::set_host_responsive(boost::shared_ptr<Host> host)
{
    host->set_responsive();
} // Pinger::set_host_responsive(boost::shared_ptr<Host> host)

void Pinger::set_host_unresponsive(boost::shared_ptr<Host> host, const boost::system::error_code& error)
{
    host->set_unresponsive(error);
} // Pinger::set_host_unresponsive(boost::shared_ptr<Host> host)

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
    // filter out only the echo replies that match the our identifier.
    std::stringstream ost_ipv4_address;    
    ost_ipv4_address << ipv4_hdr.source_address();            
    LOG4CXX_DEBUG(logger, "ICMP ping from " << ipv4_hdr.source_address()
                          << ", icmp_seq=" << icmp_hdr.sequence_number());
    if (is && icmp_hdr.type() == icmp_header::echo_reply
           && hosts_lookup.find(ost_ipv4_address.str()) != hosts_lookup.end()
           && icmp_hdr.identifier() == get_identifier())
           //&& icmp_hdr.sequence_number() == sequence_number_)
    {
        // -------------------------------------------------------------------
        // Reset the host's unresponsive timer, and mark it responsive.
        // -------------------------------------------------------------------
        boost::shared_ptr<Host> matching_host = hosts_lookup.find(ost_ipv4_address.str())->second;
        matching_host->set_responsive();
        shared_ptr_deadline_timer unresponsive_timer = matching_host->get_unresponsive_timer().lock();
        LOG4CXX_ASSERT(logger, unresponsive_timer, "Could not get host's unresponsive_timer.");
        unresponsive_timer->cancel();
        unresponsive_timer->expires_from_now(posix_time::seconds(5));
        unresponsive_timer->async_wait(boost::bind(&Pinger::set_host_unresponsive, this, matching_host, boost::asio::placeholders::error));
        // -------------------------------------------------------------------

        // Log some information about the reply packet.        
        LOG4CXX_DEBUG(logger, length - ipv4_hdr.header_length()
                              << " bytes from " << ipv4_hdr.source_address()
                              << ": icmp_seq=" << icmp_hdr.sequence_number()
                              << ", ttl=" << ipv4_hdr.time_to_live());                  
         
         // ------------------------------------------------------------------
         // Publish a ZeroMQ message corresponding to receiving this event.
         //
         // Reference:
         // http://stackoverflow.com/questions/1374468/c-stringstream-string-and-char-conversion-confusion
         // ------------------------------------------------------------------
         std::stringstream event_stream;
         if (matching_host->get_is_hostname_available())
         {
             // IP address and hostname.
             event_stream << "ICMP PING " << matching_host->get_hostname() << " (" << ipv4_hdr.source_address() << ")";
         }
         else
         {
             // only IP address.
             event_stream << "ICMP PING " << ipv4_hdr.source_address();
         } // if (matching_host->is_hostname_available)s
         
         const std::string& event_string = event_stream.str();
         const char* event_cstr = event_string.c_str();
         zmq::message_t message(strlen(event_cstr));
         strncpy((char *)message.data(), event_cstr, strlen(event_cstr));
         publisher_ptr->send(message);         
         // ------------------------------------------------------------------
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

    // -----------------------------------------------------------------------
    // -  Send the next packet after the send interval period passes.
    // -----------------------------------------------------------------------
    shared_ptr_deadline_timer send_timer = host->get_send_timer().lock();
    shared_ptr_deadline_timer unresponsive_timer = host->get_unresponsive_timer().lock();
    if (send_timer && unresponsive_timer)
    {
        send_timer->expires_from_now(posix_time::seconds(1));
        send_timer->async_wait(boost::bind(&Pinger::start_send, this, host));
    } // if (shared_ptr_deadline_timer timer = host->get_deadline_timer().lock())
    else
    {
        LOG4CXX_ERROR(logger, "Cannot get send and/or unresponsive timer for host: " << host->get_host_ip_address());
    }
    
    // Send the request.
    host->set_time_sent(posix_time::microsec_clock::universal_time());
    try
    {
        socket.send_to(request_buffer.data(), host->get_destination());
        is_network_unreachable = false;
    } // try
    catch(boost::system::system_error& e)
    {        
        if (e.code() == boost::asio::error::network_unreachable)
        {
            if (!is_network_unreachable)
            {
                LOG4CXX_WARN(logger, "Network is unreachable.");
                is_network_unreachable = true;
            } // if (!is_network_unreachable)
        } // if (e.code == boost::asio::error::network_unreachable)
        else
        {
            LOG4CXX_WARN(logger, "Unhandled exception during socket send.");
            throw;
        }
    } // catch(boost::system::system_error& e)

} // void Pinger::start_send(boost::shared_ptr<Host> host)
  
unsigned short Pinger::get_identifier()
{
    #if defined(BOOST_WINDOWS)
        return static_cast<unsigned short>(::GetCurrentProcessId());
    #else
        return static_cast<unsigned short>(::getpid());
    #endif
} // unsigned short Pinger::get_identifier()  
