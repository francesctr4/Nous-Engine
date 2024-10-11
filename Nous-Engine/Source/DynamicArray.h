#pragma once

#include "Globals.h"
#include "MemoryManager.h"

#define DARRAY_DEFAULT_CAPACITY 1
#define DARRAY_RESIZE_FACTOR 2
#define DARRAY_FIELD_LENGTH 3

template<typename T>
class DynamicArray 
{
public:

	DynamicArray(uint64 capacity = DARRAY_DEFAULT_CAPACITY);
	~DynamicArray();

	void Push(const T* valuePtr);
	T* Pop(T* destination);

	T* PopAt(uint64 index, T* destination);
	void InsertAt(uint64 index, T* valuePtr);

	void SetCapacity(uint64 newCapacity);
	void SetLength(uint64 newLength);
	void SetStride(uint64 newStride);

	uint64 GetCapacity() const;
	uint64 GetLength() const;
	uint64 GetStride() const;
	T* GetElements() const;

	void Clear();

private:

	void Create(uint64 capacity, uint64 stride);

	void Destroy();

	// ----------------------- \\

	uint64 capacity;
	uint64 length;
	uint64 stride;

	T* elements;
};

template<typename T>
inline DynamicArray<T>::DynamicArray(uint64 capacity)
{
	Create(capacity, sizeof(T));
}

template<typename T>
inline DynamicArray<T>::~DynamicArray()
{
	if (elements != nullptr) 
	{
		Destroy();
	}
}

template<typename T>
void DynamicArray<T>::Create(uint64 capacity, uint64 stride)
{
	this->capacity = capacity;
	this->length = 0;
	this->stride = stride;

	elements = NOUS_NEW_ARRAY<T>(capacity, MemoryManager::MemoryTag::DARRAY);
}

template<typename T>
void DynamicArray<T>::Destroy()
{
	NOUS_DELETE_ARRAY(elements, capacity, MemoryManager::MemoryTag::DARRAY);

	length = 0;
	capacity = 0;
	stride = 0;
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
void DynamicArray<T>::Clear()
{

}
