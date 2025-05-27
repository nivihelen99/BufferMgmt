#include "packet_buffer.hpp"
#include "buffer_metadata.hpp" 
#include "packet_buffer_pool.hpp" // For PacketBuffer::release to call owning_pool_->deallocate_buffer

// Constructor as per include/packet_buffer.hpp
PacketBuffer::PacketBuffer(
    PacketBufferPool* pool,                     // Owning pool
    unsigned char* buffer_block_start_param,    // Start of the raw memory block allocated by the pool for this buffer unit
    size_t total_block_size_param,            // Total size of that raw memory block
    unsigned char* data_area_start_ptr,         // Pointer to where the [Headroom|Payload|Tailroom] region begins
    size_t data_payload_capacity_val,           // Capacity of just the Payload part
    size_t configured_headroom, 
    size_t configured_tailroom, 
    BufferMetadata* metadata_ptr_param,         // Pointer to the BufferMetadata instance
    int numa_node_val
)
: owning_pool_(pool),
  buffer_start_(data_area_start_ptr),      // Start of the [H|P|T] data region
  total_allocated_size_(configured_headroom + data_payload_capacity_val + configured_tailroom), // Total size of the [H|P|T] data region
  data_ptr_(data_area_start_ptr + configured_headroom), // Data begins after initial headroom
  data_len_(0),                              // Initially, no data
  headroom_(configured_headroom),            // Store initial configured headroom
  tailroom_(configured_tailroom),            // Store initial configured tailroom
  metadata_(metadata_ptr_param),
  numa_node_(numa_node_val),
  ref_count_(0), // Initialized to 0. Pool sets to 1 on allocation.
  next_(nullptr)
{
    // buffer_block_start_param and total_block_size_param refer to the memory block
    // where BufferMetadata object and PacketBuffer object themselves are placed, followed by the data area.
    // These are not directly stored as members if PacketBuffer only cares about its data area region.
    // The current PacketBuffer members (buffer_start_, total_allocated_size_) are for the data area [H|P|T].
}

PacketBuffer::~PacketBuffer() {
    // Memory (for the data area and the PacketBuffer/BufferMetadata objects if embedded) 
    // is owned and managed by the PacketBufferPool.
    // This destructor doesn't need to free that memory.
    // If metadata_ was allocated with 'new' separately by PacketBuffer itself (which it isn't in current design),
    // then 'delete metadata_;' would be here.
    // If next_ pointed to a buffer that also needs release, that logic would be elsewhere (e.g. list clear).
}

PacketBuffer* PacketBuffer::add_ref() {
    ref_count_.fetch_add(1, std::memory_order_relaxed);
    return this;
}

void PacketBuffer::release() {
    // fetch_sub returns the value BEFORE subtraction.
    // If it was 1 (meaning this is the last reference), it becomes 0, and we deallocate.
    if (ref_count_.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        if (owning_pool_) {
            // Reset buffer state before returning to the pool
            data_ptr_ = buffer_start_ + headroom_; // Reset data pointer to start after initial headroom
            data_len_ = 0;
            next_ = nullptr;
            
            if (metadata_) {
                 metadata_->set_state(BufferMetadata::BufferState::Released); // Or ::Free
            }
            owning_pool_->deallocate_buffer(this);
        }
        // If no owning_pool_, it's an orphaned buffer; memory will leak if not managed externally.
    }
}

int PacketBuffer::ref_count() const { 
    return ref_count_.load(std::memory_order_relaxed); 
}

unsigned char* PacketBuffer::data() const { 
    return data_ptr_; 
}

size_t PacketBuffer::capacity() const { 
    // This is the payload capacity, i.e., total_allocated_size_ (for [H|P|T]) 
    // minus the initial configured headroom and tailroom.
    return total_allocated_size_ - headroom_ - tailroom_;
}

size_t PacketBuffer::data_len() const { 
    return data_len_; 
}

void PacketBuffer::set_data_len(size_t len) { 
    // Check if the new length is valid within the available space.
    // Available space from data_ptr_ onwards is (buffer_start_ + total_allocated_size_) - data_ptr_
    if (len > ( (buffer_start_ + total_allocated_size_) - data_ptr_) ) {
        // Error: not enough space for this length.
        // Option: throw, or truncate. Current behavior is truncate (as in previous version).
        // This check ensures data_ptr_ + len does not go out of bounds of the allocated data region.
        data_len_ = (buffer_start_ + total_allocated_size_) - data_ptr_;
    } else {
        data_len_ = len;
    }
}

// Returns the initial configured headroom size.
size_t PacketBuffer::headroom_size() const { 
    return headroom_; 
}

// Returns the initial configured tailroom size.
size_t PacketBuffer::tailroom_size() const { 
    return tailroom_; 
}

// Makes more of the pre-allocated headroom usable by moving data_ptr_ back.
// Increases data_len_ by the same amount as the data is now considered "prepended".
unsigned char* PacketBuffer::reserve_headroom(size_t len) { 
    size_t current_dynamic_headroom = static_cast<size_t>(data_ptr_ - buffer_start_);
    if (len > current_dynamic_headroom) { 
        return nullptr; // Not enough dynamic headroom available to reserve
    }
    data_ptr_ -= len;
    data_len_ += len; // The newly "reserved" space is now part of the data
    return data_ptr_; 
}

// Consumes 'len' bytes from the pre-allocated tailroom, making it part of the data.
// This is more like "append_to_tailroom_and_commit".
// The subtask wording "Increase tailroom_ by len" is interpreted as consuming tailroom space for data.
unsigned char* PacketBuffer::reserve_tailroom(size_t len) { 
    size_t current_dynamic_tailroom = static_cast<size_t>((buffer_start_ + total_allocated_size_) - (data_ptr_ + data_len_));
    if (len > current_dynamic_tailroom) {
        return nullptr; // Not enough dynamic tailroom available
    }
    // The data effectively grows into the tailroom space.
    // We return a pointer to where this new data should be written (which is current end of data).
    unsigned char* write_ptr = data_ptr_ + data_len_;
    data_len_ += len; 
    // The 'tailroom_' member (initial tailroom) is not changed. Dynamic tailroom reduces.
    return write_ptr; // User can write 'len' bytes starting here.
}


PacketBuffer* PacketBuffer::next_buffer() const { 
    return next_; 
}

void PacketBuffer::set_next_buffer(PacketBuffer* next) { 
    next_ = next; 
}

BufferMetadata* PacketBuffer::metadata() { 
    return metadata_; 
}

// This was for the old design. Now metadata_ is passed in constructor.
// void PacketBuffer::set_metadata_ptr(BufferMetadata* md_ptr) {
//     metadata_ = md_ptr;
// }

int PacketBuffer::get_numa_node() const { 
    return numa_node_; 
}
