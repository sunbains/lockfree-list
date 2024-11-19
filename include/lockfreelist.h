#pragma once

#include <atomic>
#include <iostream>
#include <memory>
#include <vector>
#include <thread>
#include <cassert>

#define DEBUG

struct Node {
  virtual ~Node() = default;

  void prefetch_next() const {
    /* Read, high temporal locality */
    if (auto next = m_next.load(std::memory_order_acquire)) {
      __builtin_prefetch(next, 0, 3);
    }
  }
  
  void prefetch_prev() const {
    /* Read, high temporal locality */
    if (auto prev = m_prev.load(std::memory_order_acquire)) {
      __builtin_prefetch(prev, 0, 3);
    }
  }

  std::atomic<Node*> m_next{};
  std::atomic<Node*> m_prev{};
};

template <typename T>
struct Lock_free_list {

  class iterator;
  class const_iterator;
    
  struct iterator {
    using value_type = T;
    using pointer = T*;
    using reference = T&;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::bidirectional_iterator_tag;
        
    iterator() noexcept : m_node(nullptr) {}

    explicit iterator(Node* node, Node* prev = nullptr) noexcept 
      : m_node(node), m_prev(prev) {}
        
    reference operator*() const {
      if (m_node == nullptr) {
        throw std::runtime_error("Dereferencing null iterator");
      }
      return *static_cast<T*>(m_node);
    }
        
    pointer operator->() const {
      if (m_node == nullptr) {
        throw std::runtime_error("Dereferencing null iterator");
      }
      return static_cast<T*>(m_node);
    }
        
    iterator& operator++() {
      if (m_node == nullptr) {
        throw std::runtime_error("Incrementing null iterator");
      }
            
      /* Load next node with acquire semantics */
      auto next = m_node->m_next.load(std::memory_order_acquire);
            
      /* Verify the node hasn't been removed */
      if (m_node->m_prev.load(std::memory_order_acquire) != m_prev) {
        /* Node was removed, try to recover by finding next valid node */
        while (m_node != nullptr && m_node->m_prev.load(std::memory_order_acquire) != m_prev) {
          m_node = m_node->m_next.load(std::memory_order_acquire);
          if (m_node != nullptr) {
            m_prev = m_node->m_prev.load(std::memory_order_acquire);
          }
        }
      } else {
        m_prev = m_node;
        m_node = next;
      }
            
      return *this;
    }
        
    iterator operator++(int) {
      auto tmp = *this;

      ++(*this);
      return tmp;
    }
        
    iterator& operator--() {
      if (m_node == nullptr) {
        throw std::runtime_error("Decrementing begin iterator");
      }
            
      /* Load prev node with acquire semantics */
      auto prev = m_prev->m_prev.load(std::memory_order_acquire);
            
      /* Verify the node hasn't been removed */
      if (m_prev->m_next.load(std::memory_order_acquire) != m_node) {
        /* Node was removed, try to recover by finding previous valid node */
        while (m_prev != nullptr && m_prev->m_next.load(std::memory_order_acquire) != m_node) {
          m_prev = m_prev->m_prev.load(std::memory_order_acquire);
          if (m_prev != nullptr) {
            m_node = m_prev->m_next.load(std::memory_order_acquire);
          }
        }
      } else {
        m_node = m_prev;
        m_prev = prev;
      }
            
      return *this;
    }
        
    iterator operator--(int) {
      auto tmp = *this;
      --(*this);
      return tmp;
    }
        
    bool operator==(const iterator& rhs) const noexcept {
      return m_node == rhs.m_node;
    }
        
    bool operator!=(const iterator& rhs) const noexcept {
      return !(*this == rhs);
    }
        
    Node* m_node;

    /* Keep track of previous node for bidirectional iteration */
    Node* m_prev;
  };
    
  struct const_iterator {
    using value_type = const T;
    using pointer = const T*;
    using reference = const T&;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::bidirectional_iterator_tag;
        
    const_iterator() noexcept
      : m_node(nullptr), m_prev(nullptr) {}

    const_iterator(const iterator& other) noexcept 
      : m_node(other.m_node), m_prev(other.m_prev) {}

    explicit const_iterator(const Node* node, const Node* prev = nullptr) noexcept 
      : m_node(node), m_prev(prev) {}
        
    reference operator*() const {
      if (m_node == nullptr) {
        throw std::runtime_error("Dereferencing null iterator");
      }
      return *static_cast<const T*>(m_node);
    }
        
    pointer operator->() const {
      if (m_node == nullptr) {
        throw std::runtime_error("Dereferencing null iterator");
      }
      return static_cast<const T*>(m_node);
    }
        
