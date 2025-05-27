#ifndef PACKET_BUFFER_POOL_HPP
#define PACKET_BUFFER_POOL_HPP

#include "packet_buffer.hpp" // Assumes PacketBuffer definition is complete
#include <vector>
#include <cstddef> // For size_t
#include <mutex>   // For current std::vector-based free_list_
#include <atomic>  // For statistics

// Forward declaration if PoolManager uses it, or include if PoolManager members are here
// class PoolManager; 

class PacketBufferPool {
public:
    PacketBufferPool(size_t buffer_payload_size, // Actual data capacity for packets
                     size_t initial_count, 
                     int numa_node = -1, 
                     size_t headroom = 64, 
                     size_t tailroom = 0);
    ~PacketBufferPool();

    PacketBuffer* allocate_buffer();
    void deallocate_buffer(PacketBuffer* buffer); // Called by PacketBuffer::release()

    size_t get_buffer_payload_size() const; // Returns configured payload size
    size_t get_initial_pool_count() const; // Total number of buffers this pool was created with
    size_t get_free_count() const;
    int get_numa_node() const;
    size_t get_headroom_size() const;
    size_t get_tailroom_size() const;

    // Basic statistics
    size_t get_alloc_count() const;
    size_t get_dealloc_count() const;
    // size_t get_high_water_mark() const; // Requires tracking: current_allocated_count_

private:
    bool initialize_pool(); // Helper to allocate and set up all buffers

    // Configuration stored from constructor
    size_t buffer_payload_size_; // User-requested payload size
    size_t initial_pool_count_;
    int numa_node_;
    size_t headroom_size_;
    size_t tailroom_size_;

    // Calculated size for the entire memory block for one buffer unit
    // (metadata + PacketBuffer obj + headroom + payload + tailroom)
    size_t single_buffer_unit_alloc_size_; 

    // Raw memory for all buffers in this pool.
    // This pointer owns the memory for all PacketBuffer objects and their data.
    unsigned char* pool_memory_block_ = nullptr; 
                                             
    std::vector<PacketBuffer*> free_list_; // Simple free list using std::vector
    std::mutex list_mutex_; // Protects free_list_

    std::atomic<size_t> alloc_count_{0};
    std::atomic<size_t> dealloc_count_{0};
    // std::atomic<size_t> current_allocated_count_{0}; // For high_water_mark, can be added later
    
    // FR-002: Pool expansion related (placeholders for now)
    // bool expand_pool(size_t additional_count);
};
#endif // PACKET_BUFFER_POOL_HPP
