#include "FileHandle.h"

//inline bool FileHandle::Open(const std::string& filePath, FileMode mode) {
//	Close(); // Ensure any previous file is closed.
//
//	path = filePath;
//	if (mode & std::ios::in) {
//		fileStream = std::make_unique<std::fstream>(path, mode);
//		isValid = inputStream->is_open();
//	}
//	else if (mode & std::ios::out) {
//		outputStream = std::make_unique<std::ofstream>(path, mode);
//		isValid = outputStream->is_open();
//	}
//	return isValid;
//}