#include "gtest/gtest.h"
#include "pool_manager.hpp"
#include "packet_buffer.hpp"    // For PacketBuffer
#include "buffer_metadata.hpp"  // For BufferMetadata (indirectly via PacketBuffer)
#include <vector>

// To properly test PoolManager's effect on pool stats, we might need to inspect pools.
// However, PoolManager doesn't expose its pools directly.
// We can test by checking if allocated buffers match expected pool configurations.

TEST(PoolManagerTest, SingletonInstance) {
    PoolManager& pm1 = PoolManager::instance();
    PoolManager& pm2 = PoolManager::instance();
    ASSERT_EQ(&pm1, &pm2) << "PoolManager::instance() should return the same instance.";
}

TEST(PoolManagerTest, PoolConfigurationAndAllocation) {
    PoolManager& pm = PoolManager::instance();
    
    // It's good practice to clear any existing configurations if tests run sequentially in one process
    // or ensure PoolManager is reset. For now, assume a fresh state or that namespacing/uniqueness
    // of pool configurations (e.g. by NUMA node and size) handles this.
    // A dedicated reset method in PoolManager would be better for testing.

    std::vector<PoolConfig> configs_node0 = {
        {128, 10, 32, 0}, // payload_size, initial_count, headroom, tailroom
        {512, 5, 64, 0}
    };
    ASSERT_TRUE(pm.configure_pools_for_numa_node(0, configs_node0)) << "Configuration for NUMA node 0 failed.";

    std::vector<PoolConfig> configs_node_global = {
        {1024, 8, 128, 0} 
    };
    ASSERT_TRUE(pm.configure_pools_for_numa_node(-1, configs_node_global)) << "Configuration for global NUMA node (-1) failed.";

    // Test allocation from NUMA node 0
    PacketBuffer* buf1 = pm.allocate(100, 0); // Should get from 128B pool on node 0
    ASSERT_NE(buf1, nullptr) << "Allocation for 100B on node 0 failed.";
    if (buf1) {
        EXPECT_EQ(buf1->get_numa_node(), 0);
        EXPECT_GE(buf1->capacity(), 128); // Payload capacity
        EXPECT_EQ(buf1->ref_count(), 1);
        // Check headroom if PacketBuffer stores and exposes its configured headroom
        // EXPECT_EQ(buf1->headroom_size(), 32); // This depends on PacketBuffer's API
        buf1->release();
    }

    PacketBuffer* buf2 = pm.allocate(500, 0); // Should get from 512B pool on node 0
    ASSERT_NE(buf2, nullptr) << "Allocation for 500B on node 0 failed.";
    if (buf2) {
        EXPECT_EQ(buf2->get_numa_node(), 0);
        EXPECT_GE(buf2->capacity(), 512);
        EXPECT_EQ(buf2->ref_count(), 1);
        // EXPECT_EQ(buf2->headroom_size(), 64);
        buf2->release();
    }

    // Test allocation from global pool (-1 implicitly or explicitly)
    PacketBuffer* buf3 = pm.allocate(1000); // Implicit NUMA -1
    ASSERT_NE(buf3, nullptr) << "Allocation for 1000B on global node failed.";
    if (buf3) {
        // NUMA node could be -1 or a system default if not strictly controlled by global pool
        // For this test, assuming global pool sets it to -1 or its designated NUMA.
        EXPECT_EQ(buf3->get_numa_node(), -1); // If global pool is strictly -1
        EXPECT_GE(buf3->capacity(), 1024);
        EXPECT_EQ(buf3->ref_count(), 1);
        // EXPECT_EQ(buf3->headroom_size(), 128);
        buf3->release();
    }
    
    PacketBuffer* buf4 = pm.allocate(1000, -1); // Explicit NUMA -1
    ASSERT_NE(buf4, nullptr) << "Allocation for 1000B on explicit node -1 failed.";
    if (buf4) {
        EXPECT_EQ(buf4->get_numa_node(), -1);
        EXPECT_GE(buf4->capacity(), 1024);
        buf4->release();
    }


    // Test allocation for a size where no direct pool exists, but a larger one might be used
    PacketBuffer* buf5 = pm.allocate(600, -1); // Should use 1024B pool from global
    ASSERT_NE(buf5, nullptr) << "Allocation for 600B (should use 1024B pool) failed.";
    if (buf5) {
        EXPECT_EQ(buf5->get_numa_node(), -1);
        EXPECT_GE(buf5->capacity(), 1024);
        buf5->release();
    }

    // Test allocation for a size that doesn't fit any pool
    PacketBuffer* buf6 = pm.allocate(2048, 0); // No pool for this size on node 0
    ASSERT_EQ(buf6, nullptr) << "Allocation for 2048B on node 0 should have failed.";
    
    PacketBuffer* buf7 = pm.allocate(2048, -1); // No pool for this size on global node
    ASSERT_EQ(buf7, nullptr) << "Allocation for 2048B on global node should have failed.";


    // Test deallocate (delegates to buffer's release)
    PacketBuffer* buf_to_dealloc = pm.allocate(100, 0);
    ASSERT_NE(buf_to_dealloc, nullptr);
    if (buf_to_dealloc) {
        ASSERT_EQ(buf_to_dealloc->ref_count(), 1);
        pm.deallocate(buf_to_dealloc); // This calls buf_to_dealloc->release()
        // After PoolManager::deallocate (which calls release), if this was the last ref,
        // the buffer would be returned to its pool.
        // We can't easily check the pool's free count without more invasive testing setup
        // or friend classes/methods for the pool.
        // The main check is that it doesn't crash and that buffer->release() was invoked.
    }
    
    // pm.print_stats(); // Useful for manual inspection during testing
}

