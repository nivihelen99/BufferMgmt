#include "buffer_metadata.hpp"

BufferMetadata::BufferMetadata() : ingress_port_(0), vlan_id_(0), custom_metadata_ptr_(nullptr), current_state_(BufferState::Free) {
    // Initialize timestamp to current time or a sensible default
    rx_timestamp_ = std::chrono::system_clock::now(); 
}

// --- Getter and Setter Implementations ---

uint16_t BufferMetadata::get_ingress_port() const {
    return ingress_port_;
}

void BufferMetadata::set_ingress_port(uint16_t port) {
    ingress_port_ = port;
}

uint16_t BufferMetadata::get_vlan_id() const {
    return vlan_id_;
}

void BufferMetadata::set_vlan_id(uint16_t vlan_id) {
    vlan_id_ = vlan_id;
}

std::chrono::time_point<std::chrono::system_clock> BufferMetadata::get_rx_timestamp() const {
    return rx_timestamp_;
}

void BufferMetadata::set_rx_timestamp(const std::chrono::time_point<std::chrono::system_clock>& ts) {
    rx_timestamp_ = ts;
}

void* BufferMetadata::get_custom_metadata() const {
    return custom_metadata_ptr_;
}

void BufferMetadata::set_custom_metadata(void* custom_data) {
    custom_metadata_ptr_ = custom_data;
}

BufferMetadata::BufferState BufferMetadata::get_state() const {
    return current_state_;
}

void BufferMetadata::set_state(BufferState state) {
    current_state_ = state;
}
