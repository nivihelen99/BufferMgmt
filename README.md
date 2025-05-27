# Network Packet Buffer Pool Manager

A high-performance C++ library for efficient packet buffer management in layer 2 network switch software. Designed for zero-copy packet processing with NUMA awareness and sub-100-cycle allocation performance.

## 🚀 Features

- **Ultra-Fast Allocation**: Sub-100 CPU cycle allocation/deallocation
- **Zero-Copy Support**: Reference counting for shared packet buffers
- **NUMA Optimized**: Memory locality awareness for multi-socket systems
- **Thread-Safe**: Lock-free algorithms for concurrent access
- **Configurable Pools**: Multiple buffer sizes with automatic expansion
- **Rich Metadata**: Extensible packet metadata system
- **Memory Efficient**: <5% overhead, cache-line aligned buffers
- **Production Ready**: Comprehensive testing and debugging support

## 🎯 Use Cases

- Layer 2 Ethernet switches
- Network packet processors
- High-frequency trading systems
- Network security appliances
- Load balancers and proxies
- Any high-performance packet processing application

## 📊 Performance

```
Allocation Latency:     < 100 CPU cycles
Throughput:            > 100M buffers/sec/core
Memory Overhead:       < 5%
Supported Cores:       Up to 64
NUMA Nodes:           Up to 8
Max Buffer Memory:     1GB+
```

## 🏗️ Quick Start

### Installation

```bash
git clone https://github.com/your-org/packet-buffer-pool.git
cd packet-buffer-pool
mkdir build && cd build
cmake ..
make -j$(nproc)
sudo make install
```

### Basic Usage

```cpp
#include <packet_buffer/pool_manager.hpp>

int main() {
    // Initialize the pool manager
    PoolManager& pool = PoolManager::instance();
    pool.initialize();
    
    // Allocate a buffer for 1500-byte packet
    PacketBuffer* buffer = pool.allocate(1500);
    
    // Use the buffer
    uint8_t* data = buffer->data();
    memcpy(data, packet_data, packet_size);
    
    // Set metadata
    buffer->metadata().set_ingress_port(5);
    buffer->metadata().set_vlan(100);
    
    // Process packet...
    
    // Release buffer
    pool.deallocate(buffer);
    return 0;
}
```

### Zero-Copy Example

```cpp
// Zero-copy packet forwarding
PacketBuffer* original = pool.allocate(1500);
// ... fill with data ...

// Create reference for forwarding (no data copy)
PacketBuffer* forwarded = original->add_ref();

// Send to multiple ports
forward_to_port(1, forwarded);
forward_to_port(2, original->add_ref());
forward_to_port(3, original->add_ref());

// Release references (buffer freed when ref count reaches 0)
original->release();
forwarded->release();
```

### NUMA-Aware Allocation

```cpp
// Get current NUMA node
int numa_node = get_numa_node();

// Allocate from local NUMA memory
PacketBuffer* buffer = pool.allocate(1500, numa_node);
```

## 📚 API Reference

### Core Classes

#### `PoolManager`
Singleton managing all buffer pools
```cpp
static PoolManager& instance();
void initialize(const Config& config = {});
PacketBuffer* allocate(size_t size, int numa_node = -1);
void deallocate(PacketBuffer* buffer);
Statistics get_statistics() const;
```

#### `PacketBuffer`
Individual packet buffer with metadata
```cpp
uint8_t* data();                    // Packet data pointer
size_t size() const;                // Buffer size
size_t capacity() const;            // Total capacity
BufferMetadata& metadata();         // Access metadata
PacketBuffer* add_ref();            // Increment ref count
void release();                     // Decrement ref count
void chain(PacketBuffer* next);     // Chain buffers
```

#### `BufferMetadata`
Packet metadata storage
```cpp
void set_ingress_port(uint16_t port);
void set_egress_port(uint16_t port);
void set_vlan(uint16_t vlan);
void set_timestamp(uint64_t ts);
// ... and more
```

