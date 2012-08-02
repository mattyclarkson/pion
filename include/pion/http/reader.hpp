// ---------------------------------------------------------------------
// pion:  a Boost C++ framework for building lightweight HTTP interfaces
// ---------------------------------------------------------------------
// Copyright (C) 2007-2012 Cloudmeter, Inc.  (http://www.cloudmeter.com)
//
// Distributed under the Boost Software License, Version 1.0.
// See http://www.boost.org/LICENSE_1_0.txt
//

#ifndef __PION_HTTP_READER_HEADER__
#define __PION_HTTP_READER_HEADER__

#include <boost/asio.hpp>
#include <pion/config.hpp>
#include <pion/http/parser.hpp>
#include <pion/http/message.hpp>
#include <pion/tcp/connection.hpp>
#include <pion/tcp/timer.hpp>


namespace pion {    // begin namespace pion
namespace http {    // begin namespace http


///
/// HTTPReader: asynchronously reads and parses HTTP messages
///
class PION_API HTTPReader :
    public http::parser
{
public:

    // default destructor
    virtual ~HTTPReader() {}
    
    /// Incrementally reads & parses the HTTP message
    void receive(void);
    
    /// returns a shared pointer to the TCP connection
    inline tcp::connection_ptr& get_connection(void) { return m_tcp_conn; }
    
    /// sets the maximum number of seconds for read operations
    inline void setTimeout(boost::uint32_t seconds) { m_read_timeout = seconds; }

    
protected:

    /**
     * protected constructor: only derived classes may create objects
     *
     * @param is_request if true, the message is parsed as an HTTP request;
     *                   if false, the message is parsed as an HTTP response
     * @param tcp_conn TCP connection containing a new message to parse
     */
    HTTPReader(const bool is_request, tcp::connection_ptr& tcp_conn)
        : http::parser(is_request), m_tcp_conn(tcp_conn),
        m_read_timeout(DEFAULT_READ_TIMEOUT)
        {}  
    
    /**
     * Consumes bytes that have been read using an HTTP parser
     * 
     * @param read_error error status from the last read operation
     * @param bytes_read number of bytes consumed by the last read operation
     */
    void consumeBytes(const boost::system::error_code& read_error,
                      std::size_t bytes_read);

    /// Consumes bytes that have been read using an HTTP parser
    void consumeBytes(void);
    
    /// Reads more bytes from the TCP connection
    virtual void readBytes(void) = 0;

    /// Called after we have finished reading/parsing the HTTP message
    virtual void finishedReading(const boost::system::error_code& ec) = 0;

    /// Returns a reference to the HTTP message being parsed
    virtual http::message& getMessage(void) = 0;


private:

    /// reads more bytes for parsing, with timeout support
    void readBytesWithTimeout(void);

    /**
     * Handles errors that occur during read operations
     *
     * @param read_error error status from the last read operation
     */
    void handleReadError(const boost::system::error_code& read_error);


    /// default maximum number of seconds for read operations
    static const boost::uint32_t            DEFAULT_READ_TIMEOUT;


    /// The HTTP connection that has a new HTTP message to parse
    tcp::connection_ptr                        m_tcp_conn;
    
    /// pointer to a tcp::timer object if read timeouts are enabled
    tcp::timer_ptr                             m_timer_ptr;

    /// maximum number of seconds for read operations
    boost::uint32_t                         m_read_timeout;
};


}   // end namespace http
}   // end namespace pion

#endif
