//
// Created by marcin on 11/20/22.
//

#ifndef EPOLL_WORK_QUEUE_UTILS_H
#define EPOLL_WORK_QUEUE_UTILS_H

#include <chrono>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <unistd.h>

namespace utils {
bool make_socket_nonblocking(int fd);

bool add_descriptor_to_epoll(int epoll_fd, int client_fd, unsigned int events);

bool remove_client_from_epoll(int epoll_fd, int client_fd);

std::string ip_address_to_string(const struct sockaddr_in& addr);

int create_epoll_fd();

int create_tcp_fd(const std::string& port);

int create_timer_fd(std::chrono::seconds expiry);

void update_timer_fd(int timer_fd, std::chrono::seconds expiry);

bool send_to_socket(int socket_fd, std::vector<char> message);

std::optional<std::vector<char>> read_from_socket(int socket_fd);

enum class ProtocolEventKind { WORK,
                               RESULT,
                               HEARTBEAT };

class ProtocolEvent {
   public:
   ProtocolEvent() : kind(ProtocolEventKind::HEARTBEAT), result{}, work{} {}
   ProtocolEvent(std::string work) : kind(ProtocolEventKind::WORK), result{}, work(work) {}
   ProtocolEvent(std::size_t result) : kind(ProtocolEventKind::RESULT), result(result), work{} {}

   ProtocolEventKind kind;
   std::size_t result;
   std::string work;

   std::vector<char> marshal() const;
};

std::optional<ProtocolEvent> unmarshal_proto(std::vector<char> data);
}

#endif //EPOLL_WORK_QUEUE_UTILS_H