TEST(PoolManagerTest, AddPoolFunction) {
    PoolManager& pm = PoolManager::instance();
    // Using a different NUMA node to avoid conflicts if tests run in sequence without reset
    int test_numa_node = 1; 
    PoolConfig config = {256, 20, 16, 0}; // payload_size, initial_count, headroom, tailroom
    
    ASSERT_TRUE(pm.add_pool(test_numa_node, config)) << "add_pool failed.";

    PacketBuffer* buf = pm.allocate(200, test_numa_node);
    ASSERT_NE(buf, nullptr) << "Allocation from newly added pool failed.";
    if (buf) {
        EXPECT_EQ(buf->get_numa_node(), test_numa_node);
        EXPECT_GE(buf->capacity(), 256);
        // EXPECT_EQ(buf->headroom_size(), 16);
        buf->release();
    }
}

// Test to ensure that requesting a buffer for a specific NUMA node
// prefers that node over a global (-1) pool of the same suitable size.
TEST(PoolManagerTest, NumaAffinityPreferredOverGlobal) {
    PoolManager& pm = PoolManager::instance();
    int specific_numa_node = 2;

    PoolConfig numa_config = {128, 5, 32, 0};
    PoolConfig global_config = {128, 5, 32, 0}; // Same size, different node association

    ASSERT_TRUE(pm.add_pool(specific_numa_node, numa_config)) << "Failed to add pool for NUMA node " << specific_numa_node;
    ASSERT_TRUE(pm.add_pool(-1, global_config)) << "Failed to add global pool.";

    PacketBuffer* buf = pm.allocate(100, specific_numa_node);
    ASSERT_NE(buf, nullptr) << "Allocation failed for specific NUMA node " << specific_numa_node;
    if (buf) {
        EXPECT_EQ(buf->get_numa_node(), specific_numa_node) 
            << "Buffer allocated from incorrect NUMA node. Expected " << specific_numa_node 
            << ", got " << buf->get_numa_node();
        buf->release();
    }
}

TEST(PoolManagerTest, FallbackToGlobalPool) {
    PoolManager& pm = PoolManager::instance();
    int non_configured_numa_node = 3; // Assume this node has no specific 128B pool
    int global_numa_node = -1;

    // Ensure a global pool of this size exists (it might from previous test, but be explicit)
    PoolConfig global_config = {128, 5, 32, 0};
    pm.add_pool(global_numa_node, global_config); // add_pool is idempotent for same config

    PacketBuffer* buf = pm.allocate(100, non_configured_numa_node);
    ASSERT_NE(buf, nullptr) << "Allocation failed, expected fallback to global pool.";
    if (buf) {
        // If it came from the global pool, its NUMA node should be -1 (or as set by that global pool)
        EXPECT_EQ(buf->get_numa_node(), global_numa_node) 
            << "Buffer should have fallen back to global pool. Expected NUMA " << global_numa_node
            << ", got " << buf->get_numa_node();
        buf->release();
    }
}
