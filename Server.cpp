//
// Created by marcin on 11/19/22.
//

#include "Server.h"

Server::Server(Callback callback)
   : running(false),
     client_id(0),
     tcp_fd(0),
     epoll_fd(0),
     callback(callback) {
}

Server::~Server() {}

void Server::start(std::string port) {
   tcp_fd = utils::create_tcp_fd(port);
   if (tcp_fd == -1) {
      throw std::runtime_error("create_and_bind failed");
   }

   if (!utils::make_socket_nonblocking(tcp_fd)) {
      throw std::runtime_error("make_socket_nonblocking failed");
   }

   if (listen(tcp_fd, SOMAXCONN) == -1) {
      throw std::runtime_error("listen failed");
   }

   epoll_fd = utils::create_epoll_fd();
   if (epoll_fd == -1) {
      throw std::runtime_error("create_epoll_fd failed");
   }

   if (!utils::add_descriptor_to_epoll(epoll_fd, tcp_fd, EPOLLIN | EPOLLET)) {
      throw std::runtime_error("add_descriptor_to_epoll on socket_fd failed");
   }

   running = true;
}

void Server::stop() {
   running = false;
}

bool Server::run() {
   struct epoll_event events[EPOLL_MAX_EVENTS];

   while (running) {
      auto epoll_ret = epoll_wait(
         epoll_fd,
         events,
         EPOLL_MAX_EVENTS,
         std::chrono::duration_cast<std::chrono::milliseconds>(EPOLL_TIMEOUT).count());
      if (epoll_ret == 0) {
         continue;
      }

      if (epoll_ret == -1) {
         if (errno == EINTR) {
            continue;
         }

         std::cerr << "epoll_wait failed: " << errno << ' ' << std::string(std::strerror(errno)) << std::endl;
         return false;
      }

      for (auto i = 0; i < epoll_ret; i++) {
         auto fd = events[i].data.fd;
         auto ev = events[i].events;

         if (fd == tcp_fd) {
            // accept all available clients
            auto clients{accept_clients()};
            // mark each worker as connected
            for (auto client : clients) {
               handle_worker_action(*client, callback({ClientEventKind::CONNECTED, client->getID()}));
            }
         } else {
            if (ev & (EPOLLHUP | EPOLLERR)) {
               // client disconnected or something went wrong
               if (auto fd_client = clients_by_fd.find(fd); fd_client != clients_by_fd.end()) {
                  auto c{*fd_client->second};
                  remove_client(c);
                  handle_worker_action(c, callback({ClientEventKind::DISCONNECTED, c.getID()}));
               }
            } else if (auto fd_client = clients_by_fd.find(fd); fd_client != clients_by_fd.end()) {
               // handle client event
               auto c{*fd_client->second};
               if (auto message{read_from_client(*fd_client->second)}; message.has_value()) {
                  utils::update_timer_fd(c.getTimerFD(), CLIENT_TIMEOUT);
                  handle_worker_action(c, callback({ClientEventKind::MESSAGE_RECEIVED, c.getID(), *message}));
               } else {
                  remove_client(c);
                  handle_worker_action(c, callback({ClientEventKind::DISCONNECTED, c.getID()}));
               }
            } else if (auto timer_client = clients_by_timer_fd.find(fd); timer_client != clients_by_timer_fd.end()) {
               // read timer value, just for compliance
               uint64_t value;
               read(fd, &value, sizeof(value));

               // evict client as timeout for heartbeat expired
               auto c{*timer_client->second};
               remove_client(c);
               handle_worker_action(c, callback({ClientEventKind::DISCONNECTED, c.getID()}));
            }
         }
      }
   }

   cleanup();

   return true;
}

