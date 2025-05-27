#include "pool_manager.hpp"
#include "packet_buffer_pool.hpp" // For PacketBufferPool and its methods
#include <iostream> // For print_stats and error logging

PoolManager& PoolManager::instance() {
    static PoolManager inst; // Meyers singleton
    return inst;
}

PoolManager::PoolManager() {
    // Constructor can be empty or initialize NUMA detection if planned.
    // For now, it's empty. The map is default-constructed.
}

PoolManager::~PoolManager() {
    // std::unique_ptr will automatically delete the PacketBufferPool objects
    // when numa_pools_ is cleared or PoolManager is destroyed.
    // Explicitly clearing can be done for orderliness or if specific cleanup
    // order beyond unique_ptr's destruction is needed (not the case here).
    std::lock_guard<std::mutex> lock(manager_mutex_);
    numa_pools_.clear();
}

bool PoolManager::configure_pools_for_numa_node(int numa_node, const std::vector<PoolConfig>& configs) {
    std::lock_guard<std::mutex> lock(manager_mutex_);

    auto& pools_for_specific_numa = numa_pools_[numa_node]; // Creates entry if numa_node not present

    for (const auto& config : configs) {
        if (pools_for_specific_numa.count(config.buffer_size)) {
            std::cerr << "PoolManager: Pool for payload size " << config.buffer_size
                      << " on NUMA node " << numa_node << " already exists. Skipping configuration." << std::endl;
            continue; // Or update, based on policy
        }

        try {
            auto new_pool = std::make_unique<PacketBufferPool>(
                config.buffer_size,    // This is buffer_payload_size for PacketBufferPool constructor
                config.initial_count,
                numa_node,
                config.headroom,
                config.tailroom
            );
            pools_for_specific_numa[config.buffer_size] = std::move(new_pool);
            std::cout << "PoolManager: Configured pool for payload size " << config.buffer_size
                      << " (" << config.initial_count << " buffers) on NUMA node " << numa_node << std::endl;
        } catch (const std::bad_alloc& e) {
            std::cerr << "PoolManager: Failed to allocate memory for PacketBufferPool (size: " 
                      << config.buffer_size << ", node: " << numa_node << "). Exception: " << e.what() << std::endl;
            return false; // Stop configuration on first failure
        } catch (const std::exception& e) {
            std::cerr << "PoolManager: Failed to create PacketBufferPool (size: " 
                      << config.buffer_size << ", node: " << numa_node << "). Exception: " << e.what() << std::endl;
            return false; // Stop configuration on first failure
        }
    }
    return true;
}

bool PoolManager::add_pool(int numa_node, const PoolConfig& config) {
    return configure_pools_for_numa_node(numa_node, {config});
}

// Private helper, assumes manager_mutex_ is held by caller if necessary.
PacketBufferPool* PoolManager::find_pool(size_t desired_payload_size, int numa_node) const {
    // Try specified NUMA node first
    auto it_numa_map = numa_pools_.find(numa_node);
    if (it_numa_map != numa_pools_.end()) {
        const auto& size_to_pool_map = it_numa_map->second;
        // Find the smallest pool that is >= desired_payload_size
        auto it_pool = size_to_pool_map.lower_bound(desired_payload_size);
        if (it_pool != size_to_pool_map.end()) {
            return it_pool->second.get(); // Return raw pointer to the pool
        }
    }

    // If not found in specific NUMA node and a specific node was requested, try fallback to global pool (-1)
    if (numa_node != -1) { 
        it_numa_map = numa_pools_.find(-1); // Try global pool
        if (it_numa_map != numa_pools_.end()) {
            const auto& size_to_pool_map = it_numa_map->second;
            auto it_pool = size_to_pool_map.lower_bound(desired_payload_size);
            if (it_pool != size_to_pool_map.end()) {
                return it_pool->second.get();
            }
        }
    }
    
    return nullptr; // No suitable pool found
}

PacketBuffer* PoolManager::allocate(size_t desired_payload_size, int numa_node) {
    PacketBufferPool* pool = nullptr;
    { // Scope for lock guard
        std::lock_guard<std::mutex> lock(manager_mutex_);
        pool = find_pool(desired_payload_size, numa_node);
    } // Mutex unlocked here

    if (pool) {
        PacketBuffer* buffer = pool->allocate_buffer();
        if (!buffer) {
             std::cerr << "PoolManager: Pool found but failed to allocate buffer (size: " << desired_payload_size 
                       << ", node: " << numa_node << "). Pool might be empty." << std::endl;
        }
        return buffer;
    } else {
        std::cerr << "PoolManager: No suitable pool found for payload size " << desired_payload_size 
                  << " on NUMA node " << numa_node << "." << std::endl;
        // FR-001: Could attempt to create a pool dynamically here if allowed by policy.
    }
    return nullptr;
}

void PoolManager::deallocate(PacketBuffer* buffer) {
    // The primary deallocation path should be buffer->release(), which interacts
    // with its owning_pool_ directly. This PoolManager::deallocate is a fallback/convenience.
    if (!buffer) {
        return;
    }

    // If PacketBuffer knew its owning_pool (it does), we could use it.
    // PacketBufferPool* owner = buffer->owning_pool_; // Assuming PacketBuffer has get_owning_pool() or similar
    // if (owner) {
    //     owner->deallocate_buffer(buffer); // This is what buffer->release() does internally after ref_count hits 0
    // } else {
    //     std::cerr << "PoolManager: Buffer to deallocate has no owning pool. Leaking." << std::endl;
    // }
    // The safest is to rely on the buffer's own release mechanism.
    // If ref_count is not yet zero, this will just decrement. If it becomes zero, it will deallocate.
    std::cout << "PoolManager: Deallocating buffer via its release() method." << std::endl;
    buffer->release(); 
}

void PoolManager::print_stats() const {
    std::lock_guard<std::mutex> lock(manager_mutex_);
    std::cout << "=============== PoolManager Statistics ===============\n";
    if (numa_pools_.empty()) {
        std::cout << "  No pools configured.\n";
    } else {
        for (const auto& numa_entry : numa_pools_) {
            std::cout << "  NUMA Node: " << numa_entry.first 
                      << (numa_entry.first == -1 ? " (Global/Unspecified)" : "") << "\n";
            if (numa_entry.second.empty()) {
                std::cout << "    No pools for this NUMA node.\n";
                continue;
            }
            for (const auto& size_entry : numa_entry.second) {
                const PacketBufferPool* pool = size_entry.second.get();
                if (pool) {
                    std::cout << "    --------------------------------------------\n";
                    std::cout << "    Pool (Payload Size: " << pool->get_buffer_payload_size() 
                              << " B, Initial Count: " << pool->get_initial_pool_count() << ")\n";
                    std::cout << "      Configured Headroom: " << pool->get_headroom_size() << " B\n";
                    std::cout << "      Configured Tailroom: " << pool->get_tailroom_size() << " B\n";
                    std::cout << "      Free Buffers:        " << pool->get_free_count() << "\n";
                    std::cout << "      Alloc Count:         " << pool->get_alloc_count() << "\n";
                    std::cout << "      Dealloc Count:       " << pool->get_dealloc_count() << "\n";
                    // std::cout << "      High Water Mark: " << pool->get_high_water_mark() << "\n"; // If implemented
                } else {
                    std::cout << "    Pool (Payload Size: " << size_entry.first << ") - ERROR: Pool pointer is null!\n";
                }
            }
        }
    }
    std::cout << "======================================================" << std::endl;
}
