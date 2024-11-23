#include "VulkanPlatform.h"
#include "Logger.h"

void ExecuteBatchFile(const char* batchFilePath)
{
    int result = std::system(batchFilePath);

    if (result != 0)
    {
        NOUS_ERROR("Failed to execute batch file: '%s'", batchFilePath);
    }
}