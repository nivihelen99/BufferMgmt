#include "gtest/gtest.h"
#include "packet_buffer.hpp"
#include "packet_buffer_pool.hpp" // For PacketBufferPool base class
#include "buffer_metadata.hpp"
#include <memory> // For std::make_shared

// Dummy pool for testing PacketBuffer release behavior.
class DummyPacketBufferPoolForTest : public PacketBufferPool {
public:
    DummyPacketBufferPoolForTest(size_t payload_size = 128, size_t count = 1, size_t headroom = 0) 
        : PacketBufferPool(payload_size, count, -1, headroom, 0), 
          deallocated_count(0), 
          last_deallocated_buffer(nullptr) {
        // This dummy pool does not perform its own memory block allocation from base.
        // Tests will provide memory for PacketBuffer objects.
    }

    // Override deallocate_buffer to track calls for testing purposes
    void deallocate_buffer(PacketBuffer* buffer) override {
        deallocated_count++;
        last_deallocated_buffer = buffer;
        // In this dummy version, the test case is responsible for the actual memory deallocation
        // of raw_mem, which includes the PacketBuffer and BufferMetadata objects.
    }

    int deallocated_count = 0;
    PacketBuffer* last_deallocated_buffer = nullptr;
};

// Helper function to create a PacketBuffer with simulated memory layout
// This mirrors how the real PacketBufferPool would set up a PacketBuffer
PacketBuffer* create_simulated_pb(PacketBufferPool* pool, 
                                  unsigned char* raw_memory, 
                                  size_t unit_size,
                                  size_t payload_size,
                                  size_t headroom,
                                  size_t tailroom,
                                  int numa_node = -1) {
    BufferMetadata* meta = new (raw_memory) BufferMetadata();
    PacketBuffer* buffer = new (raw_memory + sizeof(BufferMetadata)) PacketBuffer(
        pool,                                              // owning_pool
        raw_memory,                                        // buffer_block_start_param (start of entire unit)
        unit_size,                                         // total_block_size_param (size of entire unit)
        raw_memory + sizeof(BufferMetadata) + sizeof(PacketBuffer), // data_area_start_ptr (start of H+P+T region)
        payload_size,                                      // data_payload_capacity_val
        headroom,                                          // configured_headroom
        tailroom,                                          // configured_tailroom
        meta,                                              // metadata_ptr_param
        numa_node                                          // numa_node_val
    );
    return buffer;
}

TEST(PacketBufferTest, ConstructorInitialState) {
    auto dummy_pool = std::make_shared<DummyPacketBufferPoolForTest>(128, 1, 32);
    size_t headroom = 32;
    size_t payload_size = 128;
    size_t tailroom = 16;
    size_t unit_size = sizeof(BufferMetadata) + sizeof(PacketBuffer) + headroom + payload_size + tailroom;
    unsigned char* raw_mem = new unsigned char[unit_size];

    PacketBuffer* buffer = create_simulated_pb(dummy_pool.get(), raw_mem, unit_size, payload_size, headroom, tailroom);

    ASSERT_NE(buffer, nullptr);
    ASSERT_NE(buffer->metadata(), nullptr);
    ASSERT_EQ(buffer->ref_count(), 0); // Constructor sets to 0
    ASSERT_EQ(buffer->owning_pool_, dummy_pool.get());
    ASSERT_EQ(buffer->headroom_size(), headroom); // Initial configured headroom
    ASSERT_EQ(buffer->tailroom_size(), tailroom); // Initial configured tailroom
    ASSERT_EQ(buffer->capacity(), payload_size);
    ASSERT_EQ(buffer->data_len(), 0);
    ASSERT_EQ(buffer->data_ptr_, buffer->buffer_start_ + headroom); // data_ptr at start of payload

    // Clean up (PacketBuffer destructor not called as memory is managed externally here)
    buffer->metadata()->~BufferMetadata(); // Explicit destructor for placement new
    buffer->~PacketBuffer();             // Explicit destructor for placement new
    delete[] raw_mem;
}


TEST(PacketBufferTest, RefCountingAndRelease) {
    auto dummy_pool = std::make_shared<DummyPacketBufferPoolForTest>();
    
    size_t headroom = 32;
    size_t payload_size = 128;
    size_t tailroom = 0;
    size_t unit_size = sizeof(BufferMetadata) + sizeof(PacketBuffer) + headroom + payload_size + tailroom;
    unsigned char* raw_mem = new unsigned char[unit_size];

    PacketBuffer* buffer = create_simulated_pb(dummy_pool.get(), raw_mem, unit_size, payload_size, headroom, tailroom);
    
    ASSERT_EQ(buffer->ref_count(), 0); // Initialized to 0 by constructor
    
    // Simulate pool allocating it
    buffer->add_ref(); 
    ASSERT_EQ(buffer->ref_count(), 1);

    buffer->add_ref();
    ASSERT_EQ(buffer->ref_count(), 2);
    ASSERT_EQ(dummy_pool->deallocated_count, 0);

    buffer->release();
    ASSERT_EQ(buffer->ref_count(), 1);
    ASSERT_EQ(dummy_pool->deallocated_count, 0);

    // Before final release, check data_ptr and data_len are reset by PacketBuffer::release()
    // Let's modify them first
    buffer->set_data_len(10);
    unsigned char* modified_data_ptr = buffer->reserve_headroom(5); // data_ptr moves back, data_len increases
    ASSERT_NE(modified_data_ptr, nullptr);

    buffer->release(); // Should go to 0 and call pool's deallocate
    ASSERT_EQ(dummy_pool->deallocated_count, 1);
    ASSERT_EQ(dummy_pool->last_deallocated_buffer, buffer);
    
    // Verify that PacketBuffer::release reset the state
    // initial_payload_start is buffer_start_ + headroom_
    ASSERT_EQ(buffer->data_ptr_, buffer->buffer_start_ + headroom);
    ASSERT_EQ(buffer->data_len(), 0);


    // Clean up the simulated memory
    // Destructors for metadata and buffer were called by PacketBuffer::release if it owned them,
    // but here they are part of raw_mem. The pool doesn't delete raw_mem.
    // Explicitly call destructors because of placement new.
    buffer->metadata()->~BufferMetadata();
    buffer->~PacketBuffer();
    delete[] raw_mem; 
}

