#include <benchmark/benchmark.h>
#include <thread>
#include <vector>
#include <random>

#include "lockfreelist.h"

// Single-threaded push_front benchmark
static void BM_PushFront(benchmark::State& state) {
    for (auto _ : state) {
        state.PauseTiming();
        Lock_free_list<DataNode> list;
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
        Lock_free_list<DataNode> list;
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
        Lock_free_list<DataNode> list;
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
        Lock_free_list<DataNode> list;
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

BENCHMARK_MAIN();

