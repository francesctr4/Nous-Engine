#pragma once

#include "Globals.h"
#include "MemoryManager.h"
#include "Asserts.h"
#include <string>

#define DARRAY_DEFAULT_CAPACITY 1
#define DARRAY_RESIZE_FACTOR 2
#define DARRAY_FIELD_LENGTH 3

template<typename T>
class DynamicArray 
{
public:

	DynamicArray(uint64 capacity = DARRAY_DEFAULT_CAPACITY);

	// Constructor for initializer list
	DynamicArray(std::initializer_list<T> init, uint64 capacity = DARRAY_DEFAULT_CAPACITY);

	// Constructor from other Dynamic Array
	DynamicArray(const DynamicArray<T>& other);

	~DynamicArray();

	void Create(uint64 capacity);

	void Push(const T* valuePtr);
	void Push(const T& value);
	void InsertAt(uint64 index, T* valuePtr);

	T* Pop();
	T* PopAt(uint64 index);

	void Clear();

	bool IsEmpty();
	bool Contains(const T& value) const;
	bool Remove(const T& value);

	void CopyFrom(const DynamicArray<T>& other);

	void SetCapacity(uint64 newCapacity);
	void SetLength(uint64 newLength);
	void SetStride(uint64 newStride);

	uint64 GetCapacity() const;
	uint64 GetLength() const;
	uint64 GetStride() const;
	T* GetElements() const;

	// Overload operator[] (read only)
	const T& operator[](size_t index) const;

private:

	void Create(uint64 capacity, uint64 stride);
	void Resize();
	void Destroy();

	// ----------------------- \\

	uint64 capacity;
	uint64 length;
	uint64 stride;

	T* elements;
};

template<typename T>
inline DynamicArray<T>::DynamicArray(uint64 capacity)
	: capacity(capacity), length(0), stride(sizeof(T)), elements(nullptr)
{
	Create(capacity, stride);
}

// Constructor from other Dynamic Array
template<typename T>
inline DynamicArray<T>::DynamicArray(const DynamicArray<T>& other)
	: capacity(other.capacity), length(other.length), stride(other.stride), elements(nullptr)
{
	Create(capacity, stride);

	for (int i = 0; i < other.length; ++i)
	{
		Push(other[i]);
	}
}

// Constructor for initializer list
template<typename T>
inline DynamicArray<T>::DynamicArray(std::initializer_list<T> init, uint64 capacity)
	: capacity(capacity), length(0), stride(sizeof(T)), elements(nullptr)
{
	Create(capacity, stride);

	for (const T& item : init) 
	{
		Push(item);
	}
}

template<typename T>
inline DynamicArray<T>::~DynamicArray()
{
	Destroy();
}

template<typename T>
inline void DynamicArray<T>::Push(const T* valuePtr)
{
	// Resize if necessary
	if (length >= capacity) {
		Resize();
	}

	// Copy the value to the next available element
	MemoryManager::CopyMemory(&elements[length], valuePtr, sizeof(T));

	// Increase the length
	length++;
}

template<typename T>
inline void DynamicArray<T>::Push(const T& value)
{
	// Resize if necessary
	if (length >= capacity) {
		Resize();
	}

	// Copy the value to the next available element
	MemoryManager::CopyMemory(&elements[length], &value, sizeof(T));

	// Increase the length
	length++;
}

template<typename T>
inline void DynamicArray<T>::Resize()
{
	// Calculate the new capacity
	uint64 newCapacity = capacity * DARRAY_RESIZE_FACTOR;

	// Allocate a new array with increased capacity
	T* newElements = NOUS_NEW_ARRAY<T>(newCapacity, MemoryManager::MemoryTag::DARRAY);

	// Copy existing elements to the new array
	MemoryManager::CopyMemory(newElements, elements, length * stride);
	
	// Free the old array
	NOUS_DELETE_ARRAY(elements, capacity, MemoryManager::MemoryTag::DARRAY);

	// Update the internal state
	elements = newElements;
	capacity = newCapacity;
}

template<typename T>
inline void DynamicArray<T>::Create(uint64 capacity)
{
	Create(capacity, sizeof(T));
}

template<typename T>
inline void DynamicArray<T>::Create(uint64 capacity, uint64 stride)
{
	this->capacity = capacity;
	this->length = 0;
	this->stride = stride;

	elements = NOUS_NEW_ARRAY<T>(capacity, MemoryManager::MemoryTag::DARRAY);
	MemoryManager::SetMemory(elements, 0, capacity * stride);
}