    const_iterator& operator++() {
      if (m_node == nullptr) {
        throw std::runtime_error("Incrementing null iterator");
      }
            
      auto next = m_node->m_next.load(std::memory_order_acquire);
            
      if (m_node->m_prev.load(std::memory_order_acquire) != m_prev) {
        while (m_node != nullptr && m_node->m_prev.load(std::memory_order_acquire) != m_prev) {
          m_node = m_node->m_next.load(std::memory_order_acquire);
          if (m_node != nullptr) {
            m_prev = m_node->m_prev.load(std::memory_order_acquire);
          }
        }
      } else {
        m_prev = m_node;
        m_node = next;
      }
            
      return *this;
    }
        
    const_iterator operator++(int) {
      auto tmp = *this;
      ++(*this);
      return tmp;
    }
        
    const_iterator& operator--() {
      if (m_prev == nullptr) {
        throw std::runtime_error("Decrementing begin iterator");
      }
            
      auto prev = m_prev->m_prev.load(std::memory_order_acquire);
            
      if (m_prev->m_next.load(std::memory_order_acquire) != m_node) {
        while (m_prev && m_prev->m_next.load(std::memory_order_acquire) != m_node) {
          m_prev = m_prev->m_prev.load(std::memory_order_acquire);
          if (m_prev != nullptr) {
            m_node = m_prev->m_next.load(std::memory_order_acquire);
          }
        }
      } else {
        m_node = m_prev;
        m_prev = prev;
      }
            
      return *this;
    }
        
    const_iterator operator--(int) {
      auto tmp = *this;
      --(*this);
      return tmp;
    }
        
    bool operator==(const const_iterator& other) const noexcept {
      return m_node == other.m_node;
    }
        
    bool operator!=(const const_iterator& other) const noexcept {
      return !(*this == other);
    }
        
    const Node* m_node;
    const Node* m_prev;
  };
    
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

    /* Prefetch head node and its neighbors */
    auto head = m_head.load(std::memory_order_acquire);

    if (head != nullptr) {
      /* Write, high temporal locality */
      __builtin_prefetch(head, 1, 3);
      head->prefetch_next();
   }

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

    /* Prefetch adjacent nodes */
    if (prev != nullptr) {
      __builtin_prefetch(prev, 1, 3);
      prev->prefetch_next();
    }

    if (next != nullptr) {
      __builtin_prefetch(next, 1, 3);
      next->prefetch_prev();
    }

    if (prev != nullptr) {
      /* Prefetch new head */
      if (next != nullptr) {
        __builtin_prefetch(next, 1, 3);
      }

      prev->m_next.store(next, std::memory_order_release);
    } else {
      /* Prefetch for potential subsequent operations */
      if (auto new_head = m_head.load(std::memory_order_acquire)) {
        __builtin_prefetch(new_head, 0, 3);
      }
      m_head.compare_exchange_strong(node, next, std::memory_order_release);
    }

