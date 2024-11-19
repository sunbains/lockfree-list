#include <atomic>
#include <iostream>
#include <memory>
#include <vector>
#include <thread>
#include <cassert>

#define DEBUG

struct Node {
  std::atomic<Node*> m_next{};
  std::atomic<Node*> m_prev{};
};

template <typename T>
struct Lock_free_list {
  Lock_free_list() {
    m_head.store(nullptr, std::memory_order_relaxed);
    m_tail.store(nullptr, std::memory_order_relaxed);
  }

  ~Lock_free_list() {
    clear();
  }

  /* Add a node to the front */
  void push_front(Node* node) {
    assert(node != nullptr);

    node->m_next.store(nullptr, std::memory_order_relaxed);
    node->m_prev.store(nullptr, std::memory_order_relaxed);

    for (;;) {
      auto old_head = m_head.load(std::memory_order_acquire);

      node->m_next.store(old_head, std::memory_order_relaxed);

      if (m_head.compare_exchange_weak(old_head, node, std::memory_order_release, std::memory_order_relaxed)) {
        if (old_head != nullptr) {
          old_head->m_prev.store(node, std::memory_order_release);
        } else {
          m_tail.store(node, std::memory_order_release);
        }
        return;
      }
    }
  }

  /* Remove a specific node */
  void remove(Node* node) {
    /* Update the prev pointer */
    auto prev = node->m_prev.load(std::memory_order_acquire);
    auto next = node->m_next.load(std::memory_order_acquire);

    if (prev != nullptr) {
        prev->m_next.store(next, std::memory_order_release);
    } else {
      m_head.compare_exchange_strong(node, next, std::memory_order_release);
    }

    if (next != nullptr) {
      next->m_prev.store(prev, std::memory_order_release);
    } else {
      m_tail.compare_exchange_strong(node, prev, std::memory_order_release);
    }
  }

  /* Clear the list */
  void clear() {
    /* The containing node should be deleted by the list owne*/
    m_head.store(nullptr, std::memory_order_relaxed);
    m_tail.store(nullptr, std::memory_order_relaxed);
  }

  /* Print the list */
  void print() {
    auto current = m_head.load(std::memory_order_acquire);

    while (current != nullptr) {
      std::cout << static_cast<T*>(current)->m_value << " ";
      current = current->m_next.load(std::memory_order_acquire);
    }

    std::cout << std::endl;
  }

  std::atomic<Node*> m_head;
  std::atomic<Node*> m_tail;
};

struct DataNode : public Node {
  explicit DataNode(int v)
    : m_value(v) {}

  int m_value;
};
