//
// Created by marcin on 11/20/22.
//

#include "Client.h"

#include <iostream>

const char* WorkerEventKindText[] = {
   "CONNECTED",
   "DISCONNECTED",
   "MESSAGE_RECEIVED"};

const char* WorkerActionKindText[] = {
   "SEND_MESSAGE",
   "DISCONNECT",
   "NOOP",
   "EXIT"};

unsigned int Client::getID() const {
   return id;
}

int Client::getTimerFD() const {
   return timer_fd;
}

int Client::getClientFD() const {
   return client_fd;
}

std::ostream& operator<<(std::ostream& os, const Client& w) {
   os << "worker(" << w.id << ',' << w.ip_address << ':' << w.port << ')';
   return os;
}
std::ostream& operator<<(std::ostream& os, const WorkerAction& w) {
   std::string s(w.message.begin(), w.message.end());
   if (!s.empty() && s[s.length() - 1] == '\n') {
      s.erase(s.length() - 1);
   }

   if (s.empty()) {
      s = "<no_msg>";
   }

   os << "worker_action(" << WorkerActionKindText[((int) w.kind)] << ',' << s << ')';
   return os;
}

std::ostream& operator<<(std::ostream& os, const ClientEvent& w) {
   std::string s(w.message.begin(), w.message.end());
   if (!s.empty() && s[s.length() - 1] == '\n') {
      s.erase(s.length() - 1);
   }

   if (s.empty()) {
      s = "<no_msg>";
   }

   os << "worker_event(" << WorkerEventKindText[((int) w.kind)] << ',' << w.worker_id << ',' << s << ')';
   return os;
}