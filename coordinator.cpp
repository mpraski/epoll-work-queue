//
// Created by marcin on 11/20/22.
//

#include "coordinator.h"

namespace {
std::function<void(int)> shutdown_handler;
void signal_handler(int signal) { shutdown_handler(signal); }
}

/// Leader process that coordinates workers. Workers connect on the specified port
/// and the coordinator distributes the work of the CSV file list.
/// Example:
///    ./coordinator http://example.org/filelist.csv 4242
Coordinator::Coordinator(std::string file_location, std::string port)
   : server{},
     port{port},
     assigned_work{},
     heartbeats{},
     work_left{},
     aggregate{} {
   // Split the work into small chunks and fill the open_work vector with it
   CurlGlobalSetup globalCurl{};

   CurlRequest curl{};
   curl.set_url(file_location);
   curl.set_timeout(30);

   auto fileList = curl.execute();

   // Iterate over all files
   for (std::string url; std::getline(fileList, url, '\n');) {
      work_left.push_back(url);
   }
}

Server Coordinator::create_server() {
   // hand over a callback function that returns a WorkerAction depending on the ClientEvent received
   auto callbackFunction = [this](ClientEvent event) {
      // if all work has finished, exit
      if (work_finished()) {
         return WorkerAction(WorkerActionKind::EXIT);
      }

      switch (event.kind) {
         // start sending messages when a new client connects, leave the message empty in this case
         case ClientEventKind::CONNECTED: {
            // Do nothing
            return WorkerAction();
         }
         // also send next message when we receive a result, here we need the message
         case ClientEventKind::MESSAGE_RECEIVED: {
            if (auto proto{utils::unmarshal_proto(event.message)}; proto.has_value()) {
               switch (proto->kind) {
                  case utils::ProtocolEventKind::WORK: {
                     // Do nothing
                     return WorkerAction();
                  }
                  case utils::ProtocolEventKind::RESULT: {
                     // increment the counter
                     aggregate += static_cast<unsigned int>(proto->result);
                     // remove this work item successfully
                     finish_work(event.worker_id);
                     // if all work has finished, exit
                     if (work_finished()) {
                        return WorkerAction(WorkerActionKind::EXIT);
                     }
                     // if work is available, send it over
                     if (auto work{assign_work(event.worker_id)}; work.has_value()) {
                        return WorkerAction(WorkerActionKind::SEND_MESSAGE, utils::ProtocolEvent(*work).marshal());
                     }
                     // If none of the above, do nothing
                     return WorkerAction();
                  }
                  case utils::ProtocolEventKind::HEARTBEAT: {
                     // Just increment the worker heartbeat counter
                     increment_heartbeat(event.worker_id);
                     // On the second heatbeat actually distribute work
                     if (get_heartbeat(event.worker_id) > 1) {
                        // if work is available, send it over
                        if (auto work{assign_work(event.worker_id)}; work.has_value()) {
                           return WorkerAction(WorkerActionKind::SEND_MESSAGE, utils::ProtocolEvent(*work).marshal());
                        }
                     }
                     // If none of the above, do nothing
                     return WorkerAction();
                  }
               }
            }
            return WorkerAction();
         }
         // when we receive a disconnect event, we need to remove the client from our known workers and reassign the work
         case ClientEventKind::DISCONNECTED: {
            // lookup lost work in the map and re-add it to the vector
            remove_worker(event.worker_id);
            // return a NOOP response since the client already disconnected
            return WorkerAction();
         }
         // in any other case return a NOOP WorkerAction
         default: return WorkerAction();
      }
   };

   return Server(callbackFunction);
}

bool Coordinator::work_finished() const noexcept {
   return work_left.empty() && assigned_work.empty();
}

bool Coordinator::worker_busy(unsigned int worker_id) const noexcept {
   return assigned_work.find(worker_id) != assigned_work.end();
}

unsigned int Coordinator::get_heartbeat(unsigned int worker_id) const noexcept {
   if (auto it{heartbeats.find(worker_id)}; it != heartbeats.end()) {
      return it->second;
   }

   return 0;
}

void Coordinator::increment_heartbeat(unsigned int worker_id) noexcept {
   if (auto it{heartbeats.find(worker_id)}; it != heartbeats.end()) {
      it->second++;
   } else {
      heartbeats.insert(std::make_pair(worker_id, 1u));
   }
}

// Returns a new work unit and assigns it to the worker
std::optional<std::string> Coordinator::assign_work(unsigned int worker_id) {
   if (work_left.empty()) {
      return {};
   }

   if (worker_busy(worker_id)) {
      return {};
   }

   // Pop the top of the work queue
   auto w{work_left.front()};
   work_left.pop_front();

   // Assign it to worker
   assigned_work.insert_or_assign(worker_id, w);

   return w;
}

void Coordinator::finish_work(unsigned int worker_id) {
   assigned_work.erase(worker_id);
}

// Removes a worker and re-adds its associated work unit
void Coordinator::remove_worker(unsigned int worker_id) {
   // in in this case we need to remove the worker from our map of worker_ids and retrieve the unfinished work
   auto work{assigned_work.extract(worker_id)};
   // if the work was found, remove it from the map
   if (work) {
      work_left.push_back(work.mapped());
   }
}

Coordinator::~Coordinator() {}

void Coordinator::start() {
   // Create the server and handle Client connections
   server = create_server();
   server.start(port);

   if (!server.run()) {
      std::cerr << "Server failed to run" << std::endl;
   }

   std::cout << aggregate << std::endl;
}

void Coordinator::stop() {
   server.stop();
}

// The main function creating a Coordinator object
int main(int argc, char* argv[]) {
   if (argc != 3) {
      std::cerr << "Usage: " << argv[0] << " <URL to csv list> <listen port>" << std::endl;
      return 1;
   }

   std::signal(SIGTERM, signal_handler);

   Coordinator coordinator{std::string(argv[1]), std::string(argv[2])};

   shutdown_handler = [&](int) {
      coordinator.stop();
   };

   coordinator.start();

   return 0;
}
