#include <benchmark/benchmark.h>
#include <thread>
#include <vector>
#include <random>
#include <algorithm>
#include <sstream>
#include <iomanip>

#include "timestamp_node.h"


// Utility function to populate list
template<typename T>
void populate_list(Lock_free_list<T>& list, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        list.push_back(new T(static_cast<int>(i)));
    }
}

template<typename T>
void free_list(Lock_free_list<T>& list) {
    for (auto it = list.begin(); it != list.end();) {
        list.remove(&(*it));
        delete &(*it);
        it = list.begin();
    }
    list.clear();
}

// Basic forward iteration
static void BM_IteratorForward(benchmark::State& state) {
    Lock_free_list<TimestampNode> list;
    populate_list(list, state.range(0));
    
    for (auto _ : state) {
        int sum = 0;
        for (const auto& node : list) {
            benchmark::DoNotOptimize(sum += node.m_value);
        }
        benchmark::ClobberMemory();
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_IteratorForward)
    ->Range(8, 8<<10)
    ->UseRealTime();

#if 0
// Reverse iteration
// Note:: This test makes no sense, it should be rend() and rbegin()
static void BM_IteratorReverse(benchmark::State& state) {
    Lock_free_list<TimestampNode> list;
    populate_list(list, state.range(0));
    
    for (auto _ : state) {
        int sum = 0;
        for (auto it = list.end(); it != list.begin(); --it) {
            benchmark::DoNotOptimize(sum += it->m_value);
        }
        benchmark::ClobberMemory();
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_IteratorReverse)
    ->Range(8, 8<<10)
    ->UseRealTime();
#endif

// Random access using iterators
static void BM_IteratorRandomAccess(benchmark::State& state) {
    Lock_free_list<TimestampNode> list;
    populate_list(list, state.range(0));
    
    std::vector<size_t> indices;
    for (size_t i = 0; i < state.range(0); ++i) {
        indices.push_back(i);
    }
    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(indices.begin(), indices.end(), gen);
    
    for (auto _ : state) {
        int sum = 0;
        for (size_t idx : indices) {
            auto it = list.begin();
            std::advance(it, idx);
            benchmark::DoNotOptimize(sum += it->m_value);
        }
        benchmark::ClobberMemory();
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_IteratorRandomAccess)
    ->Range(8, 8<<10)
    ->UseRealTime();

// Concurrent iteration with modifications
static void BM_IteratorConcurrentModification(benchmark::State& state) {
    Lock_free_list<TimestampNode> list;
    populate_list(list, state.range(0));
    
    std::atomic<bool> stop_flag{false};
    std::atomic<size_t> total_iterations{0};
    
    for (auto _ : state) {
        state.PauseTiming();
        stop_flag.store(false);
        total_iterations.store(0);
        
        // Start modifier thread
        std::thread modifier([&]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 1);
            
            while (!stop_flag.load()) {
                if (dis(gen)) {
                    // Insert
                    list.push_front(new TimestampNode(1));
                } else {
                    // Remove
                    auto it = list.begin();
                    if (it != list.end()) {
                        list.remove(&(*it));
                    }
                }
                std::this_thread::yield();
            }
        });
        
        state.ResumeTiming();
        
        // Measure iteration performance
        int sum = 0;
        for (const auto& node : list) {
            benchmark::DoNotOptimize(sum += node.m_value);
            total_iterations.fetch_add(1);
        }
        
        state.PauseTiming();
        stop_flag.store(true);
        modifier.join();
        state.ResumeTiming();
        
        benchmark::ClobberMemory();
    }
    
    state.SetItemsProcessed(total_iterations.load());
}
BENCHMARK(BM_IteratorConcurrentModification)
    ->Range(8, 8<<10)
    ->UseRealTime();

// Multiple concurrent iterators
static void BM_MultipleConcurrentIterators(benchmark::State& state) {
    Lock_free_list<TimestampNode> list;
    populate_list(list, state.range(0));
    
    const int num_threads = state.range(1);
    std::atomic<bool> stop_flag{false};
    std::atomic<size_t> total_iterations{0};
    
    for (auto _ : state) {
        state.PauseTiming();
        stop_flag.store(false);
        total_iterations.store(0);
        std::vector<std::thread> threads;
        
        state.ResumeTiming();
        
        // Create multiple iterator threads
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&]() {
                while (!stop_flag.load()) {
                    int sum = 0;
                    for (const auto& node : list) {
                        benchmark::DoNotOptimize(sum += node.m_value);
                        total_iterations.fetch_add(1);
                    }
                    benchmark::ClobberMemory();
                }
            });
        }
        
        // Let them run for a while
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        state.PauseTiming();
        stop_flag.store(true);
        for (auto& thread : threads) {
            thread.join();
        }
        state.ResumeTiming();
    }
    
    state.SetItemsProcessed(total_iterations.load());
}
BENCHMARK(BM_MultipleConcurrentIterators)
    ->Ranges({{8, 8<<10}, {1, 8}})
    ->UseRealTime();

