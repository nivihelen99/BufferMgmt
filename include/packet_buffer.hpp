#ifndef PACKET_BUFFER_HPP
#define PACKET_BUFFER_HPP

#include <atomic>
#include <cstddef> // For size_t

// Forward declarations
class BufferMetadata;
class PacketBufferPool;

class PacketBuffer {
public:
    // Constructor
    PacketBuffer(PacketBufferPool* pool, unsigned char* buffer_block_start, size_t total_block_size, 
                 unsigned char* data_area_start, size_t data_area_capacity,
                 size_t headroom, size_t tailroom, BufferMetadata* metadata_ptr, int numa_node = -1);
    ~PacketBuffer();

    // Reference counting
    PacketBuffer* add_ref();
    void release();
    int ref_count() const;

    // Data access
    unsigned char* data() const; // Returns pointer to the start of packet data (after headroom)
    size_t capacity() const;    // Total buffer capacity (excluding metadata, but including headroom/tailroom)
    size_t data_len() const;    // Current length of packet data
    void set_data_len(size_t len);

    // Headroom & Tailroom
    size_t headroom_size() const;
    size_t tailroom_size() const;
    unsigned char* reserve_headroom(size_t len); // Returns pointer to new start of data
    unsigned char* reserve_tailroom(size_t len); // Returns pointer to start of tailroom reservation

    // Chaining (basic for now)
    PacketBuffer* next_buffer() const;
    void set_next_buffer(PacketBuffer* next);

    // Metadata
    BufferMetadata* metadata(); // Implementation will be in .cpp

    // NUMA node
    int get_numa_node() const;

private:
    unsigned char* buffer_start_ = nullptr;       // Start of the data region [headroom|payload|tailroom]
    size_t total_allocated_size_ = 0;       // Total size of the data region [headroom|payload|tailroom]

    unsigned char* data_ptr_ = nullptr;           // Pointer to where current packet data starts (within the payload part)
    size_t data_len_ = 0;                    // Current length of the packet data

    size_t headroom_ = 0;                     // Initial configured headroom size
    size_t tailroom_ = 0;                     // Initial configured tailroom size


    std::atomic<int> ref_count_{0};          // Atomic reference counter, initialized to 0 by constructor (pool sets to 1 on alloc)
    PacketBuffer* next_ = nullptr;               // For buffer chaining
    BufferMetadata* metadata_ = nullptr;         // Pointer to associated metadata
    int numa_node_ = -1;                       // NUMA node affinity
    PacketBufferPool* owning_pool_ = nullptr;    // Pointer to the pool that owns this buffer

    // Friend class for pool to access private members if necessary for allocation/deallocation
    // (though with owning_pool_ and public methods, this might be less needed)
    friend class PacketBufferPool;
};

#endif // PACKET_BUFFER_HPP
