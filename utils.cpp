//
// Created by marcin on 11/20/22.
//

#include "utils.h"

namespace utils {
bool make_socket_nonblocking(int fd) {
   auto flags = fcntl(fd, F_GETFL, 0);
   if (flags == -1) {
      return false;
   }

   flags |= O_NONBLOCK;

   if (auto s = fcntl(fd, F_SETFL, flags); s == -1) {
      return false;
   }

   return true;
}

bool add_descriptor_to_epoll(int epoll_fd, int client_fd, unsigned int events) {
   struct epoll_event event;

   event.events = events;
   event.data.fd = client_fd;

   return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) >= 0;
}

bool remove_client_from_epoll(int epoll_fd, int client_fd) {
   return epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL) >= 0;
}

std::string ip_address_to_string(const struct sockaddr_in& addr) {
   char s[INET6_ADDRSTRLEN];
   inet_ntop(AF_INET, &addr.sin_addr, s, sizeof s);
   return std::string(s);
}

int create_epoll_fd() {
   auto epoll_fd = epoll_create1(0);
   if (epoll_fd == -1) {
      throw std::runtime_error("epoll_create1 failed");
   }

   return epoll_fd;
}

int create_tcp_fd(const std::string& port) {
   struct addrinfo hints;

   memset(&hints, 0, sizeof(struct addrinfo));

   hints.ai_family = AF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_protocol = IPPROTO_TCP;
   hints.ai_flags = AI_PASSIVE;

   struct addrinfo* result;

   auto getaddrinfo_ret = getaddrinfo(nullptr, port.c_str(), &hints, &result);
   if (getaddrinfo_ret != 0) {
      throw std::runtime_error("getaddrinfo failed");
   }

   int socket_fd;
   struct addrinfo* info;

   for (info = result; info != nullptr; info = info->ai_next) {
      socket_fd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
      if (socket_fd == -1) {
         continue;
      }

      getaddrinfo_ret = bind(socket_fd, info->ai_addr, info->ai_addrlen);
      if (getaddrinfo_ret == 0) {
         break;
      }

      close(socket_fd);
   }

   if (info == nullptr) {
      throw std::runtime_error("failed to allocate socket");
   }

   freeaddrinfo(result);

   return socket_fd;
}

int create_timer_fd(std::chrono::seconds expiry) {
   auto timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
   if (timer_fd == -1) {
      throw std::runtime_error("timerfd_create failed");
   }

   struct itimerspec ts;
   ts.it_interval.tv_sec = 0;
   ts.it_interval.tv_nsec = 0;
   ts.it_value.tv_sec = expiry.count();
   ts.it_value.tv_nsec = 0;

   if (timerfd_settime(timer_fd, 0, &ts, NULL) < 0) {
      throw std::runtime_error("timerfd_settime failed");
   }

   return timer_fd;
}

void update_timer_fd(int timer_fd, std::chrono::seconds expiry) {
   struct itimerspec ts;
   ts.it_interval.tv_sec = 0;
   ts.it_interval.tv_nsec = 0;
   ts.it_value.tv_sec = expiry.count();
   ts.it_value.tv_nsec = 0;

   if (timerfd_settime(timer_fd, 0, &ts, NULL) < 0) {
      throw std::runtime_error("timerfd_settime failed");
   }
}

bool send_to_socket(int socket_fd, std::vector<char> message) {
   size_t total_sent{};
   auto message_size{message.size()};

   while (total_sent < message_size) {
      auto write_ret = write(socket_fd, &message[total_sent], (message_size - total_sent));
      if (write_ret < 0) {
         return false;
      }

      total_sent += static_cast<size_t>(write_ret);
   }

   return true;
}

std::optional<std::vector<char>> read_from_socket(int socket_fd) {
   char buffer[512];
   std::vector<char> message{};

   while (true) {
      auto recv_ret = recv(socket_fd, buffer, sizeof(buffer), 0);
      if (recv_ret == sizeof(buffer)) {
         std::copy(buffer, buffer + recv_ret, std::back_inserter(message));
      } else if (recv_ret > 0 && recv_ret < static_cast<ssize_t>(sizeof(buffer))) {
         std::copy(buffer, buffer + recv_ret, std::back_inserter(message));
         return message;
      } else if (recv_ret <= 0) {
         return {};
      } else {
         throw std::runtime_error("recv_ret < 0 and" + std::string(std::strerror(errno)));
      }
   }

   return {};
}

std::vector<char> ProtocolEvent::marshal() const {
   std::string r;

   switch (kind) {
      case ProtocolEventKind::WORK:
         r = "W:" + work;
         break;
      case ProtocolEventKind::RESULT:
         r = "R:" + std::to_string(result);
         break;
      case ProtocolEventKind::HEARTBEAT:
         r = "H:";
         break;
   }

   std::vector<char> data(r.begin(), r.end());

   return data;
}

std::optional<ProtocolEvent> unmarshal_proto(std::vector<char> raw_data) {
   std::string data(raw_data.begin(), raw_data.end());

   if (data.length() < 2) {
      return {};
   }

   auto prefix{data.substr(0, 2)};
   auto rest{data.erase(0, 2)};

   if (prefix == "W:") {
      return {ProtocolEvent(rest)};
   }

   if (prefix == "R:") {
      std::size_t result;
      if (std::sscanf(rest.c_str(), "%zu", &result)) {
         return {ProtocolEvent(result)};
      }

      return {};
   }

   if (prefix == "H:") {
      return {ProtocolEvent()};
   }

   return {};
}
}
