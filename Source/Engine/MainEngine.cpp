#ifdef _WIN32
extern "C" __declspec(dllexport) const char* NousEngine_Version = "0.4.0";
#else
extern "C" __attribute__((visibility("default"))) const char* NousEngine_Version = "0.4.0";
#endif