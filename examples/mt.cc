#include <atomic>
#include <thread>
#include <vector>
#include <random>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <mutex>
#include "lockfreelist.h"

// Custom node with value and timestamp
struct TimestampNode : public Node {
  using value_type = int;

  std::chrono::steady_clock::time_point timestamp;
  
  explicit TimestampNode(int v) 
      : m_value(v)
      , timestamp(std::chrono::steady_clock::now()) 
  {}

  value_type m_value;
};

// Utility for thread-safe console output
struct ConsoleLogger {
  static void log(const std::string& message) {
      static std::mutex console_mutex;
      std::lock_guard<std::mutex> lock(console_mutex);
      std::cout << "[" << std::this_thread::get_id() << "] " << message << std::endl;
  }
};

// Example 1: Concurrent insertion and scanning
void concurrent_insert_scan_example() {
  Lock_free_list<TimestampNode> list;
  std::atomic<bool> stop_flag{false};
  std::atomic<int> total_insertions{0};
  std::atomic<int> total_scans{0};
  
  // Writer threads continuously insert nodes
  auto writer_func = [&](int thread_id) {
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> dis(1, 1000);
      
      while (!stop_flag.load()) {
          auto value = dis(gen);
          auto* node = new TimestampNode(value);
          list.push_front(node);
          total_insertions.fetch_add(1);
          
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
  };
  
  // Reader threads continuously scan the list
  auto reader_func = [&](int thread_id) {
    while (!stop_flag.load()) {
      int count = 0;
      auto now = std::chrono::steady_clock::now();
          
      for (const auto& node : list) {
        auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - node.timestamp).count();
        count++;
      }
          
      total_scans.fetch_add(1);
      ConsoleLogger::log("Scanned " + std::to_string(count) + " nodes");
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  };
  
  // Start threads
  std::vector<std::thread> threads;
  const int num_writers = 4;
  const int num_readers = 2;
  
  for (int i = 0; i < num_writers; ++i) {
    threads.emplace_back(writer_func, i);
  }
  for (int i = 0; i < num_readers; ++i) {
    threads.emplace_back(reader_func, i);
  }
  
  // Run for a few seconds
  std::this_thread::sleep_for(std::chrono::seconds(5));
  stop_flag.store(true);
  
  for (auto& thread : threads) {
    thread.join();
  }
  
  ConsoleLogger::log("Total insertions: " + std::to_string(total_insertions.load()));
  ConsoleLogger::log("Total scans: " + std::to_string(total_scans.load()));
}

// Example 2: Concurrent insert-after and remove
void concurrent_insert_after_remove_example() {
  Lock_free_list<TimestampNode> list;
  std::atomic<bool> stop_flag{false};
  
  // Initialize list with some nodes
  std::vector<TimestampNode*> initial_nodes;
  for (int i = 0; i < 10; ++i) {
    auto* node = new TimestampNode(i);
    initial_nodes.push_back(node);
    list.push_back(node);
  }
  
  // Thread function for insert-after operations
  auto inserter_func = [&](int thread_id) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, initial_nodes.size() - 1);
      
    while (!stop_flag.load()) {
      // Randomly select a node to insert after
      int idx = dis(gen);
      auto* target = initial_nodes[idx];
      auto* new_node = new TimestampNode(1000 + thread_id);
          
      if (list.insert_after(target, new_node)) {
        ConsoleLogger::log("Inserted after node " + std::to_string(target->m_value));
      }
          
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
  };
  
  // Thread function for remove operations
  auto remover_func = [&](int thread_id) {
   std::random_device rd;
   std::mt19937 gen(rd());
      
   while (!stop_flag.load()) {
     // Find a node to remove using iterator
     auto it = list.begin();
     std::uniform_int_distribution<> dis(0, 5);
     int skip = dis(gen);
          
     while (skip > 0 && it != list.end()) {
       ++it;
       --skip;
     }
          
     if (it != list.end()) {
       auto value = it->m_value;
       list.remove(&(*it));
       ConsoleLogger::log("Removed node " + std::to_string(value));
     }
          
     std::this_thread::sleep_for(std::chrono::milliseconds(10));
   }
  };
  
  // Thread function for list validation
  auto validator_func = [&]() {
      while (!stop_flag.load()) {
          // Validate list integrity
          auto it = list.begin();
          Node* prev = nullptr;
          
          while (it != list.end()) {
              auto* current = &(*it);
              
              // Verify prev/next pointers
              if (current->m_prev.load() != prev) {
                  ConsoleLogger::log("WARNING: Invalid prev pointer detected");
              }
              
              prev = current;
              ++it;
          }
          
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
      }
  };
  
  // Start threads
  std::vector<std::thread> threads;
  const int num_inserters = 3;
  const int num_removers = 2;
  
  for (int i = 0; i < num_inserters; ++i) {
      threads.emplace_back(inserter_func, i);
  }
  for (int i = 0; i < num_removers; ++i) {
      threads.emplace_back(remover_func, i);
  }
  threads.emplace_back(validator_func);
  
  // Run for a few seconds
  std::this_thread::sleep_for(std::chrono::seconds(5));
  stop_flag.store(true);
  
  for (auto& thread : threads) {
      thread.join();
  }
}