// Iterator creation and destruction overhead
static void BM_IteratorCreation(benchmark::State& state) {
    Lock_free_list<TimestampNode> list;
    populate_list(list, state.range(0));
    
    for (auto _ : state) {
        auto it = list.begin();
        benchmark::DoNotOptimize(it);
        benchmark::ClobberMemory();
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_IteratorCreation)
    ->Range(8, 8<<10)
    ->UseRealTime();

// Compare iterator vs direct pointer traversal
static void BM_IteratorVsPointer(benchmark::State& state) {
    Lock_free_list<TimestampNode> list;
    populate_list(list, state.range(0));
    
    if (state.thread_index() == 0) {
        // Iterator traversal
        for (auto _ : state) {
            int sum = 0;
            for (const auto& node : list) {
                benchmark::DoNotOptimize(sum += node.m_value);
            }
            benchmark::ClobberMemory();
        }
    } else {
        // Direct pointer traversal
        for (auto _ : state) {
            int sum = 0;
            auto* current = list.m_head.load(std::memory_order_acquire);
            while (current) {
                benchmark::DoNotOptimize(sum += static_cast<TimestampNode*>(current)->m_value);
                current = current->m_next.load(std::memory_order_acquire);
            }
            benchmark::ClobberMemory();
        }
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_IteratorVsPointer)
    ->Range(8, 8<<10)
    ->UseRealTime()
    ->Threads(2);

// Find with iterator vs find method
static void BM_FindComparison(benchmark::State& state) {
    Lock_free_list<TimestampNode> list;
    populate_list(list, state.range(0));
    
    const int target_value = state.range(0) / 2;
    
    if (state.thread_index() == 0) {
        // Using iterator
        for (auto _ : state) {
            auto it = std::find_if(list.begin(), list.end(),
                [target_value](const auto& node) {
                    return node.m_value == target_value;
                });
            benchmark::DoNotOptimize(it);
        }
    } else {
        // Using find method
        for (auto _ : state) {
            auto* node = list.find(target_value);
            benchmark::DoNotOptimize(node);
        }
    }
    
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_FindComparison)
    ->Range(8, 8<<10)
    ->UseRealTime()
    ->Threads(2);

// Cache-friendly iteration pattern
static void BM_CacheFriendlyIteration(benchmark::State& state) {
    Lock_free_list<TimestampNode> list;
    populate_list(list, state.range(0));
    
    if (state.thread_index() == 0) {
        // Standard iteration
        for (auto _ : state) {
            int sum = 0;
            for (const auto& node : list) {
                benchmark::DoNotOptimize(sum += node.m_value);
            }
            benchmark::ClobberMemory();
        }
    } else {
        // Prefetching iteration
        for (auto _ : state) {
            int sum = 0;
            auto it = list.begin();
            while (it != list.end()) {
                // Prefetch next node
                auto next_it = it;
                ++next_it;
                if (next_it != list.end()) {
                    __builtin_prefetch(&(*next_it), 0, 3);
                }
                
                benchmark::DoNotOptimize(sum += it->m_value);
                ++it;
            }
            benchmark::ClobberMemory();
        }
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_CacheFriendlyIteration)
    ->Range(8, 8<<10)
    ->UseRealTime()
    ->Threads(2);

BENCHMARK_MAIN();

static void BM_BatchOperations(benchmark::State& state) {
    Lock_free_list<TimestampNode> list;
    const size_t batch_size = state.range(1);

    for (auto _ : state) {
        state.PauseTiming();
        populate_list(list, state.range(0));
        std::vector<TimestampNode*> nodes_to_process;
        state.ResumeTiming();

        // Collect batch of nodes using iterator
        auto it = list.begin();
        size_t count = 0;
        while (it != list.end() && count < batch_size) {
            nodes_to_process.push_back(&(*it));
            ++it;
            ++count;
        }

        // Process batch
        for (auto* node : nodes_to_process) {
            benchmark::DoNotOptimize(node->m_value *= 2);
        }

        state.PauseTiming();
        free_list(list);
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * batch_size);
}
BENCHMARK(BM_BatchOperations)
    ->Ranges({{1<<10, 1<<15}, {16, 256}})
    ->UseRealTime();

// Sliding window using iterators
static void BM_SlidingWindow(benchmark::State& state) {
    Lock_free_list<TimestampNode> list;
    const size_t window_size = state.range(1);

    for (auto _ : state) {
        state.PauseTiming();
        populate_list(list, state.range(0));
        state.ResumeTiming();

        auto window_begin = list.begin();
        auto window_end = window_begin;

        // Advance window_end to create initial window
        for (size_t i = 0; i < window_size && window_end != list.end(); ++i) {
            ++window_end;
        }

        // Slide window through list
        while (window_end != list.end()) {
            int sum = 0;
            auto it = window_begin;
            while (it != window_end) {
                benchmark::DoNotOptimize(sum += it->m_value);
                ++it;
            }
            ++window_begin;
            ++window_end;
            benchmark::ClobberMemory();
        }

        state.PauseTiming();
        free_list(list);
        state.ResumeTiming();
    }
}
BENCHMARK(BM_SlidingWindow)
    ->Ranges({{1<<10, 1<<15}, {16, 256}})
    ->UseRealTime();

// Iterator stability under high contention
static void BM_IteratorStability(benchmark::State& state) {
    Lock_free_list<TimestampNode> list;
    std::atomic<bool> stop_flag{false};

    for (auto _ : state) {
        state.PauseTiming();
        populate_list(list, state.range(0));
        stop_flag.store(false);

        // Start multiple modifier threads
        std::vector<std::thread> modifier_threads;
        for (int i = 0; i < state.range(1); ++i) {
            modifier_threads.emplace_back([&]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(0, 2);

                while (!stop_flag.load()) {
                    switch (dis(gen)) {
                        case 0:
                            list.push_front(new TimestampNode(1));
                            break;
                        case 1:
                            if (auto it = list.begin(); it != list.end()) {
                                list.remove(&(*it));
                            }
                            break;
                        case 2:
                            if (auto it = list.begin(); it != list.end()) {
                                list.insert_after(&(*it), new TimestampNode(2));
                            }
                            break;
                    }
                    std::this_thread::yield();
                }
            });
        }

        state.ResumeTiming();

        // Measure iterator stability
        size_t successful_iterations = 0;
        for (int i = 0; i < 100; ++i) {
            bool iteration_completed = true;
            for (const auto& node : list) {
                if (node.m_value < 0) { // Should never happen
                    iteration_completed = false;
                    break;
                }
            }
            if (iteration_completed) {
                ++successful_iterations;
            }
        }

        state.PauseTiming();
        stop_flag.store(true);
        for (auto& thread : modifier_threads) {
            thread.join();
        }
        free_list(list);
        state.ResumeTiming();

        benchmark::DoNotOptimize(successful_iterations);
    }
}
BENCHMARK(BM_IteratorStability)
    ->Ranges({{1<<10, 1<<15}, {1, 8}})
    ->UseRealTime();

// Iterator with predicate filtering
static void BM_IteratorFiltering(benchmark::State& state) {
    Lock_free_list<TimestampNode> list;

    for (auto _ : state) {
        state.PauseTiming();
        populate_list(list, state.range(0));
        state.ResumeTiming();

        if (state.thread_index() == 0) {
            // Using iterator with if condition
            int sum = 0;
            for (const auto& node : list) {
                if (node.m_value % 2 == 0) {
                    benchmark::DoNotOptimize(sum += node.m_value);
                }
            }
        } else {
            // Using std::find_if repeatedly
            int sum = 0;
            auto it = list.begin();
            while (it != list.end()) {
                it = std::find_if(it, list.end(),
                    [](const auto& node) { return node.m_value % 2 == 0; });
                if (it != list.end()) {
                    benchmark::DoNotOptimize(sum += it->m_value);
                    ++it;
                }
            }
        }

        state.PauseTiming();
        list.clear();
        state.ResumeTiming();
    }
}
BENCHMARK(BM_IteratorFiltering)
    ->Range(8, 8<<10)
    ->UseRealTime()
    ->Threads(8);

// Iterator with distance calculation
static void BM_IteratorDistance(benchmark::State& state) {
    Lock_free_list<TimestampNode> list;

    for (auto _ : state) {
        state.PauseTiming();
        populate_list(list, state.range(0));
        auto mid_point = list.begin();
        std::advance(mid_point, state.range(0) / 2);
        state.ResumeTiming();

        if (state.thread_index() == 0) {
            // Calculate distance by iteration
            size_t distance = 0;
            auto it = list.begin();
            while (it != mid_point) {
                ++distance;
                ++it;
            }
            benchmark::DoNotOptimize(distance);
        } else {
            // Calculate distance using std::distance
            auto distance = std::distance(list.begin(), mid_point);
            benchmark::DoNotOptimize(distance);
        }

        state.PauseTiming();
        free_list(list);
        state.ResumeTiming();
    }
}
BENCHMARK(BM_IteratorDistance)
    ->Range(8, 8<<10)
    ->UseRealTime()
    ->Threads(2);

BENCHMARK(BM_IteratorDistance)
    ->Range(8, 8<<10)
    ->UseRealTime()
    ->Threads(4);

BENCHMARK(BM_IteratorDistance)
    ->Range(8, 8<<10)
    ->UseRealTime()
    ->Threads(8);

BENCHMARK(BM_IteratorDistance)
    ->Range(8, 8<<10)
    ->UseRealTime()
    ->Threads(16);

BENCHMARK(BM_IteratorDistance)
    ->Range(8, 8<<10)
    ->UseRealTime()
    ->Threads(32);

// Benchmark for iterator reuse vs creation
static void BM_IteratorReuse(benchmark::State& state) {
    Lock_free_list<TimestampNode> list;

    for (auto _ : state) {
        state.PauseTiming();
        populate_list(list, state.range(0));
        state.ResumeTiming();

        if (state.thread_index() == 0) {
            // Create new iterator each time
            for (int i = 0; i < 100; ++i) {
                int sum = 0;
                for (auto it = list.begin(); it != list.end(); ++it) {
                    benchmark::DoNotOptimize(sum += it->m_value);
                }
            }
        } else {
            // Reuse iterator
            auto it = list.begin();
            for (int i = 0; i < 100; ++i) {
                int sum = 0;
                for (it = list.begin(); it != list.end(); ++it) {
                    benchmark::DoNotOptimize(sum += it->m_value);
                }
            }
        }

        state.PauseTiming();
        free_list(list);
        state.ResumeTiming();
    }
}
BENCHMARK(BM_IteratorReuse)
    ->Range(8, 8<<10)
    ->UseRealTime()
    ->Threads(2);
BENCHMARK(BM_IteratorReuse)
    ->Range(8, 8<<10)
    ->UseRealTime()
    ->Threads(4);
BENCHMARK(BM_IteratorReuse)
    ->Range(8, 8<<10)
    ->UseRealTime()
    ->Threads(8);
BENCHMARK(BM_IteratorReuse)
    ->Range(8, 8<<10)
    ->UseRealTime()
    ->Threads(16);
BENCHMARK(BM_IteratorReuse)
    ->Range(8, 8<<10)
    ->UseRealTime()
    ->Threads(32);

