#ifndef POOL_MANAGER_HPP
#define POOL_MANAGER_HPP

#include "packet_buffer.hpp"       // For PacketBuffer type
#include "packet_buffer_pool.hpp"  // For PacketBufferPool type
#include <vector>
#include <map>
#include <mutex>
#include <memory> // For std::unique_ptr

struct PoolConfig {
    size_t buffer_size;     // Payload size
    size_t initial_count;
    size_t headroom = 64;   // Default, can be overridden
    size_t tailroom = 0;    // Default
    // int numa_node = -1; // If not specified per-pool here, manager can assign it
};

class PoolManager {
public:
    static PoolManager& instance();

    // Configuration
    // Configure pools for a specific NUMA node, or globally if numa_node is -1
    bool configure_pools_for_numa_node(int numa_node, const std::vector<PoolConfig>& configs);
    // Simpler configuration for a single pool type on a given NUMA node
    bool add_pool(int numa_node, const PoolConfig& config);

    PacketBuffer* allocate(size_t desired_payload_size, int numa_node = -1);
    void deallocate(PacketBuffer* buffer); // May not be the primary path

    void print_stats() const; // For diagnostics

private:
    PoolManager();
    ~PoolManager();

    PoolManager(const PoolManager&) = delete;
    PoolManager& operator=(const PoolManager&) = delete;

    // Key: NUMA node ID (-1 for 'any' or 'unspecified', or if NUMA is not supported/detected)
    // Value: Map of (buffer_payload_size -> PacketBufferPool unique_ptr)
    std::map<int, std::map<size_t, std::unique_ptr<PacketBufferPool>>> numa_pools_;
    mutable std::mutex manager_mutex_; // Protects numa_pools_

    PacketBufferPool* find_pool(size_t desired_payload_size, int numa_node) const;
};
#endif // POOL_MANAGER_HPP