// Example 3: Mixed operations with periodic snapshots
void mixed_operations_example() {
  Lock_free_list<TimestampNode> list;
  std::atomic<bool> stop_flag{false};
  std::atomic<int> operation_count{0};
  
  // Thread function for mixed operations
  auto worker_func = [&](int thread_id) {
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_int_distribution<> op_dis(0, 2);  // 0=insert, 1=remove, 2=insert_after
      
      while (!stop_flag.load()) {
          int operation = op_dis(gen);
          
          switch (operation) {
              case 0: { // Insert
                  auto* node = new TimestampNode(thread_id * 1000 + operation_count.fetch_add(1));
                  list.push_front(node);
                  ConsoleLogger::log("Inserted front: " + std::to_string(node->m_value));
                  break;
              }
              case 1: { // Remove
                  auto it = list.begin();
                  if (it != list.end()) {
                      auto value = it->m_value;
                      list.remove(&(*it));
                      ConsoleLogger::log("Removed: " + std::to_string(value));
                  }
                  break;
              }
              case 2: { // Insert after
                  auto it = list.begin();
                  if (it != list.end()) {
                      auto* new_node = new TimestampNode(thread_id * 1000 + operation_count.fetch_add(1));
                      if (list.insert_after(&(*it), new_node)) {
                          ConsoleLogger::log("Inserted after: " + std::to_string(it->m_value));
                      }
                  }
                  break;
              }
          }
          
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
  };
  
  // Thread function for periodic snapshots using iterator
  auto snapshot_func = [&]() {
      while (!stop_flag.load()) {
          std::vector<int> snapshot;
          
          // Take a snapshot of the list
          for (const auto& node : list) {
              snapshot.push_back(node.m_value);
          }
          
          std::stringstream ss;
          ss << "Snapshot [" << snapshot.size() << " nodes]: ";
          for (int val : snapshot) {
              ss << val << " ";
          }
          ConsoleLogger::log(ss.str());
          
          std::this_thread::sleep_for(std::chrono::milliseconds(500));
      }
  };
  
  // Start threads
  std::vector<std::thread> threads;
  const int num_workers = 4;
  
  for (int i = 0; i < num_workers; ++i) {
      threads.emplace_back(worker_func, i);
  }
  threads.emplace_back(snapshot_func);
  
  // Run for a few seconds
  std::this_thread::sleep_for(std::chrono::seconds(5));
  stop_flag.store(true);
  
  for (auto& thread : threads) {
      thread.join();
  }
}

int main() {
  std::cout << "Running concurrent insert/scan example..." << std::endl;
  concurrent_insert_scan_example();
  
  std::cout << "\nRunning concurrent insert-after/remove example..." << std::endl;
  concurrent_insert_after_remove_example();
  
  std::cout << "\nRunning mixed operations example..." << std::endl;
  mixed_operations_example();
  
  return 0;
}

