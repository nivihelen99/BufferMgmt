#include "gtest/gtest.h"
#include "buffer_metadata.hpp" // Adjust path as necessary

// Test fixture for BufferMetadata tests
class BufferMetadataTest : public ::testing::Test {
protected:
    BufferMetadata meta; // Each test gets a fresh BufferMetadata instance

    // Per-test set-up and tear-down logic can go here if needed.
    void SetUp() override {
        // meta is default constructed
    }

    void TearDown() override {
        // Clean-up.
    }
};

TEST_F(BufferMetadataTest, InitialState) {
    EXPECT_EQ(meta.get_ingress_port(), 0);
    EXPECT_EQ(meta.get_vlan_id(), 0);
    // rx_timestamp_ is initialized to std::chrono::system_clock::now();
    // So, we can't check for a fixed value, but we can check it's not zero or some default.
    // For simplicity here, we'll just check state and custom ptr.
    EXPECT_NE(meta.get_rx_timestamp(), std::chrono::time_point<std::chrono::system_clock>()); // Not epoch
    EXPECT_EQ(meta.get_custom_metadata(), nullptr);
    EXPECT_EQ(meta.get_state(), BufferMetadata::BufferState::Free);
}

TEST_F(BufferMetadataTest, SetAndGetIngressPort) {
    uint16_t test_port = 12345;
    meta.set_ingress_port(test_port);
    EXPECT_EQ(meta.get_ingress_port(), test_port);
}

TEST_F(BufferMetadataTest, SetAndGetVlanId) {
    uint16_t test_vlan_id = 101;
    meta.set_vlan_id(test_vlan_id);
    EXPECT_EQ(meta.get_vlan_id(), test_vlan_id);
}

TEST_F(BufferMetadataTest, SetAndGetRxTimestamp) {
    auto now = std::chrono::system_clock::now();
    // Allow for slight delay in setting/getting if precision is extremely high
    auto slightly_later = now + std::chrono::microseconds(10); 

    meta.set_rx_timestamp(now);
    // Timestamps can be tricky to compare exactly due to precision.
    // Check if it's close.
    EXPECT_GE(meta.get_rx_timestamp(), now);
    EXPECT_LE(meta.get_rx_timestamp(), slightly_later); // Ensure it's not wildly different
}

TEST_F(BufferMetadataTest, SetAndGetCustomMetadata) {
    int custom_data_value = 42;
    void* custom_ptr = &custom_data_value;
    
    meta.set_custom_metadata(custom_ptr);
    EXPECT_EQ(meta.get_custom_metadata(), custom_ptr);
    
    // Verify the data through the pointer if needed (careful with types)
    if (meta.get_custom_metadata()) {
        EXPECT_EQ(*(static_cast<int*>(meta.get_custom_metadata())), custom_data_value);
    }
}

TEST_F(BufferMetadataTest, SetAndGetState) {
    meta.set_state(BufferMetadata::BufferState::Allocated);
    EXPECT_EQ(meta.get_state(), BufferMetadata::BufferState::Allocated);

    meta.set_state(BufferMetadata::BufferState::InUse);
    EXPECT_EQ(meta.get_state(), BufferMetadata::BufferState::InUse);

    meta.set_state(BufferMetadata::BufferState::Released);
    EXPECT_EQ(meta.get_state(), BufferMetadata::BufferState::Released);
    
    meta.set_state(BufferMetadata::BufferState::Free);
    EXPECT_EQ(meta.get_state(), BufferMetadata::BufferState::Free);
}
