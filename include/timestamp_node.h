#pragma once

#include "lockfreelist.h"

struct TimestampNode : public Node {
    using value_type = int;

    using clock_type = std::chrono::steady_clock;
    using time_point = clock_type::time_point;
    using duration = clock_type::duration;
    
    int m_value;
    time_point timestamp;
    std::atomic<uint64_t> access_count{0};  // Track number of times node is accessed
    
    explicit TimestampNode(int v) 
        : m_value(v)
        , timestamp(clock_type::now()) 
    {}
    
    // Copy constructor with new timestamp
    TimestampNode(const TimestampNode& other)
        : m_value(other.m_value)
        , timestamp(clock_type::now())
    {}
    
    // Get age of node in milliseconds
    int64_t age_ms() const {
        auto now = clock_type::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - timestamp).count();
    }
    
    // Get age of node in seconds
    double age_seconds() const {
        auto now = clock_type::now();
        return std::chrono::duration<double>(now - timestamp).count();
    }
    
    // Format timestamp as string
    std::string timestamp_string() const {
        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
    
    // Track node access
    void record_access() {
        access_count.fetch_add(1, std::memory_order_relaxed);
    }
    
    // Get access count
    uint64_t get_access_count() const {
        return access_count.load(std::memory_order_relaxed);
    }
    
    // Reset access count
    void reset_access_count() {
        access_count.store(0, std::memory_order_relaxed);
    }
    
    // Update timestamp to current time
    void update_timestamp() {
        timestamp = clock_type::now();
    }
    
    // Check if node is older than specified duration
    template<typename Duration>
    bool is_older_than(Duration duration) const {
        return (clock_type::now() - timestamp) > duration;
    }
    
    // Prefetch hints for cache optimization
    void prefetch() const {
        __builtin_prefetch(this, 0, 3);  // Read access, high temporal locality
    }
    
    // String representation of the node
    std::string to_string() const {
        std::stringstream ss;
        ss << "Value: " << m_value 
           << ", Age: " << age_ms() << "ms"
           << ", Accesses: " << get_access_count();
        return ss.str();
    }
    
    // Comparison operators
    bool operator<(const TimestampNode& other) const {
        return m_value < other.m_value;
    }
    
    bool operator==(const TimestampNode& other) const {
        return m_value == other.m_value;
    }
    
    // Static helper methods
    static TimestampNode* create_node(int value) {
        return new TimestampNode(value);
    }
    
    static std::vector<TimestampNode*> create_nodes(const std::vector<int>& values) {
        std::vector<TimestampNode*> nodes;
        nodes.reserve(values.size());
        for (int val : values) {
            nodes.push_back(create_node(val));
        }
        return nodes;
    }
    
    // Custom deleter for use with smart pointers
    struct Deleter {
        void operator()(TimestampNode* node) const {
            delete node;
        }
    };
};

// Helper for creating unique_ptr with TimestampNode
using TimestampNodePtr = std::unique_ptr<TimestampNode, TimestampNode::Deleter>;

// Factory function for creating smart pointer to TimestampNode
inline TimestampNodePtr make_timestamp_node(int value) {
    return TimestampNodePtr(new TimestampNode(value));
}

// Utility functions for time-based operations
namespace node_utils {
    // Find oldest node in a range
    template<typename Iterator>
    Iterator find_oldest(Iterator begin, Iterator end) {
        if (begin == end) return end;
        Iterator oldest = begin;
        for (auto it = std::next(begin); it != end; ++it) {
            if (it->timestamp < oldest->timestamp) {
                oldest = it;
            }
        }
        return oldest;
    }
    
    // Find nodes older than specified duration
    template<typename Iterator, typename Duration>
    std::vector<Iterator> find_expired(Iterator begin, Iterator end, Duration max_age) {
        std::vector<Iterator> expired;
        auto now = TimestampNode::clock_type::now();
        for (auto it = begin; it != end; ++it) {
            if ((now - it->timestamp) > max_age) {
                expired.push_back(it);
            }
        }
        return expired;
    }
    
    // Calculate average age of nodes in range
    template<typename Iterator>
    double average_age_seconds(Iterator begin, Iterator end) {
        if (begin == end) return 0.0;
        
        double total_age = 0.0;
        size_t count = 0;
        
        for (auto it = begin; it != end; ++it) {
            total_age += it->age_seconds();
            ++count;
        }
        
        return total_age / count;
    }
}

