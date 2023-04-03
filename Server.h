//
// Created by marcin on 11/19/22.
//

#ifndef EPOLL_WORK_QUEUE_SERVER_H
#define EPOLL_WORK_QUEUE_SERVER_H

#include "Client.h"
#include "utils.h"

#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

using Callback = std::function<WorkerAction(ClientEvent)>;

class Server {
   public:
   Server(){};
   Server(Callback callback);
   ~Server();

   // Does the server event loop.
   // This call will terminate once stop() is called
   bool run();
   // Marks the server as ready for run()
   void start(std::string port);
   // Let's the server exit the run() loop
   void stop();

   private:
   static const constexpr auto EPOLL_MAX_EVENTS = 64;
   static const constexpr auto EPOLL_TIMEOUT = std::chrono::seconds(1);
   static const constexpr auto CLIENT_TIMEOUT = std::chrono::seconds(5);

   // Are we running?
   bool running;
   // Sequence for client IDs
   unsigned int client_id;
   // File descriptor of the TCP server socket
   int tcp_fd;
   // File descriptor of the epoll queue
   int epoll_fd;
   // Mapping of client FDs to clients
   std::unordered_map<int, std::shared_ptr<Client>> clients_by_fd;
   // Mapping of client heartbeat timer FDs to clients
   std::unordered_map<int, std::shared_ptr<Client>> clients_by_timer_fd;
   // The callback
   Callback callback;

   // Accept new client when ready
   std::vector<std::shared_ptr<Client>> accept_clients();
   // Remove existing client: close the socket, remove from epoll and maps
   void remove_client(Client client);
   // Handle event arriving from the client
   std::optional<std::vector<char>> read_from_client(const Client& client);
   // Handle the response to a worker event
   void handle_worker_action(const Client& client, WorkerAction action);
   // Cleanup the clients and sockets
   void cleanup();
};

#endif //EPOLL_WORK_QUEUE_SERVER_H