std::vector<std::shared_ptr<Client>> Server::accept_clients() {
   std::vector<std::shared_ptr<Client>> clients{};

   while (true) {
      struct sockaddr_in in_addr;
      socklen_t in_len = sizeof(in_addr);

      auto client_fd = accept(tcp_fd, (struct sockaddr*) &in_addr, &in_len);
      if (client_fd == -1) {
         if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return clients;
         }

         throw std::runtime_error("accept failed");
      }

      if (!utils::make_socket_nonblocking(client_fd)) {
         throw std::runtime_error("make_socket_nonblocking failed");
      }

      if (!utils::add_descriptor_to_epoll(epoll_fd, client_fd, EPOLLIN | EPOLLET)) {
         throw std::runtime_error("add_descriptor_to_epoll on client_fd failed");
      }

      auto timer_fd = utils::create_timer_fd(CLIENT_TIMEOUT);

      if (!utils::add_descriptor_to_epoll(epoll_fd, timer_fd, EPOLLIN | EPOLLET)) {
         throw std::runtime_error("add_descriptor_to_epoll on timer_fd failed");
      }

      auto client = std::make_shared<Client>(
         client_id++,
         timer_fd,
         client_fd,
         utils::ip_address_to_string(in_addr),
         ntohs(in_addr.sin_port));

      clients_by_fd[client->getClientFD()] = client;
      clients_by_timer_fd[client->getTimerFD()] = client;

      clients.push_back(std::move(client));
   }

   return clients;
}

void Server::remove_client(Client client) {
   if (auto c = clients_by_fd.find(client.getClientFD()); c != clients_by_fd.end()) {
      if (!utils::remove_client_from_epoll(epoll_fd, c->second->getClientFD())) {
      }

      close(c->second->getClientFD());

      clients_by_fd.erase(c);
   }

   if (auto c = clients_by_timer_fd.find(client.getTimerFD()); c != clients_by_timer_fd.end()) {
      if (!utils::remove_client_from_epoll(epoll_fd, c->second->getTimerFD())) {
      }

      close(c->second->getTimerFD());

      clients_by_timer_fd.erase(c);
   }
}

std::optional<std::vector<char>> Server::read_from_client(const Client& client) {
   char buffer[512];
   std::vector<char> message{};
   ssize_t recv_ret = recv(client.getClientFD(), buffer, sizeof(buffer), 0);

   if (recv_ret > 0) {
      std::copy(buffer, buffer + recv_ret, std::back_inserter(message));
   } else if (recv_ret == 0) {
      // client disconnected
      return {};
   } else if (recv_ret < 0) {
      if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
         // something went badly wrong
         throw std::runtime_error("recv_ret < 0 and" + std::string(std::strerror(errno)));
      }
   }

   return {message};
}

void Server::cleanup() {
   for (auto const& [a, c] : clients_by_fd) {
      if (!utils::remove_client_from_epoll(epoll_fd, c->getClientFD())) {
      }

      close(c->getClientFD());

      if (!utils::remove_client_from_epoll(epoll_fd, c->getTimerFD())) {
      }

      close(c->getTimerFD());
   }

   clients_by_fd.clear();
   clients_by_timer_fd.clear();

   if (!utils::remove_client_from_epoll(epoll_fd, tcp_fd)) {
   }

   close(tcp_fd);
   close(epoll_fd);

   client_id = 0;
   tcp_fd = 0;
   epoll_fd = 0;
}

void Server::handle_worker_action(const Client& client, WorkerAction action) {
   size_t total_sent{};
   auto message_size{action.message.size()};

   switch (action.kind) {
      case WorkerActionKind::SEND_MESSAGE:
         while (total_sent < message_size) {
            auto write_ret = write(client.getClientFD(), &action.message[total_sent], (message_size - total_sent));
            if (write_ret < 0) {
               if (errno == EAGAIN || errno == EWOULDBLOCK) {
                  return;
               }

               remove_client(client);
               callback({ClientEventKind::DISCONNECTED, client.getID()});
            }

            total_sent += static_cast<size_t>(write_ret);
         }

         break;
      case WorkerActionKind::DISCONNECT:
         remove_client(client);
         callback({ClientEventKind::DISCONNECTED, client.getID()});
         break;

      case WorkerActionKind::EXIT: stop(); break;
      case WorkerActionKind::NOOP: break;
   }
}
