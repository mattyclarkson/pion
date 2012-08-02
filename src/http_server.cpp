// ---------------------------------------------------------------------
// pion:  a Boost C++ framework for building lightweight HTTP interfaces
// ---------------------------------------------------------------------
// Copyright (C) 2007-2012 Cloudmeter, Inc.  (http://www.cloudmeter.com)
//
// Distributed under the Boost Software License, Version 1.0.
// See http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/exception/diagnostic_information.hpp>
#include <pion/http/server.hpp>
#include <pion/http/request.hpp>
#include <pion/http/request_reader.hpp>
#include <pion/http/response_writer.hpp>


namespace pion {    // begin namespace pion
namespace http {    // begin namespace http


// static members of HTTPServer

const unsigned int          HTTPServer::MAX_REDIRECTS = 10;


// HTTPServer member functions

void HTTPServer::handleConnection(tcp::connection_ptr& tcp_conn)
{
    HTTPRequestReaderPtr reader_ptr;
    reader_ptr = HTTPRequestReader::create(tcp_conn, boost::bind(&HTTPServer::handleRequest,
                                           this, _1, _2, _3));
    reader_ptr->setMaxContentLength(m_max_content_length);
    reader_ptr->receive();
}

void HTTPServer::handleRequest(HTTPRequestPtr& http_request,
    tcp::connection_ptr& tcp_conn, const boost::system::error_code& ec)
{
    if (ec || ! http_request->isValid()) {
        tcp_conn->set_lifecycle(connection::LIFECYCLE_CLOSE); // make sure it will get closed
        if (tcp_conn->is_open() && (&ec.category() == &http::parser::get_error_category())) {
            // HTTP parser error
            PION_LOG_INFO(m_logger, "Invalid HTTP request (" << ec.message() << ")");
            m_bad_request_handler(http_request, tcp_conn);
        } else {
            // other (IO) error
            PION_LOG_INFO(m_logger, "Lost connection on port " << getPort());
            tcp_conn->finish();
        }
        return;
    }
        
    PION_LOG_DEBUG(m_logger, "Received a valid HTTP request");

    // strip off trailing slash if the request has one
    std::string resource_requested(stripTrailingSlash(http_request->getResource()));

    // apply any redirection
    RedirectMap::const_iterator it = m_redirects.find(resource_requested);
    unsigned int num_redirects = 0;
    while (it != m_redirects.end()) {
        if (++num_redirects > MAX_REDIRECTS) {
            PION_LOG_ERROR(m_logger, "Maximum number of redirects (HTTPServer::MAX_REDIRECTS) exceeded for requested resource: " << http_request->getOriginalResource());
            m_server_error_handler(http_request, tcp_conn, "Maximum number of redirects (HTTPServer::MAX_REDIRECTS) exceeded for requested resource");
            return;
        }
        resource_requested = it->second;
        http_request->changeResource(resource_requested);
        it = m_redirects.find(resource_requested);
    }

    // if authentication activated, check current request
    if (m_auth_ptr) {
        // try to verify authentication
        if (! m_auth_ptr->handleRequest(http_request, tcp_conn)) {
            // the HTTP 401 message has already been sent by the authentication object
            PION_LOG_DEBUG(m_logger, "Authentication required for HTTP resource: "
                << resource_requested);
            if (http_request->getResource() != http_request->getOriginalResource()) {
                PION_LOG_DEBUG(m_logger, "Original resource requested was: " << http_request->getOriginalResource());
            }
            return;
        }
    }
    
    // search for a handler matching the resource requested
    RequestHandler request_handler;
    if (findRequestHandler(resource_requested, request_handler)) {
        
        // try to handle the request
        try {
            request_handler(http_request, tcp_conn);
            PION_LOG_DEBUG(m_logger, "Found request handler for HTTP resource: "
                           << resource_requested);
            if (http_request->getResource() != http_request->getOriginalResource()) {
                PION_LOG_DEBUG(m_logger, "Original resource requested was: " << http_request->getOriginalResource());
            }
        } catch (std::bad_alloc&) {
            // propagate memory errors (FATAL)
            throw;
        } catch (std::exception& e) {
            // recover gracefully from other exceptions thrown request handlers
            PION_LOG_ERROR(m_logger, "HTTP request handler: " << boost::diagnostic_information(e));
            m_server_error_handler(http_request, tcp_conn, boost::diagnostic_information(e));
        }
        
    } else {
        
        // no web services found that could handle the request
        PION_LOG_INFO(m_logger, "No HTTP request handlers found for resource: "
                      << resource_requested);
        if (http_request->getResource() != http_request->getOriginalResource()) {
            PION_LOG_DEBUG(m_logger, "Original resource requested was: " << http_request->getOriginalResource());
        }
        m_not_found_handler(http_request, tcp_conn);
    }
}
    
bool HTTPServer::findRequestHandler(const std::string& resource,
                                    RequestHandler& request_handler) const
{
    // first make sure that HTTP resources are registered
    boost::mutex::scoped_lock resource_lock(m_resource_mutex);
    if (m_resources.empty())
        return false;
    
    // iterate through each resource entry that may match the resource
    ResourceMap::const_iterator i = m_resources.upper_bound(resource);
    while (i != m_resources.begin()) {
        --i;
        // check for a match if the first part of the strings match
        if (i->first.empty() || resource.compare(0, i->first.size(), i->first) == 0) {
            // only if the resource matches the plug-in's identifier
            // or if resource is followed first with a '/' character
            if (resource.size() == i->first.size() || resource[i->first.size()]=='/') {
                request_handler = i->second;
                return true;
            }
        }
    }
    
    return false;
}

void HTTPServer::addResource(const std::string& resource,
                             RequestHandler request_handler)
{
    boost::mutex::scoped_lock resource_lock(m_resource_mutex);
    const std::string clean_resource(stripTrailingSlash(resource));
    m_resources.insert(std::make_pair(clean_resource, request_handler));
    PION_LOG_INFO(m_logger, "Added request handler for HTTP resource: " << clean_resource);
}

void HTTPServer::removeResource(const std::string& resource)
{
    boost::mutex::scoped_lock resource_lock(m_resource_mutex);
    const std::string clean_resource(stripTrailingSlash(resource));
    m_resources.erase(clean_resource);
    PION_LOG_INFO(m_logger, "Removed request handler for HTTP resource: " << clean_resource);
}

void HTTPServer::addRedirect(const std::string& requested_resource,
                             const std::string& new_resource)
{
    boost::mutex::scoped_lock resource_lock(m_resource_mutex);
    const std::string clean_requested_resource(stripTrailingSlash(requested_resource));
    const std::string clean_new_resource(stripTrailingSlash(new_resource));
    m_redirects.insert(std::make_pair(clean_requested_resource, clean_new_resource));
    PION_LOG_INFO(m_logger, "Added redirection for HTTP resource " << clean_requested_resource << " to resource " << clean_new_resource);
}

void HTTPServer::handleBadRequest(HTTPRequestPtr& http_request,
                                  tcp::connection_ptr& tcp_conn)
{
    static const std::string BAD_REQUEST_HTML =
        "<html><head>\n"
        "<title>400 Bad Request</title>\n"
        "</head><body>\n"
        "<h1>Bad Request</h1>\n"
        "<p>Your browser sent a request that this server could not understand.</p>\n"
        "</body></html>\n";
    HTTPResponseWriterPtr writer(HTTPResponseWriter::create(tcp_conn, *http_request,
                                                            boost::bind(&connection::finish, tcp_conn)));
    writer->getResponse().setStatusCode(HTTPTypes::RESPONSE_CODE_BAD_REQUEST);
    writer->getResponse().setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_BAD_REQUEST);
    writer->writeNoCopy(BAD_REQUEST_HTML);
    writer->send();
}

void HTTPServer::handleNotFoundRequest(HTTPRequestPtr& http_request,
                                       tcp::connection_ptr& tcp_conn)
{
    static const std::string NOT_FOUND_HTML_START =
        "<html><head>\n"
        "<title>404 Not Found</title>\n"
        "</head><body>\n"
        "<h1>Not Found</h1>\n"
        "<p>The requested URL ";
    static const std::string NOT_FOUND_HTML_FINISH =
        " was not found on this server.</p>\n"
        "</body></html>\n";
    HTTPResponseWriterPtr writer(HTTPResponseWriter::create(tcp_conn, *http_request,
                                                            boost::bind(&connection::finish, tcp_conn)));
    writer->getResponse().setStatusCode(HTTPTypes::RESPONSE_CODE_NOT_FOUND);
    writer->getResponse().setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_NOT_FOUND);
    writer->writeNoCopy(NOT_FOUND_HTML_START);
    writer << http_request->getResource();
    writer->writeNoCopy(NOT_FOUND_HTML_FINISH);
    writer->send();
}

