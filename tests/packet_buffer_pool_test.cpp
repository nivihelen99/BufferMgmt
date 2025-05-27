#include "gtest/gtest.h"
#include "packet_buffer_pool.hpp"
#include "packet_buffer.hpp" // For PacketBuffer type
#include "buffer_metadata.hpp" // For BufferMetadata type (used by PacketBuffer)

// Test fixture for PacketBufferPool tests
class PacketBufferPoolTest : public ::testing::Test {
protected:
    // Per-test set-up and tear-down logic can go here if needed.
    void SetUp() override {
    }

    void TearDown() override {
        // Clean-up.
    }
};

TEST_F(PacketBufferPoolTest, BasicInitializationAndGetters) {
    size_t payload_size = 256;
    size_t initial_count = 5;
    int numa_node = 0;
    size_t headroom = 64;
    size_t tailroom = 16;

    try {
        PacketBufferPool pool(payload_size, initial_count, numa_node, headroom, tailroom);
        
        EXPECT_EQ(pool.get_buffer_payload_size(), payload_size);
        EXPECT_EQ(pool.get_initial_pool_count(), initial_count);
        EXPECT_EQ(pool.get_numa_node(), numa_node);
        EXPECT_EQ(pool.get_headroom_size(), headroom);
        EXPECT_EQ(pool.get_tailroom_size(), tailroom);
        
        // If initialize_pool worked as per the latest intended implementation (single large block),
        // free_count should match initial_count.
        EXPECT_EQ(pool.get_free_count(), initial_count);
        EXPECT_EQ(pool.get_alloc_count(), 0);
        EXPECT_EQ(pool.get_dealloc_count(), 0);

    } catch (const std::exception& e) {
        FAIL() << "PacketBufferPool constructor or basic getter methods threw an exception: " << e.what();
    } catch (...) {
        FAIL() << "PacketBufferPool constructor or basic getter methods threw an unknown exception.";
    }
}

TEST_F(PacketBufferPoolTest, AllocateAndDeallocateBasic) {
    size_t payload_size = 128;
    size_t initial_count = 3;
    PacketBufferPool pool(payload_size, initial_count);

    ASSERT_EQ(pool.get_free_count(), initial_count);

    PacketBuffer* buf1 = pool.allocate_buffer();
    ASSERT_NE(buf1, nullptr) << "First allocation failed.";
    if (!buf1) return; // Guard for static analyzers

    EXPECT_EQ(pool.get_free_count(), initial_count - 1);
    EXPECT_EQ(pool.get_alloc_count(), 1);
    EXPECT_EQ(buf1->ref_count(), 1); // Pool should set ref_count to 1
    EXPECT_EQ(buf1->owning_pool_, &pool);
    EXPECT_NE(buf1->metadata(), nullptr);
    if(buf1->metadata()) {
        EXPECT_EQ(buf1->metadata()->get_state(), BufferMetadata::BufferState::Allocated);
    }


    PacketBuffer* buf2 = pool.allocate_buffer();
    ASSERT_NE(buf2, nullptr) << "Second allocation failed.";
    if (!buf2) return;

    EXPECT_EQ(pool.get_free_count(), initial_count - 2);
    EXPECT_EQ(pool.get_alloc_count(), 2);
    EXPECT_EQ(buf2->ref_count(), 1);

    // Deallocate buf1 (simulating PacketBuffer::release() calling pool's deallocate_buffer)
    // To do this properly, buffer's ref_count must be 0 when pool.deallocate_buffer is called.
    // The pool.deallocate_buffer is the one called by PacketBuffer::release AFTER ref_count hits 0.
    // So we simulate that state.
    // In a real scenario, user calls buf1->release(). If ref_count becomes 0,
    // PacketBuffer::release calls owning_pool_->deallocate_buffer(this).
    
    // Simulate PacketBuffer::release() behavior:
    buf1->ref_count_.store(0); // Manually set ref_count to 0 as if release path was fully taken
                               // This is because we are directly calling the pool's deallocate here.
    pool.deallocate_buffer(buf1);
    EXPECT_EQ(pool.get_free_count(), initial_count - 1);
    EXPECT_EQ(pool.get_dealloc_count(), 1);
    if(buf1->metadata()) { // Check if metadata was reset by deallocate_buffer
        EXPECT_EQ(buf1->metadata()->get_state(), BufferMetadata::BufferState::Free);
    }


    // Allocate again, should get buf1 (or a buffer)
    PacketBuffer* buf3 = pool.allocate_buffer();
    ASSERT_NE(buf3, nullptr) << "Third allocation failed.";
    EXPECT_EQ(pool.get_free_count(), initial_count - 2);
    EXPECT_EQ(pool.get_alloc_count(), 3); // Total allocations increased

    // Clean up remaining buffers
    buf2->ref_count_.store(0);
    pool.deallocate_buffer(buf2);
    buf3->ref_count_.store(0);
    pool.deallocate_buffer(buf3);

    EXPECT_EQ(pool.get_free_count(), initial_count);
    EXPECT_EQ(pool.get_dealloc_count(), 3);
}

TEST_F(PacketBufferPoolTest, AllocateAllBuffers) {
    size_t initial_count = 5;
    PacketBufferPool pool(128, initial_count);
    std::vector<PacketBuffer*> allocated_buffers;

    for (size_t i = 0; i < initial_count; ++i) {
        PacketBuffer* buf = pool.allocate_buffer();
        ASSERT_NE(buf, nullptr) << "Allocation " << i + 1 << " failed.";
        if (buf) {
            allocated_buffers.push_back(buf);
        }
    }
    EXPECT_EQ(pool.get_free_count(), 0);
    EXPECT_EQ(pool.get_alloc_count(), initial_count);

    // Try to allocate one more, should fail
    PacketBuffer* extra_buf = pool.allocate_buffer();
    EXPECT_EQ(extra_buf, nullptr) << "Allocation beyond capacity should fail.";

    // Deallocate all
    for (PacketBuffer* buf : allocated_buffers) {
        if (buf) {
            buf->ref_count_.store(0); // Simulate release path
            pool.deallocate_buffer(buf);
        }
    }
    EXPECT_EQ(pool.get_free_count(), initial_count);
    EXPECT_EQ(pool.get_dealloc_count(), initial_count);
}
