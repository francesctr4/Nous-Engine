#include "Globals.h"
#include "Application.h"
#include "Logger.h"
#include "Asserts.h"
#include "MemoryManager.h"

typedef enum MainState
{
	MAIN_CREATION,
	MAIN_START,
	MAIN_UPDATE,
	MAIN_FINISH,
	MAIN_EXIT

} MainState;

void DumpMemoryLeaks()
{
	_CrtDumpMemoryLeaks();
}

int main(int argc, char** argv) {

	MemoryManager::InitializeMemory();

	InitializeLogging();

	NOUS_INFO("Starting engine '%s'....", TITLE);

	int mainReturn = EXIT_FAILURE;
	MainState nousState = MAIN_CREATION;
	Application* App = nullptr;

	while (nousState != MAIN_EXIT) 
	{
		switch (nousState)
		{
		case MAIN_CREATION:

			NOUS_INFO("-------------- Application Creation --------------");
			App = NOUS_NEW<Application>(MemoryManager::MemoryTag::APPLICATION);

			nousState = MAIN_START;

			break;

		case MAIN_START:

			NOUS_INFO("-------------- Application Awake --------------");
			if (App->Awake() == false)
			{
				NOUS_INFO("[ERROR] Application Awake exits with ERROR");
				nousState = MAIN_EXIT;
			}
			else
			{
				nousState = MAIN_UPDATE;
				NOUS_INFO("-------------- Application Update --------------");
			}

			break;

		case MAIN_UPDATE:
		{
			int updateReturn = App->Update();

			if (updateReturn == UPDATE_ERROR)
			{
				NOUS_INFO("[ERROR] Application Update exits with ERROR");
				nousState = MAIN_EXIT;
			}
			else if (updateReturn == UPDATE_STOP) 
			{
				nousState = MAIN_FINISH;
			}

			break;
		}

		case MAIN_FINISH:

			NOUS_INFO("-------------- Application CleanUp --------------");
			if (App->CleanUp() == false)
			{
				NOUS_INFO("[ERROR] Application CleanUp exits with ERROR");
			}
			else 
			{
				mainReturn = EXIT_SUCCESS;
			}
				
			nousState = MAIN_EXIT;

			break;

		}
	}

	External = nullptr;

	NOUS_INFO("-------------- Application Destruction --------------");
	NOUS_DELETE(App, MemoryManager::MemoryTag::APPLICATION);

	NOUS_INFO("Exiting engine '%s'...\n", TITLE);

	NOUS_INFO(MemoryManager::GetMemoryUsageStats());

	ShutdownLogging();

	MemoryManager::ShutdownMemory();

	atexit(DumpMemoryLeaks);

	return mainReturn;
}