    if (next != nullptr) {
      next->m_prev.store(prev, std::memory_order_release);
    } else {
      m_tail.compare_exchange_strong(node, prev, std::memory_order_release);
    }
  }

  /* Add a node to the back */
  void push_back(Node* node) {
    assert(node != nullptr);
    node->m_next.store(nullptr, std::memory_order_relaxed);
    node->m_prev.store(nullptr, std::memory_order_relaxed);
    
    for (;;) {
      auto old_tail = m_tail.load(std::memory_order_acquire);

      if (old_tail == nullptr) {
        /* Empty list - try to set both head and tail */
        if (m_head.load(std::memory_order_acquire) == nullptr) {
          Node *null_node = nullptr;

          node->m_next.store(nullptr, std::memory_order_relaxed);
          if (m_head.compare_exchange_weak(null_node, node, std::memory_order_release, std::memory_order_relaxed)) {
            m_tail.store(node, std::memory_order_release);
            return;
          }
        }
        continue;
      }
        
      /* Try to link after the current tail */
      node->m_prev.store(old_tail, std::memory_order_relaxed);
      old_tail->m_next.store(node, std::memory_order_release);
      
      /* Try to update tail */
      if (m_tail.compare_exchange_weak(old_tail, node, std::memory_order_release, std::memory_order_relaxed)) {
          return;
      }
        
      /* If CAS failed, someone else modified the tail */
      /* Reset the next pointer of old_tail to avoid leaving dangling pointers */
      old_tail->m_next.store(nullptr, std::memory_order_release);
    }
  }

  /* Insert a node after a specific node */
  bool insert_after(Node* node, Node* new_node) {
    assert(node != nullptr);
    assert(new_node != nullptr);
    
    new_node->m_next.store(nullptr, std::memory_order_relaxed);
    new_node->m_prev.store(nullptr, std::memory_order_relaxed);
    
    for (;;) {
      /* Load the next node that follows our insertion point */
      auto next = node->m_next.load(std::memory_order_acquire);
        
      /* Verify node is still in the list by checking its prev link */
      auto prev_check = node->m_prev.load(std::memory_order_acquire);

      if (prev_check != nullptr) {
        if (prev_check->m_next.load(std::memory_order_acquire) != node) {
          return false; // Node was removed from list
        }
      } else {
        /* If node claims to be head, verify */
        if (m_head.load(std::memory_order_acquire) != node) {
          return false; // Node was removed from list
        }
      }
        
      /* Set up new node's links */
      new_node->m_next.store(next, std::memory_order_relaxed);
      new_node->m_prev.store(node, std::memory_order_relaxed);
        
      /* Try to link new_node after node */
      if (node->m_next.compare_exchange_weak(next, new_node, std::memory_order_release, std::memory_order_relaxed)) {
        /* Successfully linked new_node after node */
            
        if (next != nullptr) {
          /* Update next node's prev pointer */
          next->m_prev.store(new_node, std::memory_order_release);
        } else {
          /* new_node is the new tail */
          Node* expected = node;

          if (!m_tail.compare_exchange_strong(expected, new_node, std::memory_order_release, std::memory_order_relaxed)) {
            /* Tail update failed - someone else modified the list We need to retry */
            continue;
          }
        }
            
        return true;
      }
      /* If CAS failed, another thread modified the list, try again */
    }
  }

  /* Find a node with a specific value */
  template<typename Predicate>
  Node* find_if(Predicate pred) {
    for (;;) {
      auto current = m_head.load(std::memory_order_acquire);
        
      while (current != nullptr) {
        /* Check if current node matches predicate */
        if (pred(static_cast<T*>(current))) {
          /* Verify node is still in list by checking its links */
          auto next = current->m_next.load(std::memory_order_acquire);
          auto prev = current->m_prev.load(std::memory_order_acquire);
                
          /* If node's next->prev points back to node, it's still valid */
          if (next != nullptr) {
            if (next->m_prev.load(std::memory_order_acquire) != current) {
              /* Node was removed, restart search */
              break;
            }
          } else if (m_tail.load(std::memory_order_acquire) != current) {
            /* Node was tail but no longer is */
            break;
          }
                
          /* If node's prev->next points to node, it's still valid */
          if (prev != nullptr) {
            if (prev->m_next.load(std::memory_order_acquire) != current) {
              /* Node was removed, restart search */
              break;
            }
          } else if (m_head.load(std::memory_order_acquire) != current) {
            /* Node was head but no longer is */
            break;
          }
                
          return current;
        }
        current = current->m_next.load(std::memory_order_acquire);
      }
        
      /* If we completed iteration without finding a match, return nullptr */
      if (current == nullptr) {
        return nullptr;
      }
        
      /* If we broke out of the loop due to concurrent modification, retry */
    }
  }

  /* Convenience method to find by value */
  Node* find(const typename T::value_type& value) {
    return find_if([&value](const T* node) {
      return node->m_value == value;
    });
  }

  /* Clear the list */
  void clear() {
    /* The containing node should be deleted by the list owne*/
    m_head.store(nullptr, std::memory_order_relaxed);
    m_tail.store(nullptr, std::memory_order_relaxed);
  }

  iterator begin() noexcept {
    return iterator(m_head.load(std::memory_order_acquire), nullptr);
  }
    
  const_iterator begin() const noexcept {
      return const_iterator(m_head.load(std::memory_order_acquire), nullptr);
  }
    
  const_iterator cbegin() const noexcept {
      return const_iterator(m_head.load(std::memory_order_acquire), nullptr);
  }
  
  iterator end() noexcept {
      return iterator(nullptr, m_tail.load(std::memory_order_acquire));
  }
  
  const_iterator end() const noexcept {
      return const_iterator(nullptr, m_tail.load(std::memory_order_acquire));
  }
  
  const_iterator cend() const noexcept {
      return const_iterator(nullptr, m_tail.load(std::memory_order_acquire));
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

  std::atomic<Node*> m_head{};
  std::atomic<Node*> m_tail{};

};

struct DataNode : public Node {
  using value_type = int;

  explicit DataNode(int v)
    : m_value(v) {}

  value_type m_value;
};
