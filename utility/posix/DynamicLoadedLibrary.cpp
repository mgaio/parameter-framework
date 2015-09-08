
#include <DynamicLoadedLibrary.hpp>

#include <dlfcn.h>

#include <stdexcept>



const std::string DynamicLoadedLibrary::_osLibraryPrefix = "lib";
const std::string DynamicLoadedLibrary::_osLibrarySuffix = ".so";

DynamicLoadedLibrary::DynamicLoadedLibrary(const std::string& path)
{
    std::string sanitizedPath = osSanitizePathName(path);

    _handle = dlopen(sanitizedPath.c_str(), RTLD_LAZY);

    if (!_handle) {

        throw std::runtime_error(dlerror());
    }
}

DynamicLoadedLibrary::~DynamicLoadedLibrary(void)
{
    dlclose(_handle);
}

void *DynamicLoadedLibrary::getSymbol(const std::string& symbol) const
{
    return dlsym(_handle, symbol.c_str());
}