template<typename T>
inline void DynamicArray<T>::Destroy()
{
	if (elements) 
	{
		NOUS_DELETE_ARRAY(elements, capacity, MemoryManager::MemoryTag::DARRAY);
		elements = nullptr;
	}

	length = 0;
	capacity = 0;
	stride = 0;
}

template<typename T>
inline const T& DynamicArray<T>::operator[](size_t index) const
{
	return elements[index];
}

template<typename T>
inline void DynamicArray<T>::InsertAt(uint64 index, T* valuePtr)
{
	if (index > length) return;  // Index out of bounds

	if (length >= capacity) {
		Resize();
	}

	// Shift elements to the right
	for (uint64 i = length; i > index; --i) {
		MemoryManager::CopyMemory(&elements[i], &elements[i - 1], sizeof(T));
	}

	// Insert the new value
	MemoryManager::CopyMemory(&elements[index], valuePtr, sizeof(T));

	// Update length
	length++;
}

template<typename T>
inline T* DynamicArray<T>::Pop()
{
	if (length > 0) {
		length--;
		return &elements[length];  // Return a pointer to the popped element
	}
	return nullptr;  // Return nullptr if the array is empty
}

template<typename T>
inline T* DynamicArray<T>::PopAt(uint64 index)
{
	if (index >= length) return nullptr;  // Index out of bounds

	// Allocate memory for the value to be popped
	T* poppedValue = NOUS_NEW<T>(MemoryManager::MemoryTag::DARRAY);
	MemoryManager::CopyMemory(poppedValue, &elements[index], sizeof(T));  // Copy the value before shifting

	// Shift elements to the left
	for (uint64 i = index; i < length - 1; ++i) {
		MemoryManager::CopyMemory(&elements[i], &elements[i + 1], sizeof(T));
	}

	length--;  // Update length
	return poppedValue;  // Return a pointer to the copied (popped) value
}

template<typename T>
inline void DynamicArray<T>::SetCapacity(uint64 newCapacity)
{
	// If the new capacity is less than or equal to current capacity, do nothing
	if (newCapacity <= capacity) return;

	// Allocate a new array with the requested capacity
	T* newElements = NOUS_NEW_ARRAY<T>(newCapacity, MemoryManager::MemoryTag::DARRAY);

	// Copy existing elements to the new array
	if (elements) {
		MemoryManager::CopyMemory(newElements, elements, length * stride);
		// Free the old array
		NOUS_DELETE_ARRAY(elements, capacity, MemoryManager::MemoryTag::DARRAY);
	}

	// Update the internal state
	elements = newElements;
	capacity = newCapacity;
}

template<typename T>
inline void DynamicArray<T>::SetLength(uint64 newLength)
{
	// Ensure the new length does not exceed the current capacity
	if (newLength > capacity) {
		// Optionally resize if necessary
		SetCapacity(newLength);
	}

	// Update the length
	length = newLength;
}

template<typename T>
inline void DynamicArray<T>::SetStride(uint64 newStride)
{
	this->stride = newStride;
}

template<typename T>
inline uint64 DynamicArray<T>::GetCapacity() const
{
	return capacity;
}

template<typename T>
inline uint64 DynamicArray<T>::GetLength() const
{
	return length;
}

template<typename T>
inline uint64 DynamicArray<T>::GetStride() const
{
	return stride;
}

template<typename T>
inline T* DynamicArray<T>::GetElements() const
{
	return elements;
}

template<typename T>
inline void DynamicArray<T>::Clear()
{
	length = 0;
}

template<typename T>
inline bool DynamicArray<T>::IsEmpty()
{
	return length == 0;
}

template<typename T>
inline bool DynamicArray<T>::Contains(const T& value) const
{
	for (uint64 i = 0; i < length; ++i) 
	{
		if (elements[i] == value) 
		{
			return true;
		}
	}
	return false;
}

template<typename T>
inline bool DynamicArray<T>::Remove(const T& value)
{
	for (uint64 i = 0; i < length; ++i) 
	{
		if (elements[i] == value) 
		{  
			NOUS_DELETE(PopAt(i), MemoryManager::MemoryTag::DARRAY);
			return true;  
		}
	}
	return false;
}

template<typename T>
void DynamicArray<T>::CopyFrom(const DynamicArray<T>& other) 
{
	Clear();  // Clear the current array before copying

	// Ensure the current array has enough capacity
	if (capacity < other.length) {
		SetCapacity(other.length);  // Resize if necessary
	}

	// Copy each element from the other array
	for (uint64 i = 0; i < other.length; ++i) {
		Push(other[i]);  // Use Push to insert elements into this array
	}
}