void HTTPServer::handleServerError(HTTPRequestPtr& http_request,
                                   tcp::connection_ptr& tcp_conn,
                                   const std::string& error_msg)
{
    static const std::string SERVER_ERROR_HTML_START =
        "<html><head>\n"
        "<title>500 Server Error</title>\n"
        "</head><body>\n"
        "<h1>Internal Server Error</h1>\n"
        "<p>The server encountered an internal error: <strong>";
    static const std::string SERVER_ERROR_HTML_FINISH =
        "</strong></p>\n"
        "</body></html>\n";
    HTTPResponseWriterPtr writer(HTTPResponseWriter::create(tcp_conn, *http_request,
                                                            boost::bind(&connection::finish, tcp_conn)));
    writer->getResponse().setStatusCode(HTTPTypes::RESPONSE_CODE_SERVER_ERROR);
    writer->getResponse().setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_SERVER_ERROR);
    writer->writeNoCopy(SERVER_ERROR_HTML_START);
    writer << error_msg;
    writer->writeNoCopy(SERVER_ERROR_HTML_FINISH);
    writer->send();
}

void HTTPServer::handleForbiddenRequest(HTTPRequestPtr& http_request,
                                        tcp::connection_ptr& tcp_conn,
                                        const std::string& error_msg)
{
    static const std::string FORBIDDEN_HTML_START =
        "<html><head>\n"
        "<title>403 Forbidden</title>\n"
        "</head><body>\n"
        "<h1>Forbidden</h1>\n"
        "<p>User not authorized to access the requested URL ";
    static const std::string FORBIDDEN_HTML_MIDDLE =
        "</p><p><strong>\n";
    static const std::string FORBIDDEN_HTML_FINISH =
        "</strong></p>\n"
        "</body></html>\n";
    HTTPResponseWriterPtr writer(HTTPResponseWriter::create(tcp_conn, *http_request,
                                                            boost::bind(&connection::finish, tcp_conn)));
    writer->getResponse().setStatusCode(HTTPTypes::RESPONSE_CODE_FORBIDDEN);
    writer->getResponse().setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_FORBIDDEN);
    writer->writeNoCopy(FORBIDDEN_HTML_START);
    writer << http_request->getResource();
    writer->writeNoCopy(FORBIDDEN_HTML_MIDDLE);
    writer << error_msg;
    writer->writeNoCopy(FORBIDDEN_HTML_FINISH);
    writer->send();
}

