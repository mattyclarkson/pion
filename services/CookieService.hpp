// ---------------------------------------------------------------------
// pion:  a Boost C++ framework for building lightweight HTTP interfaces
// ---------------------------------------------------------------------
// Copyright (C) 2007-2012 Cloudmeter, Inc.  (http://www.cloudmeter.com)
//
// Distributed under the Boost Software License, Version 1.0.
// See http://www.boost.org/LICENSE_1_0.txt
//

#ifndef __PION_COOKIESERVICE_HEADER__
#define __PION_COOKIESERVICE_HEADER__

#include <pion/http/plugin_service.hpp>


namespace pion {        // begin namespace pion
namespace plugins {     // begin namespace plugins

///
/// CookieService: web service that displays and updates cookies
/// 
class CookieService :
    public pion::http::WebService
{
public:
    CookieService(void) {}
    virtual ~CookieService() {}
    virtual void operator()(pion::http::HTTPRequestPtr& request,
                            pion::tcp::connection_ptr& tcp_conn);
};

}   // end namespace plugins
}   // end namespace pion
    
#endif
