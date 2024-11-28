#include <benchmark/benchmark.h>
#include <thread>
#include <vector>
#include <random>

#include "tests/timestamp_node.h"

// Single-threaded push_front benchmark
static void BM_PushFront(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        ut::Lock_free_list<DataNode> list;
        std::vector<std::unique_ptr<DataNode>> nodes;
        nodes.reserve(state.range(0));
        state.ResumeTiming();
        
        for (int i = 0; i < state.range(0); ++i) {
            nodes.push_back(std::make_unique<DataNode>(i));
            list.push_front(nodes.back().get());
        }
    }
}
BENCHMARK(BM_PushFront)->Range(8, 8<<10);

// Multi-threaded push_front benchmark
static void BM_PushFront_MultiThreaded(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        ut::Lock_free_list<DataNode> list;
        std::vector<std::unique_ptr<DataNode>> nodes;
        nodes.reserve(state.range(0));
        const int num_threads = state.range(1);
        const int items_per_thread = state.range(0) / num_threads;
        state.ResumeTiming();
        
        std::vector<std::thread> threads;
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&list, &nodes, t, items_per_thread]() {
                for (int i = 0; i < items_per_thread; ++i) {
                    auto node = new DataNode(t * items_per_thread + i);
                    list.push_front(node);
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
    }
}
BENCHMARK(BM_PushFront_MultiThreaded)
    ->Ranges({{8, 8<<10}, {1, 8}});

// Mixed operations benchmark
static void BM_MixedOperations(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        ut::Lock_free_list<DataNode> list;
        std::vector<std::unique_ptr<DataNode>> nodes;
        nodes.reserve(state.range(0));
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 1);
        state.ResumeTiming();
        
        for (int i = 0; i < state.range(0); ++i) {
            if (dis(gen)) {  // 50% chance to push
                nodes.push_back(std::make_unique<DataNode>(i));
                list.push_front(nodes.back().get());
            } else {  // 50% chance to remove
                auto head = list.m_head.load(std::memory_order_acquire);

                if (head != nullptr) {
                    list.remove(head);
                }
            }
        }
    }
}
BENCHMARK(BM_MixedOperations)->Range(8, 8<<10);

// High-contention benchmark
static void BM_HighContention(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        ut::Lock_free_list<DataNode> list;
        const int num_threads = state.range(0);
        const int operations_per_thread = 1000;
        state.ResumeTiming();
        
        std::vector<std::thread> threads;
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&list, operations_per_thread]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(0, 1);
                
                for (int i = 0; i < operations_per_thread; ++i) {
                    if (dis(gen)) {
                        list.push_front(new DataNode(i));
                    } else {
                        auto head = list.m_head.load(std::memory_order_acquire);

                        if (head != nullptr) {
                            list.remove(head);
                        }
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
    }
}
BENCHMARK(BM_HighContention)->Range(1, 32);

static void BM_PushBack(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        ut::Lock_free_list<DataNode> list;
        std::vector<std::unique_ptr<DataNode>> nodes;
        nodes.reserve(state.range(0));
        state.ResumeTiming();
        
        for (int i = 0; i < state.range(0); ++i) {
            nodes.push_back(std::make_unique<DataNode>(i));
            list.push_back(nodes.back().get());
        }
    }
}
BENCHMARK(BM_PushBack)->Range(8, 8<<10);

// Multi-threaded push_back benchmark
static void BM_PushBack_MultiThreaded(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        ut::Lock_free_list<DataNode> list;
        std::vector<std::unique_ptr<DataNode>> nodes;
        nodes.reserve(state.range(0));
        const int num_threads = state.range(1);
        const int items_per_thread = state.range(0) / num_threads;
        state.ResumeTiming();
        
        std::vector<std::thread> threads;
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&list, &nodes, t, items_per_thread]() {
                for (int i = 0; i < items_per_thread; ++i) {
                    auto node = new DataNode(t * items_per_thread + i);
                    list.push_back(node);
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
    }
}
BENCHMARK(BM_PushBack_MultiThreaded)
    ->Ranges({{8, 8<<10}, {1, 8}});

// Find operation benchmark
static void BM_Find(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        ut::Lock_free_list<DataNode> list;
        std::vector<std::unique_ptr<DataNode>> nodes;
        const int size = state.range(0);
        nodes.reserve(size);
        
        // Populate list
        for (int i = 0; i < size; ++i) {
            nodes.push_back(std::make_unique<DataNode>(i));
            list.push_back(nodes.back().get());
        }
        
        state.ResumeTiming();
        
        // Search for random values
        for (int i = 0; i < 100; ++i) {
            int value = rand() % size;
            auto found = list.find(value);
            benchmark::DoNotOptimize(found);
        }
    }
}
BENCHMARK(BM_Find)->Range(8, 8<<10);

// Insert after benchmark
static void BM_InsertAfter(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        ut::Lock_free_list<DataNode> list;
        std::vector<std::unique_ptr<DataNode>> nodes;
        nodes.reserve(state.range(0) + 1);
        
        // Create initial node
        auto initial = new DataNode(0);
        list.push_back(initial);
        nodes.emplace_back(initial);
        
        state.ResumeTiming();
        
        for (int i = 1; i <= state.range(0); ++i) {
            auto node = new DataNode(i);
            nodes.emplace_back(node);
            list.insert_after(initial, node);
        }
    }
}
BENCHMARK(BM_InsertAfter)->Range(8, 8<<10);

// Concurrent mixed operations benchmark
static void BM_ConcurrentMixedOps(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        ut::Lock_free_list<DataNode> list;
        const int num_threads = state.range(0);
        const int operations_per_thread = 1000;
        state.ResumeTiming();
        
        std::vector<std::thread> threads;
        for (int t = 0; t < num_threads; ++t) {
            threads.emplace_back([&list, operations_per_thread]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(0, 2);
                
                auto initial = new DataNode(0);
                list.push_back(initial);
                
                for (int i = 0; i < operations_per_thread; ++i) {
                    switch (dis(gen)) {
                        case 0:
                            list.push_front(new DataNode(i));
                            break;
                        case 1:
                            list.push_back(new DataNode(i));
                            break;
                        case 2: 
                            list.insert_after(initial, new DataNode(i));
                            break;
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
    }
}
BENCHMARK(BM_ConcurrentMixedOps)->Range(1, 32);

BENCHMARK_MAIN();