void HTTPServer::handleMethodNotAllowed(HTTPRequestPtr& http_request,
                                        tcp::connection_ptr& tcp_conn,
                                        const std::string& allowed_methods)
{
    static const std::string NOT_ALLOWED_HTML_START =
        "<html><head>\n"
        "<title>405 Method Not Allowed</title>\n"
        "</head><body>\n"
        "<h1>Not Allowed</h1>\n"
        "<p>The requested method ";
    static const std::string NOT_ALLOWED_HTML_FINISH =
        " is not allowed on this server.</p>\n"
        "</body></html>\n";
    HTTPResponseWriterPtr writer(HTTPResponseWriter::create(tcp_conn, *http_request,
                                                            boost::bind(&connection::finish, tcp_conn)));
    writer->getResponse().setStatusCode(HTTPTypes::RESPONSE_CODE_METHOD_NOT_ALLOWED);
    writer->getResponse().setStatusMessage(HTTPTypes::RESPONSE_MESSAGE_METHOD_NOT_ALLOWED);
    if (! allowed_methods.empty())
        writer->getResponse().addHeader("Allow", allowed_methods);
    writer->writeNoCopy(NOT_ALLOWED_HTML_START);
    writer << http_request->getMethod();
    writer->writeNoCopy(NOT_ALLOWED_HTML_FINISH);
    writer->send();
}

}   // end namespace http
}   // end namespace pion
