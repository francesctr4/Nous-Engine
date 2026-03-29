#ifndef NOUS_ENGINE_NOUS_VECTOR_H
#define NOUS_ENGINE_NOUS_VECTOR_H

#include "Engine/Core/MemoryManager/CustomAllocators/NOUS_STLAllocator/include/NOUS_STLAllocator.h"

#include <vector>

// std::vector wrapper that uses Nous Engine Memory Manager and Custom Allocator.
// Must specify the memory tag when constructing.

template<typename T>
class NOUS_Vector
{
public:
    using value_type = T;
    using allocator_type = NOUS_STLAllocator<T>;
    using vector_type = std::vector<T, allocator_type>;
    using iterator = vector_type::iterator;
    using const_iterator = vector_type::const_iterator;
    using reverse_iterator = vector_type::reverse_iterator;
    using const_reverse_iterator = vector_type::const_reverse_iterator;

    // Default constructor -> uses UNKNOWN tag
    NOUS_Vector()
            : internal_(allocator_type(MemoryTag::UNKNOWN))
    {
    }

    // Constructor with tag
    explicit NOUS_Vector(MemoryTag tag)
            : internal_(allocator_type(tag))
    {
    }

    // Size constructor
    explicit NOUS_Vector(size_t count, MemoryTag tag = MemoryTag::UNKNOWN)
            : internal_(count, T{}, allocator_type(tag))
    {
    }

    // Copy / Move constructors
    NOUS_Vector(const NOUS_Vector&) = default;
    NOUS_Vector(NOUS_Vector&&) noexcept = default;

    NOUS_Vector& operator=(const NOUS_Vector&) = default;
    NOUS_Vector& operator=(NOUS_Vector&&) noexcept = default;

    // Vector-like API forwarding
    void push_back(const T& value) { internal_.push_back(value); }
    void push_back(T&& value) { internal_.push_back(std::move(value)); }

    template<typename... Args>
    T& emplace_back(Args&&... args) { return internal_.emplace_back(std::forward<Args>(args)...); }

    void pop_back() { internal_.pop_back(); }

    void clear() { internal_.clear(); }

    size_t size() const { return internal_.size(); }
    bool empty() const { return internal_.empty(); }

    T& operator[](size_t i) { return internal_[i]; }
    const T& operator[](size_t i) const { return internal_[i]; }

    iterator begin() { return internal_.begin(); }
    iterator end() { return internal_.end(); }

    const_iterator begin() const { return internal_.begin(); }
    const_iterator end() const { return internal_.end(); }

    vector_type& raw() { return internal_; }
    const vector_type& raw() const { return internal_; }

    iterator erase(const_iterator pos) {
        return internal_.erase(pos);
    }

    iterator erase(const_iterator first, const_iterator last) {
        return internal_.erase(first, last);
    }

    reverse_iterator rbegin() { return internal_.rbegin(); }
    reverse_iterator rend() { return internal_.rend(); }

    const_reverse_iterator rbegin() const { return internal_.rbegin(); }
    const_reverse_iterator rend() const { return internal_.rend(); }

    void EraseAt(size_t index) {
        if (index < internal_.size())
            internal_.erase(internal_.begin() + index);
    }

private:
    vector_type internal_;
};

#endif //NOUS_ENGINE_NOUS_VECTOR_H
