# Lock-Free Doubly Linked List

A high-performance, thread-safe, lock-free doubly linked list implementation in C++17. This implementation provides safe concurrent access and modification without using traditional locks, making it suitable for high-throughput, low-latency applications.

## Features

- **Lock-Free Operations**: All operations are lock-free, ensuring progress even under contention
- **Thread Safety**: Safe concurrent access and modification from multiple threads
- **STL-Compatible Iterators**: Bidirectional iterators that work with STL algorithms
- **Memory Safety**: Memory has to be reclaimed by the caller
- **Cache-Friendly**: Optimized for modern CPU architectures with prefetching and cache-aware design
- **Comprehensive Testing**: Extensive unit tests and benchmarks
- **Formal Verification**: TLA+ specifications for core operations

## Requirements

- C++17 compliant compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.14 or higher
- Google Test (for testing)
- Google Benchmark (for benchmarking)

## Installation

### Using CMake

```bash
git clone https://github.com/yourusername/lockfree-list.git
cd lockfree-list
mkdir build && cd build
cmake ..
make
```

### Header-Only Usage

Simply copy `include/lockfreelist.h` to your project and include it:

```cpp
#include "lockfreelist.h"
```

## Usage

### Basic Usage

```cpp
#include "lockfreelist.h"

// Create a custom node type
struct MyNode : public Node {
    int value;
    explicit MyNode(int v) : value(v) {}
};

// Create a list
Lock_free_list<MyNode> list;

// Add elements
list.push_front(new MyNode(1));
list.push_back(new MyNode(2));

// Iterate through the list
for (const auto& node : list) {
    std::cout << node.value << " ";
}

// Find elements
auto* node = list.find(2);
if (node) {
    list.remove(node);
}
```

### Advanced Usage with TimestampNode

```cpp
#include "lockfreelist.h"

// Create and use a timestamped node
auto node = make_timestamp_node(42);
list.push_front(node.get());

// Access time-based information
std::cout << "Node age: " << node->age_ms() << "ms\n";

// Find expired nodes
using namespace std::chrono_literals;
auto expired = node_utils::find_expired(list.begin(), list.end(), 1s);
```

## Performance

Performance benchmarks showing operations/second under different scenarios:

| Operation    | Single Thread | 4 Threads | 8 Threads |
|-------------|---------------|-----------|-----------|
| push_front  | 5M ops/s     | 2M ops/s  | 1M ops/s  |
| push_back   | 4.8M ops/s   | 1.9M ops/s| 950K ops/s|
| find        | 10M ops/s    | 8M ops/s  | 6M ops/s  |
| iteration   | 15M ops/s    | 12M ops/s | 10M ops/s |

## API Reference

### Core Operations

- `push_front(Node* node)`: Add a node to the front
- `push_back(Node* node)`: Add a node to the back
- `insert_after(Node* node, Node* new_node)`: Insert after a specific node
- `remove(Node* node)`: Remove a specific node
- `find(const T& value)`: Find a node by value
- `clear()`: Remove all nodes

### Iterator Operations

- `begin()`, `end()`: Get iterators for the list
- `cbegin()`, `cend()`: Get const iterators
- Supports bidirectional iteration, range-based for loops

### Memory Management

- The user is responsible for deleting the node that is removed from the list.

## Implementation Details

### Lock-Free Mechanisms

- Uses atomic operations for thread safety
- ABA problem prevention (using 4 bits from the pointer, so not 100% fool proof)
- Memory ordering guarantees

### Cache Optimization

- Node pooling for better locality
- Prefetching hints
- Aligned memory allocation
- Cache-line aware design

## Testing

Run the tests:

```bash
cd build
ctest
```

Run benchmarks:

```bash
./lockfree_list_bench
```

## Contributing

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add some amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## Verification

The implementation includes TLA+ specifications for formal verification of the core algorithms. View the specifications in the `tla` directory.

## License

This project is licensed under the Apache v2.0 license- see the LICENSE file for details.

## Credits

- Implementation inspired by various lock-free algorithms research papers
- Special thanks to the C++ community for feedback and improvements

## Citation

If you use this implementation in your research, please cite:

```bibtex
@misc{lockfreelist,
  author = {Sunny Bains},
  title = {Lock-Free Doubly Linked List},
  year = {2024},
  publisher = {GitHub},
  url = {https://github.com/sunbains/lockfree-list}
}
```

## Contact

Your Name - [@sunbains](https://twitter.com/sunbains)

Project Link: [https://github.com/sunbains/lockfree-list](https://github.com/sunbains/lockfree-list)

