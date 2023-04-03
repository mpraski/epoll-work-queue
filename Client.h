//
// Created by marcin on 11/20/22.
//

#ifndef EPOLL_WORK_QUEUE_CLIENT_H
#define EPOLL_WORK_QUEUE_CLIENT_H

#include <array>
#include <string>
#include <vector>

// This is the kind of the event from a client
// A client can either connect, disconnect (or be disconnected in case heartbeat expires)
// or send us a message
enum class ClientEventKind { CONNECTED,
                             DISCONNECTED,
                             MESSAGE_RECEIVED };

// This is the action in response to worker event.
// We can either send the worker a message, disconnect it
// gracefully from our list, or do nothing.
enum class WorkerActionKind { SEND_MESSAGE,
                              DISCONNECT,
                              NOOP,
                              EXIT };

// This is the object representing the response
// to a worker event. It has a kind and optional message.
class WorkerAction {
   public:
   WorkerAction() : kind(WorkerActionKind::NOOP), message{} {}
   WorkerAction(WorkerActionKind kind) : kind(kind), message{} {}
   WorkerAction(WorkerActionKind kind, std::vector<char> message) : kind(kind), message(message) {}

   WorkerActionKind kind;
   std::vector<char> message;

   friend std::ostream& operator<<(std::ostream& os, const WorkerAction& w);
};

// This is the object representing the client event.
// It has a kind, client ID and optional message.
class ClientEvent {
   public:
   ClientEvent(ClientEventKind kind, unsigned int worker_id) : kind(kind), worker_id(worker_id), message{} {}
   ClientEvent(ClientEventKind kind, unsigned int worker_id, std::vector<char> message) : kind(kind), worker_id(worker_id), message(message) {}

   ClientEventKind kind;
   unsigned int worker_id;
   std::vector<char> message;

   friend std::ostream& operator<<(std::ostream& os, const ClientEvent& w);
};

class Client {
   public:
   Client(
      unsigned int id,
      int timer_fd,
      int client_fd,
      std::string ip_address,
      unsigned short port) : id(id),
                             timer_fd(timer_fd),
                             client_fd(client_fd),
                             ip_address(ip_address),
                             port(port) {}

   unsigned int getID() const;
   int getTimerFD() const;
   int getClientFD() const;
   int getStringOccurences(std::string listUrl) const;

   private:
   // The unique ID of the client
   unsigned int id;
   // File descriptor of associated heartbeat timer
   int timer_fd;
   // File descriptor of associated client socket
   int client_fd;
   // IP address of the client
   std::string ip_address;
   // Port of the client
   unsigned short port;

   friend std::ostream& operator<<(std::ostream& os, const Client& w);
};

#endif //EPOLL_WORK_QUEUE_CLIENT_H
