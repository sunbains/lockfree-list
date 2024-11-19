#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <random>
#include <algorithm>

#include "timestamp_node.h"
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

TEST_F(LockFreeListTest, PushBackSingleElement) {
    auto node = createNode(42);
    list->push_back(node);
    
    auto tail = list->m_tail.load(std::memory_order_acquire);
    ASSERT_NE(tail, nullptr);
    EXPECT_EQ(static_cast<DataNode*>(tail)->m_value, 42);
    EXPECT_EQ(tail->m_next.load(), nullptr);
}

TEST_F(LockFreeListTest, PushBackMultipleElements) {
    std::vector<int> values = {1, 2, 3, 4, 5};
    for (int val : values) {
        list->push_back(createNode(val));
    }
    
    std::vector<int> actual;
    auto current = list->m_head.load(std::memory_order_acquire);
    while (current) {
        actual.push_back(static_cast<DataNode*>(current)->m_value);
        current = current->m_next.load(std::memory_order_acquire);
    }
    
    EXPECT_EQ(actual, values);
}

TEST_F(LockFreeListTest, FindExistingValue) {
    auto n1 = createNode(1);
    auto n2 = createNode(2);
    auto n3 = createNode(3);
    
    list->push_back(n1);
    list->push_back(n2);
    list->push_back(n3);
    
    auto found = list->find(2);
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(static_cast<DataNode*>(found)->m_value, 2);
}

TEST_F(LockFreeListTest, FindNonExistentValue) {
    auto n1 = createNode(1);
    auto n2 = createNode(2);
    
    list->push_back(n1);
    list->push_back(n2);
    
    auto found = list->find(3);
    EXPECT_EQ(found, nullptr);
}

TEST_F(LockFreeListTest, FindWithPredicate) {
    auto n1 = createNode(1);
    auto n2 = createNode(2);
    auto n3 = createNode(3);
    
    list->push_back(n1);
    list->push_back(n2);
    list->push_back(n3);
    
    auto found = list->find_if([](const DataNode* node) {
        return node->m_value % 2 == 0;
    });
    
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(static_cast<DataNode*>(found)->m_value, 2);
}

TEST_F(LockFreeListTest, InsertAfterMiddle) {
    auto n1 = createNode(1);
    auto n2 = createNode(2);
    auto n3 = createNode(3);
    
    list->push_back(n1);
    list->push_back(n2);
    
    bool success = list->insert_after(n1, n3);
    EXPECT_TRUE(success);
    
    std::vector<int> expected = {1, 3, 2};
    std::vector<int> actual;
    auto current = list->m_head.load(std::memory_order_acquire);
    while (current) {
        actual.push_back(static_cast<DataNode*>(current)->m_value);
        current = current->m_next.load(std::memory_order_acquire);
    }
    
    EXPECT_EQ(actual, expected);
}

TEST_F(LockFreeListTest, InsertAfterTail) {
    auto n1 = createNode(1);
    auto n2 = createNode(2);
    auto n3 = createNode(3);
    
    list->push_back(n1);
    list->push_back(n2);
    
    bool success = list->insert_after(n2, n3);
    EXPECT_TRUE(success);
    
    auto tail = list->m_tail.load(std::memory_order_acquire);
    ASSERT_NE(tail, nullptr);
    EXPECT_EQ(static_cast<DataNode*>(tail)->m_value, 3);
}

