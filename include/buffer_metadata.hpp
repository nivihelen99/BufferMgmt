#ifndef BUFFER_METADATA_HPP
#define BUFFER_METADATA_HPP

#include <cstdint> // For uintXX_t types
#include <chrono>  // For timestamps

class PacketBuffer; // Forward declaration

class BufferMetadata {
public:
    BufferMetadata();

    // Common L2 fields
    uint16_t get_ingress_port() const;
    void set_ingress_port(uint16_t port);

    uint16_t get_vlan_id() const;
    void set_vlan_id(uint16_t vlan_id);

    // Timestamps (example)
    std::chrono::time_point<std::chrono::system_clock> get_rx_timestamp() const;
    void set_rx_timestamp(const std::chrono::time_point<std::chrono::system_clock>& ts);
    
    // Custom metadata (example placeholder)
    void* get_custom_metadata() const;
    void set_custom_metadata(void* custom_data);

    // Buffer lifecycle (example placeholder)
    enum class BufferState {
        Free,
        Allocated,
        InUse,
        Released
    };
    BufferState get_state() const;
    void set_state(BufferState state);

private:
    uint16_t ingress_port_ = 0;
    uint16_t vlan_id_ = 0;
    std::chrono::time_point<std::chrono::system_clock> rx_timestamp_;
    void* custom_metadata_ptr_ = nullptr;
    BufferState current_state_ = BufferState::Free;

    // Potentially a pointer back to the PacketBuffer if metadata is separate
    // PacketBuffer* owner_buffer_ = nullptr; 
};

#endif // BUFFER_METADATA_HPP
