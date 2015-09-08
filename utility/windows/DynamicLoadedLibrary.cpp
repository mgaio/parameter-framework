
#include <DynamicLoadedLibrary.hpp>

#include "windows.h"

#include <stdexcept>



const std::string DynamicLoadedLibrary::_osLibraryPrefix = "";
const std::string DynamicLoadedLibrary::_osLibrarySuffix = ".dll";

DynamicLoadedLibrary::DynamicLoadedLibrary(const std::string& path)
{
    std::string sanitizedPath = osSanitizePathName(path);

    _handle = reinterpret_cast<void *>(LoadLibrary(sanitizedPath.c_str()));

    if (!_handle) {

        throw std::runtime_error(sanitizedPath + ": cannot open shared object file.");
    }
}

DynamicLoadedLibrary::~DynamicLoadedLibrary(void)
{
    HINSTANCE Module = reinterpret_cast<HINSTANCE>(_handle);

    FreeLibrary(Module);
}

void *DynamicLoadedLibrary::getSymbol(const std::string& symbol) const
{
    HINSTANCE Module = reinterpret_cast<HINSTANCE>(_handle);

    return reinterpret_cast<void *>(GetProcAddress(Module, symbol.c_str()));
}
