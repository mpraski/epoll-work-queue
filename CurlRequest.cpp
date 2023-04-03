//
// Created by marcin on 11/20/22.
//

#include "CurlRequest.h"
#include <fstream>
#include <iostream>
#include <sstream>

CurlGlobalSetup::CurlGlobalSetup() {
   curl_global_init(CURL_GLOBAL_ALL);
}

CurlGlobalSetup::~CurlGlobalSetup() {
   curl_global_cleanup();
}

CurlRequest::CurlRequest(CURL* handle) : ptr{handle, curl_easy_cleanup} {
   if (!ptr) {
      throw std::runtime_error("failed to initialize curl");
   }
}

void CurlRequest::set_url(const std::string& url) {
   curl_easy_setopt(ptr.get(), CURLOPT_URL, url.c_str());
}

void CurlRequest::set_timeout(int timeout_secs) {
   curl_easy_setopt(ptr.get(), CURLOPT_TIMEOUT, timeout_secs);
}

std::stringstream CurlRequest::execute() {
   std::stringstream responseData;

   auto writeToString = +[](char* contents, size_t size, size_t nmemb, void* userdata) -> size_t {
      auto& responseData = *reinterpret_cast<std::stringstream*>(userdata);
      responseData << std::string_view(contents, size * nmemb);
      return size * nmemb;
   };

   curl_easy_setopt(ptr.get(), CURLOPT_WRITEDATA, &responseData);
   curl_easy_setopt(ptr.get(), CURLOPT_WRITEFUNCTION, writeToString);

   if (auto res = curl_easy_perform(ptr.get()); res != CURLE_OK)
      throw std::runtime_error(curl_easy_strerror(res));

   return responseData;
}
