//
// Created by marcin on 11/20/22.
//

#ifndef EPOLL_WORK_QUEUE_COORDINATOR_H
#define EPOLL_WORK_QUEUE_COORDINATOR_H

#include "CurlRequest.h"
#include "Server.h"
#include "utils.h"

#include <chrono>
#include <csignal>
#include <deque>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>

class Coordinator {
   public:
   Coordinator(std::string file_location, std::string port);
   ~Coordinator();

   void start();
   void stop();

   private:
   // The Server created by the coordinator
   Server server;
   // The server port
   std::string port;
   // A mapping of worker id to its current workload
   std::unordered_map<unsigned int, std::string> assigned_work;
   // A mapping of worker id to its number of heatbeats
   std::unordered_map<unsigned int, unsigned int> heartbeats;
   // The work that is still to be done
   std::deque<std::string> work_left;
   // the total result adding together all subresults from the workers
   unsigned int aggregate;

   // Creates a new server with an appropriate callback for our purposes
   Server create_server();
   // Checks if all works has finished
   bool work_finished() const noexcept;
   // Assigns the work item to a worker, when possible
   std::optional<std::string> assign_work(unsigned int worker_id);
   // Get the heartbeat counter for a worker
   unsigned int get_heartbeat(unsigned int worker_id) const noexcept;
   // Increments the heatbeat counter for this worker
   void increment_heartbeat(unsigned int worker_id) noexcept;
   // Check if this worker is currently processing work
   bool worker_busy(unsigned int worker_id) const noexcept;
   // Mark work of this worker as finished, remove it from the workload map
   void finish_work(unsigned int worker_id);
   // removes the worker from the workload map and adds the associated work back to the queue
   void remove_worker(unsigned int worker_id);
};

#endif //EPOLL_WORK_QUEUE_COORDINATOR_H