TEST(PacketBufferTest, DataManipulation) {
    auto dummy_pool = std::make_shared<DummyPacketBufferPoolForTest>();
    size_t headroom = 32;
    size_t payload_size = 128;
    size_t tailroom = 16;
    size_t unit_size = sizeof(BufferMetadata) + sizeof(PacketBuffer) + headroom + payload_size + tailroom;
    unsigned char* raw_mem = new unsigned char[unit_size];

    PacketBuffer* buffer = create_simulated_pb(dummy_pool.get(), raw_mem, unit_size, payload_size, headroom, tailroom);
    buffer->add_ref(); // Simulate allocated by pool

    // Initial state
    ASSERT_EQ(buffer->data(), buffer->buffer_start_ + headroom);
    ASSERT_EQ(buffer->capacity(), payload_size);
    ASSERT_EQ(buffer->headroom_size(), headroom); // Configured headroom
    ASSERT_EQ(buffer->tailroom_size(), tailroom); // Configured tailroom
    ASSERT_EQ(buffer->data_len(), 0);

    // Set data length
    buffer->set_data_len(50);
    ASSERT_EQ(buffer->data_len(), 50);
    // Dynamic tailroom should decrease
    // Dynamic tailroom = (buffer_start_ + total_allocated_size_) - (data_ptr_ + data_len_)
    // total_allocated_size_ for the data area is headroom + payload_size + tailroom
    // data_ptr_ is buffer_start_ + headroom
    // So, dynamic tailroom = (buffer_start_ + H+P+T) - (buffer_start_ + H + 50) = P+T - 50
    // = payload_size + tailroom - 50 = 128 + 16 - 50 = 94
    // Let's get dynamic tailroom: ( (buffer_start_ + total_data_area_size) - (data_ptr_ + data_len_) )
    size_t total_data_area_size = headroom + payload_size + tailroom;
    ASSERT_EQ( (buffer->buffer_start_ + total_data_area_size) - (buffer->data_ptr_ + buffer->data_len()), payload_size + tailroom - 50);


    // Set data length beyond capacity (relative to current data_ptr_)
    buffer->set_data_len(payload_size + 1); // Try to set length to 129, payload is 128
                                            // Max len from current data_ptr is payload_size
    ASSERT_EQ(buffer->data_len(), payload_size); 

    // Reset data_ptr and length for next tests
    buffer->reset_data_ptr(); // data_ptr_ should be at initial_payload_start
    buffer->set_data_len(0);  // data_len_ is 0
    ASSERT_EQ(buffer->data_ptr_, buffer->buffer_start_ + headroom);
    ASSERT_EQ(buffer->data_len(), 0);


    // Reserve headroom
    size_t reserve_h_len = 10;
    unsigned char* new_data_start = buffer->reserve_headroom(reserve_h_len);
    ASSERT_NE(new_data_start, nullptr);
    ASSERT_EQ(buffer->data_ptr_, (buffer->buffer_start_ + headroom) - reserve_h_len);
    ASSERT_EQ(buffer->data_len(), reserve_h_len); // reserve_headroom increases data_len

    // Reserve tailroom
    buffer->set_data_len(payload_size - 20); // Make some space at the end of payload
    buffer->reset_data_ptr(); // Reset data_ptr, data_len remains as is
    buffer->set_data_len(payload_size - 20); // data_len is now P-20, data_ptr is at start of payload
    
    size_t reserve_t_len = 10;
    unsigned char* tailroom_write_ptr = buffer->reserve_tailroom(reserve_t_len);
    ASSERT_NE(tailroom_write_ptr, nullptr);
    ASSERT_EQ(tailroom_write_ptr, buffer->buffer_start_ + headroom + (payload_size - 20) );
    ASSERT_EQ(buffer->data_len(), payload_size - 20 + reserve_t_len);

    buffer->release();
    delete[] raw_mem;
}

TEST(PacketBufferTest, ResetDataPtrFunctionality) {
    auto dummy_pool = std::make_shared<DummyPacketBufferPoolForTest>();
    size_t headroom = 32;
    size_t payload_size = 128;
    size_t tailroom = 0;
    size_t unit_size = sizeof(BufferMetadata) + sizeof(PacketBuffer) + headroom + payload_size + tailroom;
    unsigned char* raw_mem = new unsigned char[unit_size];

    PacketBuffer* buffer = create_simulated_pb(dummy_pool.get(), raw_mem, unit_size, payload_size, headroom, tailroom);
    buffer->add_ref(); 

    // Initial position
    unsigned char* initial_payload_start = buffer->buffer_start_ + headroom;
    ASSERT_EQ(buffer->data_ptr_, initial_payload_start);

    // Modify data_ptr
    buffer->reserve_headroom(10);
    ASSERT_NE(buffer->data_ptr_, initial_payload_start);
    
    // Store current data_len to check if reset_data_ptr modifies it
    size_t len_before_reset = buffer->data_len();

    buffer->reset_data_ptr();
    ASSERT_EQ(buffer->data_ptr_, initial_payload_start);
    ASSERT_EQ(buffer->data_len(), len_before_reset); // reset_data_ptr shouldn't change data_len

    buffer->release();
    delete[] raw_mem;
}
