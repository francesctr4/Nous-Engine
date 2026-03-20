#ifndef NOUS_ENGINE_NOUS_STL_ALLOCATOR_H
#define NOUS_ENGINE_NOUS_STL_ALLOCATOR_H

#include <cstddef>
#include <limits>
#include <type_traits>
#include <new>

#include "Engine/Core/MemoryManager/MemoryManager.h"

// A standard-conforming STL allocator that routes to your MemoryManager.
//
//  - Uses MemoryManager::Allocate(size, tag) which internally records the size
//    in DynamicAllocator's map.
//  - Uses the new size-less MemoryManager::Free(void* block, MemoryTag tag),
//    which looks up the original size via DynamicAllocator::GetRecordedSize
//    and keeps stats correct.
//
//  - We store a MemoryTag inside the allocator so stats are correct per container.
//    The allocator is trivially copyable and default-constructible (UNKNOWN tag).
//
//  - Works with std::allocator_traits (construct/destroy are not required).

template <class T>
class NOUS_STLAllocator {
public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using void_pointer = void*;
    using const_void_pointer = const void*;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    // Propagation traits (safe defaults for most containers)
    using propagate_on_container_copy_assignment = std::false_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap            = std::true_type;
    using is_always_equal                        = std::false_type; // depends on tag

    template<class U> struct rebind { using other = NOUS_STLAllocator<U>; };

    // Constructors
    constexpr NOUS_STLAllocator() noexcept
            : tag_(MemoryTag::UNKNOWN) {}

    constexpr explicit NOUS_STLAllocator(MemoryTag tag) noexcept
            : tag_(tag) {}

    template<class U>
    constexpr NOUS_STLAllocator(const NOUS_STLAllocator<U>& other) noexcept
            : tag_(other.tag()) {}

    // Allocate n elements of T (byte size = n * sizeof(T))
    [[nodiscard]] pointer allocate(size_type n) {
        if (n > max_size()) {
            throw std::bad_array_new_length();
        }

        const std::size_t bytes = n * sizeof(T);
        void* p = MemoryManager::Allocate(static_cast<uint64>(bytes), tag_);
        if (!p) {
            throw std::bad_alloc();
        }
        return static_cast<pointer>(p);
    }

    // Deallocate n elements of T.
    //
    // We ignore n and route to the new size-less free path:
    //   MemoryManager::Free(void* block, MemoryTag tag)
    //
    // That function uses DynamicAllocator::GetRecordedSize(block) to:
    //   - look up the original requested size
    //   - update stats correctly
    //   - free via DynamicAllocator::Free(void*)
    void deallocate(pointer p, size_type /*n*/) noexcept {
        // MemoryManager::Free already early-outs if p == nullptr.
        MemoryManager::Free(static_cast<void*>(p), tag_);
    }

    // Max elements that could theoretically fit in the pool.
    // Queries the live pool capacity so containers don't attempt reserves beyond it.
    // Falls back to the theoretical maximum if the pool isn't initialised yet.
    size_type max_size() const noexcept {
        const uint64 poolSize = MemoryManager::GetMemoryConfig().totalAllocationSize;
        if (poolSize == 0)
            return std::numeric_limits<size_type>::max() / sizeof(T);
        return static_cast<size_type>(poolSize / sizeof(T));
    }

    // Access / change tag
    constexpr MemoryTag tag() const noexcept { return tag_; }

    // Allocators compare equal if they use the same tag
    friend bool operator==(const NOUS_STLAllocator& a, const NOUS_STLAllocator& b) noexcept {
        return a.tag_ == b.tag_;
    }
    friend bool operator!=(const NOUS_STLAllocator& a, const NOUS_STLAllocator& b) noexcept {
        return !(a == b);
    }

private:
    template<class U> friend class NOUS_STLAllocator;
    MemoryTag tag_;
};

#endif // NOUS_ENGINE_NOUS_STL_ALLOCATOR_H
