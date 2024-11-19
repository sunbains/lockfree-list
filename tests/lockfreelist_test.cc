#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <random>
#include <algorithm>

#include "lockfreelist.h"

class LockFreeListTest : public ::testing::Test {
protected:
    std::unique_ptr<Lock_free_list<DataNode>> list;
    std::vector<std::unique_ptr<DataNode>> nodes;

    void SetUp() override {
        list = std::make_unique<Lock_free_list<DataNode>>();
    }

    void TearDown() override {
        nodes.clear();
    }

    DataNode* createNode(int value) {
        nodes.push_back(std::make_unique<DataNode>(value));
        assert(nodes.back() != nullptr);
        return nodes.back().get();
    }
};

// Basic functionality tests
TEST_F(LockFreeListTest, EmptyListIsEmpty) {
    std::vector<int> values;
    auto current = list->m_head.load(std::memory_order_acquire);
    EXPECT_EQ(current, nullptr);
}

TEST_F(LockFreeListTest, PushFrontSingleElement) {
    auto node = createNode(42);
    list->push_front(node);
    
    auto head = list->m_head.load(std::memory_order_acquire);
    ASSERT_NE(head, nullptr);
    EXPECT_EQ(static_cast<DataNode*>(head)->m_value, 42);
    EXPECT_EQ(head->m_next.load(), nullptr);
}

TEST_F(LockFreeListTest, PushFrontMultipleElements) {
    std::vector<int> values = {1, 2, 3, 4, 5};
    for (int val : values) {
        list->push_front(createNode(val));
    }
    
    std::vector<int> actual;
    auto current = list->m_head.load(std::memory_order_acquire);
    while (current) {
        actual.push_back(static_cast<DataNode*>(current)->m_value);
        current = current->m_next.load(std::memory_order_acquire);
    }
    
    std::vector<int> expected = {5, 4, 3, 2, 1};
    EXPECT_EQ(actual, expected);
}

TEST_F(LockFreeListTest, RemoveMiddleNode) {
    auto n1 = createNode(1);
    auto n2 = createNode(2);
    auto n3 = createNode(3);
    
    list->push_front(n1);
    list->push_front(n2);
    list->push_front(n3);
    
    list->remove(n2);
    
    std::vector<int> actual;
    auto current = list->m_head.load(std::memory_order_acquire);
    while (current) {
        actual.push_back(static_cast<DataNode*>(current)->m_value);
        current = current->m_next.load(std::memory_order_acquire);
    }
    
    std::vector<int> expected = {3, 1};
    EXPECT_EQ(actual, expected);
}

// Concurrent operation tests
TEST_F(LockFreeListTest, ConcurrentPushFront) {
    static const int NUM_THREADS = 4;
    static const int ITEMS_PER_THREAD = 1000;
    
    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t]() {
            for (int i = 0; i < ITEMS_PER_THREAD; ++i) {
                int value = t * ITEMS_PER_THREAD + i;
                list->push_front(createNode(value));
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all elements are present (order doesn't matter)
    std::vector<int> actual;
    auto current = list->m_head.load(std::memory_order_acquire);
    while (current) {
        actual.push_back(static_cast<DataNode*>(current)->m_value);
        current = current->m_next.load(std::memory_order_acquire);
    }
    
    EXPECT_EQ(actual.size(), NUM_THREADS * ITEMS_PER_THREAD);
    
    std::sort(actual.begin(), actual.end());
    for (int i = 0; i < NUM_THREADS * ITEMS_PER_THREAD; ++i) {
        EXPECT_EQ(std::count(actual.begin(), actual.end(), i), 1);
    }
}

TEST_F(LockFreeListTest, ConcurrentPushAndRemove) {
    static const int NUM_THREADS = 4;
    static const int OPERATIONS_PER_THREAD = 1000;
    
    std::atomic<int> shared_counter{0};
    std::vector<std::thread> threads;
    
    // Half threads push, half remove
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t, &shared_counter]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 100);
            
            for (int i = 0; i < OPERATIONS_PER_THREAD; ++i) {
                if (t < NUM_THREADS/2) {
                    // Push thread
                    int value = shared_counter.fetch_add(1);
                    list->push_front(createNode(value));
                } else {
                    // Remove thread
                    auto current = list->m_head.load(std::memory_order_acquire);
                    if (current) {
                        list->remove(current);
                    }
                }
                
                // Add some randomness to thread scheduling
                if (dis(gen) > 95) {
                    std::this_thread::yield();
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify list integrity
    auto current = list->m_head.load(std::memory_order_acquire);
    while (current) {
        EXPECT_NE(current->m_next.load(std::memory_order_acquire), current);
        if (auto next = current->m_next.load(std::memory_order_acquire)) {
            EXPECT_EQ(next->m_prev.load(std::memory_order_acquire), current);
        }
        current = current->m_next.load(std::memory_order_acquire);
    }
}