## ⚙️ Configuration

### Runtime Configuration

```cpp
PoolManager::Config config;
config.pool_sizes = {64, 128, 256, 512, 1024, 2048, 9216};
config.initial_buffers_per_pool = 1000;
config.numa_aware = true;
config.enable_statistics = true;
config.per_thread_cache_size = 32;

PoolManager::instance().initialize(config);
```

### CMake Integration

```cmake
find_package(PacketBufferPool REQUIRED)
target_link_libraries(your_target PacketBufferPool::PacketBufferPool)
```

### pkg-config

```bash
gcc $(pkg-config --cflags --libs packet-buffer-pool) -o myapp myapp.cpp
```

## 🔧 Building

### Requirements

- C++17 compatible compiler (GCC 9+, Clang 10+)
- CMake 3.15+
- Linux x86_64 (primary support)
- NUMA development libraries (optional)

### Build Options

```bash
# Standard build
cmake -DCMAKE_BUILD_TYPE=Release ..

# With DPDK support
cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_DPDK=ON ..

# Debug build with sanitizers
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON ..

# Build benchmarks
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON ..
```

## 🧪 Testing

```bash
# Run unit tests
make test

# Run performance benchmarks
./build/benchmarks/buffer_benchmark

# Run stress tests
./build/tests/stress_test --duration=60 --threads=16
```

## 📈 Monitoring

### Statistics API

```cpp
auto stats = pool.get_statistics();
std::cout << "Allocations: " << stats.total_allocations << std::endl;
std::cout << "Pool utilization: " << stats.pool_utilization << "%" << std::endl;
std::cout << "Cache hit rate: " << stats.cache_hit_rate << "%" << std::endl;
```

### Integration with Monitoring Systems

```cpp
// Export metrics to Prometheus
auto exporter = PrometheusExporter::create();
pool.register_metrics_exporter(exporter);

// Custom statistics callback
pool.set_statistics_callback([](const Statistics& stats) {
    if (stats.pool_utilization > 90) {
        alert("Buffer pool utilization high: " + 
              std::to_string(stats.pool_utilization) + "%");
    }
});
```

## 🐛 Debugging

### Memory Leak Detection

```cpp
#ifdef DEBUG
pool.enable_leak_detection(true);
pool.dump_active_buffers();  // Shows all allocated buffers
#endif
```

### Buffer Tracing

```cpp
// Enable detailed buffer lifecycle tracking
pool.enable_buffer_tracing(true);

// Dump buffer history for debugging
pool.dump_buffer_trace(buffer_id);
```

## 🚀 Performance Tuning

### NUMA Optimization

```bash
# Bind application to specific NUMA node
numactl --cpunodebind=0 --membind=0 ./your_app

# Check NUMA allocation
numastat -p $(pidof your_app)
```

### CPU Affinity

```cpp
// Set thread affinity for optimal performance
cpu_set_t cpuset;
CPU_ZERO(&cpuset);
CPU_SET(core_id, &cpuset);
pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
```

### Hugepages Setup

```bash
# Enable hugepages
echo 1024 > /proc/sys/vm/nr_hugepages

# Mount hugepages
mkdir -p /mnt/hugepages
mount -t hugetlbfs hugetlbfs /mnt/hugepages
```

## 📖 Examples

See the `examples/` directory for complete working examples:

- `basic_usage.cpp` - Simple allocation/deallocation
- `zero_copy.cpp` - Reference counting and buffer sharing
- `numa_aware.cpp` - NUMA-optimized allocation
- `packet_processor.cpp` - Complete L2 packet processing loop
- `performance_test.cpp` - Latency and throughput measurement

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

Please ensure all tests pass and add new tests for any new functionality.

## 📄 License



## 🙏 Acknowledgments

- Inspired by DPDK's mbuf design
- NUMA optimization techniques from Linux kernel
- Lock-free algorithms from research papers
- Performance testing methodology from network industry best practices


---

*Built with ❤️ for high-performance networking*
