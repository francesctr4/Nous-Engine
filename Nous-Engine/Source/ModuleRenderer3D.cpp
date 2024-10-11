#include "ModuleRenderer3D.h"
#include "Logger.h"

#include "Tracy.h"

ModuleRenderer3D::ModuleRenderer3D(Application* app, std::string name, bool start_enabled) : Module(app, name, start_enabled)
{
	NOUS_TRACE("%s()", __FUNCTION__);
}

ModuleRenderer3D::~ModuleRenderer3D()
{
	NOUS_TRACE("%s()", __FUNCTION__);
}

bool ModuleRenderer3D::Awake()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	return true;
}

// Test pushing elements to the dynamic array
void TestPush()
{
	DynamicArray<int> arr;
	int val1 = 10;
	int val2 = 20;

	arr.Push(&val1);
	arr.Push(&val2);

	NOUS_ASSERT(arr.GetLength() == 2);    // Check length
	NOUS_ASSERT(arr.GetCapacity() >= 2);  // Capacity should be >= 2

	// Check if elements are correctly added
	NOUS_ASSERT(*arr.GetElements() == 10);
	NOUS_ASSERT(*(arr.GetElements() + 1) == 20);
}

// Test popping the last element
void TestPop()
{
	DynamicArray<int> arr;
	int val1 = 10;
	int val2 = 20;

	arr.Push(&val1);
	arr.Push(&val2);

	int* poppedVal = arr.Pop();
	NOUS_ASSERT(poppedVal != nullptr);
	NOUS_ASSERT(*poppedVal == 20);  // Last pushed value should be popped
	NOUS_ASSERT(arr.GetLength() == 1);  // Length should decrease

	poppedVal = arr.Pop();
	NOUS_ASSERT(poppedVal != nullptr);
	NOUS_ASSERT(*poppedVal == 10);
	NOUS_ASSERT(arr.GetLength() == 0);  // Array should now be empty

	NOUS_ASSERT(arr.Pop() == nullptr);  // Pop should return nullptr when empty
}

// Test popping an element at a specific index
void TestPopAt()
{
	DynamicArray<int> arr;
	int val1 = 10;
	int val2 = 20;
	int val3 = 30;

	arr.Push(&val1);
	arr.Push(&val2);
	arr.Push(&val3);

	int* poppedVal = arr.PopAt(1);  // Pop the element at index 1 (20)
	NOUS_ASSERT(poppedVal != nullptr);
	NOUS_ASSERT(*poppedVal == 20);
	NOUS_ASSERT(arr.GetLength() == 2);  // Length should decrease

	// Check if the remaining elements are correct
	NOUS_ASSERT(*arr.GetElements() == 10);
	NOUS_ASSERT(*(arr.GetElements() + 1) == 30);
	NOUS_DELETE(poppedVal, MemoryManager::MemoryTag::DARRAY);
}

// Test inserting an element at a specific index
void TestInsertAt()
{
	DynamicArray<int> arr;
	int val1 = 10;
	int val2 = 20;
	int val3 = 30;

	arr.Push(&val1);
	arr.Push(&val2);

	// Insert a new element at index 1
	arr.InsertAt(1, &val3);

	NOUS_ASSERT(arr.GetLength() == 3);
	NOUS_ASSERT(arr.GetCapacity() >= 3);  // Check that capacity is updated if necessary

	// Check if the elements are correct
	NOUS_ASSERT(*arr.GetElements() == 10);
	NOUS_ASSERT(*(arr.GetElements() + 1) == 30);  // Inserted element
	NOUS_ASSERT(*(arr.GetElements() + 2) == 20);
}

// Test array resizing
void TestResize()
{
	DynamicArray<int> arr(2);  // Start with capacity 2
	int val1 = 10;
	int val2 = 20;
	int val3 = 30;

	arr.Push(&val1);
	arr.Push(&val2);

	// Should trigger a resize
	arr.Push(&val3);

	NOUS_ASSERT(arr.GetLength() == 3);
	NOUS_ASSERT(arr.GetCapacity() >= 3);  // Capacity should have increased

	// Check that elements are still correct after resizing
	NOUS_ASSERT(*arr.GetElements() == 10);
	NOUS_ASSERT(*(arr.GetElements() + 1) == 20);
	NOUS_ASSERT(*(arr.GetElements() + 2) == 30);
}

// Test clearing the array
void TestClear()
{
	DynamicArray<int> arr;
	int val1 = 10;
	int val2 = 20;

	arr.Push(&val1);
	arr.Push(&val2);

	NOUS_ASSERT(arr.GetLength() == 2);  // Initially 2 elements

	arr.Clear();  // Clear the array

	NOUS_ASSERT(arr.GetLength() == 0);  // Length should be 0
	NOUS_ASSERT(arr.GetCapacity() >= 2);  // Capacity should not change
}

bool ModuleRenderer3D::Start()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	TestPush();
	TestPop();
	TestPopAt();
	TestInsertAt();
	TestResize();
	TestClear();

	return true;
}

UpdateStatus ModuleRenderer3D::PreUpdate(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	return UPDATE_CONTINUE;
}

UpdateStatus ModuleRenderer3D::Update(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	return UPDATE_CONTINUE;
}

UpdateStatus ModuleRenderer3D::PostUpdate(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	return UPDATE_CONTINUE;
}

bool ModuleRenderer3D::CleanUp()
{
	NOUS_TRACE("%s()", __FUNCTION__);
	return true;
}

void ModuleRenderer3D::ReceiveEvent(const Event& event)
{
	switch (event.type)
	{
	case EventType::TEST:
		NOUS_ERROR("ModuleRenderer3D LISTENED TEST");
		NOUS_ERROR("Received context: %d, %d", event.context.int64[0], event.context.int64[1]);
		break;
	default:
		break;
	}
}
