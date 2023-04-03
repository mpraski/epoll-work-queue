//
// Created by marcin on 11/20/22.
//

using namespace std;

#include "Client.h"
#include "CurlRequest.h"
#include "utils.h"
#include <atomic>
#include <cstring>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

auto fetch_url_list(std::string fileLink) {
   CurlRequest curl{curl_easy_init()};
   curl.set_url(fileLink);
   curl.set_timeout(30);

   return curl.execute();
}

auto count_unique_domains(std::stringstream fileList) {
   std::unordered_set<std::string> domains_seen{};

   for (std::string row; getline(fileList, row, '\n');) {
      if (auto pos = row.find_first_of(","); pos != std::string::npos) {
         auto url = std::string_view(row).substr(0, pos);

         if (auto pos = url.find_first_of("/"); pos != std::string::npos) {
            domains_seen.emplace(url.substr(0, pos));
         } else {
            domains_seen.emplace(url);
         }
      }
   }

   return domains_seen.size();
}

/// Client process that receives a list of URLs and reports the result
/// Example:
///    ./worker localhost 4242
/// The worker then contacts the leader process on "localhost" port "4242" for work
int main(int argc, char* argv[]) {
   if (argc != 3) {
      std::cerr << "Usage: " << argv[0] << " <host> <port>" << std::endl;
      return 1;
   }

   sleep(1);

   auto curlSetup = CurlGlobalSetup();

   std::mutex sockMutex;
   std::atomic<bool> running{true};
   std::string host{argv[1]};
   std::string port{argv[2]};

   int sockfd;
   struct addrinfo hints, *servinfo, *p;

   memset(&hints, 0, sizeof hints);
   hints.ai_family = AF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;

   // This is a real workhorse of a function with a lot of options, but usage is
   // actually pretty simple. It helps set up the structs you need later on.
   if (auto rv = getaddrinfo(host.c_str(), port.c_str(), &hints, &servinfo); rv != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
      return 1;
   }

   // loop through all the results and connect to the first we can
   for (p = servinfo; p != NULL; p = p->ai_next) {
      if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
         perror("client: socket");
         continue;
      }

      if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
         close(sockfd);
         perror("client: connect");
         continue;
      }

      break;
   }

   if (p == NULL) {
      fprintf(stderr, "client: failed to connect\n");
      return 2;
   }

   freeaddrinfo(servinfo);

   // Create a thread that sends a heartbeat every N seconds
   std::thread heartbeatThread([&] {
      while (running) {
         std::this_thread::sleep_for(1s);

         auto response{utils::ProtocolEvent().marshal()};
         std::unique_lock<std::mutex> lock(sockMutex);
         utils::send_to_socket(sockfd, response);
         lock.unlock();
      }
   });

   // This is the main loop of the worker. It receives a message from the leader,
   // does the work, and sends the result back to the leader.
   // The worker will continue to do this until the leader disconnects.
   // The worker will then exit.
   std::thread mainThread([&] {
      fd_set fdread;
      struct timeval tv;

      do {
         tv.tv_sec = 10;
         tv.tv_usec = 0;

         FD_ZERO(&fdread);
         FD_SET(sockfd, &fdread);

         auto select_ret = select(sockfd + 1, &fdread, NULL, NULL, &tv);
         switch (select_ret) {
            case -1:
               // Error happened
               running = false;
               break;
            case 0:
               // Timeout - nothing to read
               break;
            default: {
               if (auto received{utils::read_from_socket(sockfd)}; received.has_value()) {
                  if (auto proto{utils::unmarshal_proto(*received)}; proto.has_value()) {
                     if (proto->kind == utils::ProtocolEventKind::WORK) {
                        auto count{count_unique_domains(fetch_url_list(proto->work))};
                        auto response{utils::ProtocolEvent(count).marshal()};

                        std::unique_lock<std::mutex> lock(sockMutex);
                        utils::send_to_socket(sockfd, response);
                        lock.unlock();

                        break;
                     }
                  }
               }

               running = false;
            }

            break;
         }
      } while (running);
   });

   mainThread.join();
   heartbeatThread.join();

   close(sockfd);

   return 0;
}
