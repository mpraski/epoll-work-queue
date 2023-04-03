//
// Created by marcin on 11/20/22.
//

#ifndef EPOLL_WORK_QUEUE_CURL_REQUEST_H
#define EPOLL_WORK_QUEUE_CURL_REQUEST_H

#include <string>
#include <curl/curl.h>

class CurlGlobalSetup {
   public:
   CurlGlobalSetup();
   ~CurlGlobalSetup();

   CurlGlobalSetup(const CurlGlobalSetup&) = delete;
   CurlGlobalSetup& operator=(const CurlGlobalSetup&) = delete;
};

class CurlRequest {
   public:
   CurlRequest();

   void set_url(const std::string& url);
   void set_timeout(int timeout_secs);
   std::stringstream execute();

   private:
   std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> ptr;
};

#endif