TEST_F(LockFreeListTest, ConcurrentPushBackAndFront) {
    static const int NUM_THREADS = 4;
    static const int ITEMS_PER_THREAD = 1000;
    
    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t]() {
            for (int i = 0; i < ITEMS_PER_THREAD; ++i) {
                int value = t * ITEMS_PER_THREAD + i;
                if (i % 2 == 0) {
                    list->push_front(createNode(value));
                } else {
                    list->push_back(createNode(value));
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify all elements are present
    std::vector<int> actual;
    auto current = list->m_head.load(std::memory_order_acquire);
    while (current) {
        actual.push_back(static_cast<DataNode*>(current)->m_value);
        current = current->m_next.load(std::memory_order_acquire);
    }
    
    EXPECT_EQ(actual.size(), NUM_THREADS * ITEMS_PER_THREAD);
}

TEST_F(LockFreeListTest, ConcurrentInsertAfter) {
    static const int NUM_THREADS = 4;
    static const int ITEMS_PER_THREAD = 100;
    
    // Create initial list
    auto initial = createNode(0);
    list->push_back(initial);
    
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t, initial, &success_count]() {
            for (int i = 0; i < ITEMS_PER_THREAD; ++i) {
                int value = t * ITEMS_PER_THREAD + i + 1;
                if (list->insert_after(initial, createNode(value))) {
                    success_count.fetch_add(1);
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify successful insertions
    int actual_count = 0;
    auto current = list->m_head.load(std::memory_order_acquire);
    while (current) {
        actual_count++;
        current = current->m_next.load(std::memory_order_acquire);
    }
    
    EXPECT_EQ(actual_count, success_count.load() + 1);  // +1 for initial node
}

class IteratorBasics : public ::testing::Test {
protected:
    using ListType = Lock_free_list<TimestampNode>;
    ListType list;
    std::vector<TimestampNode*> nodes;

    void SetUp() override {
        // Create some nodes for testing
        for (int i = 0; i < 5; ++i) {
            auto* node = new TimestampNode(i);
            nodes.push_back(node);
            list.push_back(node);
        }
    }

    void TearDown() override {
        for (auto* node : nodes) {
            delete node;
        }
        nodes.clear();
    }
};

// Test iterator traits and categories
TEST_F(IteratorBasics, IteratorTraits) {
    using Iterator = ListType::iterator;
    using ConstIterator = ListType::const_iterator;
    
    // Check iterator category
    static_assert(std::is_same_v<
        std::iterator_traits<Iterator>::iterator_category,
        std::bidirectional_iterator_tag
    >, "Iterator should be bidirectional");
    
    // Check value type
    static_assert(std::is_same_v<
        std::iterator_traits<Iterator>::value_type,
        TimestampNode
    >, "Value type should be TimestampNode");
    
    // Check reference type
    static_assert(std::is_same_v<
        std::iterator_traits<Iterator>::reference,
        TimestampNode&
    >, "Reference type should be TimestampNode&");
    
    // Check const iterator traits
    static_assert(std::is_same_v<
        std::iterator_traits<ConstIterator>::reference,
        const TimestampNode&
    >, "Const reference type should be const TimestampNode&");
}

// Test default construction
TEST_F(IteratorBasics, DefaultConstruction) {
    ListType::iterator it;
    ListType::const_iterator cit;
    
    EXPECT_EQ(it, ListType::iterator());
    EXPECT_EQ(cit, ListType::const_iterator());
}

// Test equality/inequality operators
TEST_F(IteratorBasics, EqualityOperators) {
    auto it1 = list.begin();
    auto it2 = list.begin();
    auto it3 = std::next(it1);
    
    EXPECT_EQ(it1, it2);
    EXPECT_NE(it1, it3);
    EXPECT_NE(it2, it3);
    
    // Test with const_iterator: This test makes no sense.
    // ListType::const_iterator cit1 = list.begin();
    // EXPECT_EQ(it1, cit1);
    // EXPECT_EQ(cit1, it1);
}

// Test increment operators
TEST_F(IteratorBasics, IncrementOperators) {
    auto it = list.begin();
    
    // Pre-increment
    auto& ref1 = ++it;
    EXPECT_EQ(&ref1, &it) << "Pre-increment should return reference";
    EXPECT_EQ(it->m_value, 1);
    
    // Post-increment
    auto old_it = it++;
    EXPECT_EQ(old_it->m_value, 1);
    EXPECT_EQ(it->m_value, 2);
}

// Test decrement operators
TEST_F(IteratorBasics, DecrementOperators) {
    auto it = std::next(list.begin(), 2);  // Point to value 2
    
    // Pre-decrement
    auto& ref1 = --it;
    EXPECT_EQ(&ref1, &it) << "Pre-decrement should return reference";
    EXPECT_EQ(it->m_value, 1);
    
    // Post-decrement
    auto old_it = it--;
    EXPECT_EQ(old_it->m_value, 1);
    EXPECT_EQ(it->m_value, 0);
}

// Test dereference operators
TEST_F(IteratorBasics, DereferenceOperators) {
    auto it = list.begin();
    
    // Operator*
    EXPECT_EQ((*it).m_value, 0);
    
    // Operator->
    EXPECT_EQ(it->m_value, 0);
    
    // Const iterator
    ListType::const_iterator cit = list.begin();
    EXPECT_EQ((*cit).m_value, 0);
    EXPECT_EQ(cit->m_value, 0);
}

// Test iterator conversion to const_iterator
TEST_F(IteratorBasics, IteratorConversion) {
    ListType::iterator it = list.begin();
    ListType::const_iterator cit = it;  // Should compile
    
    // These should not compile:
    // EXPECT_EQ(it, cit);
    // EXPECT_EQ(*it, *cit);
}

// Test begin/end consistency
TEST_F(IteratorBasics, BeginEndConsistency) {
    auto begin = list.begin();
    auto end = list.end();
    
    // Test forward iteration
    std::vector<int> values;
    for (auto it = begin; it != end; ++it) {
        values.push_back(it->m_value);
    }
    
    EXPECT_EQ(values.size(), 5);
    EXPECT_TRUE(std::is_sorted(values.begin(), values.end()));
}

// Test range-based for loop compatibility
TEST_F(IteratorBasics, RangeBasedFor) {
    std::vector<int> values;
    
    for (const auto& node : list) {
        values.push_back(node.m_value);
    }
    
    EXPECT_EQ(values.size(), 5);
    for (size_t i = 0; i < values.size(); ++i) {
        EXPECT_EQ(values[i], static_cast<int>(i));
    }
}

// Test STL algorithm compatibility
TEST_F(IteratorBasics, STLAlgorithmCompatibility) {
    // Test std::find_if
    auto it = std::find_if(list.begin(), list.end(),
                          [](const auto& node) { return node.m_value == 2; });
    EXPECT_NE(it, list.end());
    EXPECT_EQ(it->m_value, 2);
    
    // Test std::count_if
    int count = std::count_if(list.begin(), list.end(),
                             [](const auto& node) { return node.m_value % 2 == 0; });
    EXPECT_EQ(count, 3);  // 0, 2, 4 are even
    
    // Test std::distance
    auto dist = std::distance(list.begin(), list.end());
    EXPECT_EQ(dist, 5);
}

// Test iterator invalidation
TEST_F(IteratorBasics, IteratorInvalidation) {
    auto it = std::next(list.begin(), 2);  // Point to value 2
    auto value = it->m_value;
    
    // Remove node before iterator
    list.remove(nodes[1]);  // Remove value 1
    
    // Iterator should still be valid and point to the same value
    EXPECT_EQ(it->m_value, value);
    
    // Remove the node iterator points to
    list.remove(nodes[2]);  // Remove value 2
    
    // Attempting to dereference iterator now would be undefined behavior
    // We can test advancing the iterator still works
    ++it;
    EXPECT_EQ(it->m_value, 3);
}

// Test const correctness
TEST_F(IteratorBasics, ConstCorrectness) {
    const ListType& const_list = list;
    
    // These should all compile
    auto cit1 = const_list.begin();
    auto cit2 = const_list.cbegin();
    auto cit3 = list.cbegin();
    
    // These shouldn't modify the node
    EXPECT_EQ(cit1->m_value, 0);
    EXPECT_EQ((*cit2).m_value, 0);
    
    // This should not compile:
    // cit1->m_value = 42;  // Should cause compilation error
}

// Test empty list behavior
TEST_F(IteratorBasics, EmptyListBehavior) {
    ListType empty_list;
    
    EXPECT_EQ(empty_list.begin(), empty_list.end());
    EXPECT_EQ(empty_list.cbegin(), empty_list.cend());
    
    // Range-based for should work with empty list
    int count = 0;
    for ([[maybe_unused]] const auto& node : empty_list) {
        ++count;
    }
    EXPECT_EQ(count, 0);
}

TEST_F(LockFreeListTest, IteratorBasics) {
    auto n1 = createNode(1);
    auto n2 = createNode(2);
    auto n3 = createNode(3);
    
    list->push_back(n1);
    list->push_back(n2);
    list->push_back(n3);
    
    std::vector<int> values;
    for (const auto& node : *list) {
        values.push_back(node.m_value);
    }
    
    EXPECT_EQ(values, std::vector<int>({1, 2, 3}));
}

TEST_F(LockFreeListTest, IteratorBidirectional) {
    auto n1 = createNode(1);
    auto n2 = createNode(2);
    auto n3 = createNode(3);
    
    list->push_back(n1);
    list->push_back(n2);
    list->push_back(n3);
    
    auto it = list->begin();
    EXPECT_EQ(it->m_value, 1);
    ++it;
    EXPECT_EQ(it->m_value, 2);
    ++it;
    EXPECT_EQ(it->m_value, 3);
    --it;
    EXPECT_EQ(it->m_value, 2);
    --it;
    EXPECT_EQ(it->m_value, 1);
}

TEST_F(LockFreeListTest, ConstIterator) {
    auto n1 = createNode(1);
    auto n2 = createNode(2);
    
    list->push_back(n1);
    list->push_back(n2);
    
    const Lock_free_list<DataNode>& const_list = *list;
    std::vector<int> values;
    for (const auto& node : const_list) {
        values.push_back(node.m_value);
    }
    
    EXPECT_EQ(values, std::vector<int>({1, 2}));
}

TEST_F(LockFreeListTest, IteratorConcurrentModification) {
    std::vector<std::unique_ptr<DataNode>> nodes;
    for (int i = 0; i < 10; ++i) {
        nodes.push_back(std::make_unique<DataNode>(i));
        list->push_back(nodes.back().get());
    }
    
    std::thread modifier([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        list->remove(nodes[5].get());
    });
    
    std::vector<int> values;
    for (const auto& node : *list) {
        values.push_back(node.m_value);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    modifier.join();
    
    // Verify we didn't crash and got some valid sequence
    EXPECT_GT(values.size(), 0);
    
    // Verify values are in ascending order (even with removal)
    EXPECT_TRUE(std::is_sorted(values.begin(), values.end()));
}

TEST_F(LockFreeListTest, STLAlgorithms) {
    auto n1 = createNode(3);
    auto n2 = createNode(1);
    auto n3 = createNode(4);
    auto n4 = createNode(2);
    
    list->push_back(n1);
    list->push_back(n2);
    list->push_back(n3);
    list->push_back(n4);
    
    // Test with std::find_if
    auto it = std::find_if(list->begin(), list->end(),
                          [](const auto& node) { return node.m_value == 4; });
    EXPECT_NE(it, list->end());
    EXPECT_EQ(it->m_value, 4);
    
    // Test with std::count_if
    int count = std::count_if(list->begin(), list->end(),
                             [](const auto& node) { return node.m_value % 2 == 0; });
    EXPECT_EQ(count, 2);
}

