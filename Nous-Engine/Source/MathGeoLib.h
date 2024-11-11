#pragma once

#ifdef _DEBUG
#pragma comment (lib, "Source/External/MathGeoLib/libx64/lib_Debug/MathGeoLib.lib")
#else
#pragma comment (lib, "Source/External/MathGeoLib/libx64/lib_Release/MathGeoLib.lib")
#endif // _DEBUG

#include "External/MathGeoLib/include/MathGeoLib.h"