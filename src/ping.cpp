//
// ping.cpp
// ~~~~~~~~
//
// Copyright (c) 2003-2011 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/program_options.hpp>
#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>
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

#include "icmp_header.hpp"
#include "ipv4_header.hpp"

using boost::asio::ip::icmp;
using boost::asio::deadline_timer;
namespace posix_time = boost::posix_time;
namespace po = boost::program_options;

class Host
{
public:
    Host(boost::asio::io_service& io_service, std::string& host)
        : timer(io_service)
    {
        icmp::resolver resolver(io_service);
        icmp::resolver::query query(icmp::v4(), host.c_str(), "");
        destination = *resolver.resolve(query);                

        std::stringstream destination_address;        
        destination_address << destination.address();
        host_ip_address = destination_address.str();
        std::cout << "host constructor.  host_ip_address: " << host_ip_address << std::endl;
        num_replies = 0;
        sequence_number = 0;
    }

    std::string host_ip_address;
    boost::asio::deadline_timer timer;    
    unsigned short sequence_number;
    posix_time::ptime time_sent;    
    std::size_t num_replies;    
    icmp::endpoint destination;
}; // class Host

class Pinger
{
public:
    Pinger(boost::asio::io_service& io_service, std::vector< std::string >& hosts)
        : socket(io_service, icmp::v4())
    {
        BOOST_FOREACH( std::string host, hosts )
        {
            boost::shared_ptr<Host> h1 = boost::shared_ptr<Host>(new Host (io_service, host));
            hosts_lookup[h1->host_ip_address] = h1;
            std::cout << "Pinger host: " << h1->host_ip_address << std::endl;            
            start_send(h1);
            start_receive(h1);
        } // BOOST_FOREACH( std::string host, hosts )
    } // constructor

    ~Pinger()
    {
        socket.close();
    } // ~PingReceiver()
    
private:
    std::map< std::string, boost::shared_ptr<Host> > hosts_lookup;    
    icmp::socket socket;
    boost::asio::streambuf reply_buffer;

  void start_receive(boost::shared_ptr<Host> host)
  {
    std::cout << "start_receive.  host: " << host->host_ip_address << std::endl;

    // Discard any data already in the buffer.
    reply_buffer.consume(reply_buffer.size());

    // Wait for a reply. We prepare the buffer to receive up to 64KB.   
    socket.async_receive(reply_buffer.prepare(65536),
                         boost::bind(&Pinger::handle_receive, this, host, _2));
  }

  void handle_receive(boost::shared_ptr<Host> host, std::size_t length)
  {
    std::cout << "handle_receive.  host: " << host->host_ip_address << std::endl;

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

    std::cout << "ICMP ping from " << ipv4_hdr.source_address()
              << ", icmp_seq=" << icmp_hdr.sequence_number()
              << std::endl;        
    
    if (is && icmp_hdr.type() == icmp_header::echo_reply
           && hosts_lookup.find(ost_ipv4_address.str()) != hosts_lookup.end()
           && icmp_hdr.identifier() == get_identifier())
           //&& icmp_hdr.sequence_number() == sequence_number_)
    {
        //std::cout << "Handling source address: " << ipv4_hdr.source_address() << std::endl;
        //std::cout << "ICMP destination: " << destination_.address() << std::endl;        

        // Print out some information about the reply packet.
        posix_time::ptime now = posix_time::microsec_clock::universal_time();
        std::cout << length - ipv4_hdr.header_length()
                  << " bytes from " << ipv4_hdr.source_address()
                  << ": icmp_seq=" << icmp_hdr.sequence_number()
                  << ", ttl=" << ipv4_hdr.time_to_live()
                  << ", time=" << (now - host->time_sent).total_milliseconds() << " ms"
                  << std::endl;
    }
    start_receive(host);
  }    

  void start_send(boost::shared_ptr<Host> host)
  {
    std::cout << "start_send.  host: " << host->host_ip_address << std::endl;
    std::string body("\"Hello!\" from Asio ping.");

    // Create an ICMP header for an echo request.
    icmp_header echo_request;
    echo_request.type(icmp_header::echo_request);
    echo_request.code(0);
    echo_request.identifier(get_identifier());
    echo_request.sequence_number(++host->sequence_number);
    compute_checksum(echo_request, body.begin(), body.end());

    // Encode the request packet.
    boost::asio::streambuf request_buffer;
    std::ostream os(&request_buffer);
    os << echo_request << body;

    // Send the request.
    host->time_sent = posix_time::microsec_clock::universal_time();
    socket.send_to(request_buffer.data(), host->destination);

    host->timer.expires_at(host->time_sent + posix_time::seconds(1));
    host->timer.async_wait(boost::bind(&Pinger::start_send, this, host));
  }
  
  static unsigned short get_identifier()
  {
#if defined(BOOST_WINDOWS)
    return static_cast<unsigned short>(::GetCurrentProcessId());
#else
    return static_cast<unsigned short>(::getpid());
#endif
  }
  
};

#if !defined(BOOST_WINDOWS)
    void stop(int s)
    {
        std::cout << "CTRL-C" << std::endl;
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
  std::vector<std::string> hosts;

  try
  {
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")
        ("host,H", po::value< std::vector<std::string> >(), "host");  
  
    po::positional_options_description positional_desc;
    positional_desc.add("host", -1);
    
    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).options(desc).positional(positional_desc).run(),
              vm);
    po::notify(vm);
    
    if (vm.count("help")) {
        std::cout << "Usage: ping [hosts]" << std::endl;
        std::cout << desc;
        return 0;
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
    boost::shared_ptr<Pinger> ping_receiver(new Pinger(io_service, hosts));
    io_service.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << std::endl;
  }
}
