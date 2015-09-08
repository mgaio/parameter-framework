
#include <DynamicLoadedLibrary.hpp>

std::string DynamicLoadedLibrary::osSanitizePathName(const std::string& path)
{
    if (path.rfind(".") != std::string::npos) {

        std::string sanitizedPath = _osLibraryPrefix + path + _osLibraryPrefix;

        return sanitizedPath;
    }

    return path;
}
