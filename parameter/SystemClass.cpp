/*
 * Copyright (c) 2011-2015, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <dlfcn.h>
#include <dirent.h>
#include <algorithm>
#include <ctype.h>
#include "SystemClass.h"
#include "SubsystemLibrary.h"
#include "VirtualSubsystem.h"
#include "LoggingElementBuilderTemplate.h"
#include <assert.h>
#include "PluginLocation.h"
#include "DynamicLoadedLibrary.hpp"
#include "Utility.h"
#include "Memory.hpp"

#define base CConfigurableElement

using std::list;
using std::string;

// FIXME: integrate SystemClass to core namespace
using namespace core;

/**
 * A plugin file name is of the form:
 * lib<type>-subsystem.so or lib<type>-subsystem._host.so
 *
 * The plugin symbol is of the form:
 * get<TYPE>SubsystemBuilder
*/
// Plugin file naming
const char* gpcPluginSuffix = "-subsystem";
const char* gpcPluginPrefix = "lib";

// Plugin symbol naming
const char* gpcPluginSymbolPrefix = "get";
const char* gpcPluginSymbolSuffix = "SubsystemBuilder";

// Used by subsystem plugins
typedef void (*GetSubsystemBuilder)(CSubsystemLibrary*, core::log::Logger& logger);

CSystemClass::CSystemClass(log::Logger& logger)
    : _pSubsystemLibrary(new CSubsystemLibrary()), _logger(logger)
{
}

CSystemClass::~CSystemClass()
{
    delete _pSubsystemLibrary;

    // Destroy child subsystems *before* unloading the libraries (otherwise crashes will occur
    // as unmapped code will be referenced)
    clean();

    // Close all previously opened subsystem libraries
    list<DynamicLoadedLibrary *>::const_iterator it;

    for (it = _subsystemLibraryHandleList.begin(); it != _subsystemLibraryHandleList.end(); ++it) {

        delete *it;
    }
}

bool CSystemClass::childrenAreDynamic() const
{
    return true;
}

string CSystemClass::getKind() const
{
    return "SystemClass";
}

bool CSystemClass::loadSubsystems(string& strError,
                                  const CSubsystemPlugins* pSubsystemPlugins,
                                  bool bVirtualSubsystemFallback)
{
    // Start clean
    _pSubsystemLibrary->clean();

    typedef TLoggingElementBuilderTemplate<CVirtualSubsystem> VirtualSubsystemBuilder;
    // Add virtual subsystem builder
    _pSubsystemLibrary->addElementBuilder("Virtual", new VirtualSubsystemBuilder(_logger));
    // Set virtual subsytem as builder fallback if required
    if (bVirtualSubsystemFallback) {
        _pSubsystemLibrary->setDefaultBuilder(make_unique<VirtualSubsystemBuilder>(_logger));
    }

    // Add subsystem defined in shared libraries
    core::Results errors;
    bool bLoadPluginsSuccess = loadSubsystemsFromSharedLibraries(errors, pSubsystemPlugins);

    // Fill strError for caller, he has to decide if there is a problem depending on
    // bVirtualSubsystemFallback value
    CUtility::asString(errors, strError);

    return bLoadPluginsSuccess || bVirtualSubsystemFallback;
}

bool CSystemClass::loadSubsystemsFromSharedLibraries(core::Results& errors,
                                                     const CSubsystemPlugins* pSubsystemPlugins)
{
    // Plugin list
    list<string> lstrPluginFiles;

    uint32_t uiPluginLocation;

    for (uiPluginLocation = 0; uiPluginLocation <  pSubsystemPlugins->getNbChildren(); uiPluginLocation++) {

        // Get Folder for current Plugin Location
        const CPluginLocation* pPluginLocation = static_cast<const CPluginLocation*>(pSubsystemPlugins->getChild(uiPluginLocation));

        string strFolder(pPluginLocation->getFolder());
        if (!strFolder.empty()) {
            strFolder += "/";
        }
        // Iterator on Plugin List:
        list<string>::const_iterator it;

        const list<string>& pluginList = pPluginLocation->getPluginList();

        for (it = pluginList.begin(); it != pluginList.end(); ++it) {

            // Fill Plugin files list
            lstrPluginFiles.push_back(strFolder + *it);
        }
    }

    // Actually load plugins
    while (!lstrPluginFiles.empty()) {

        // Because plugins might depend on one another, loading will be done
        // as an iteration process that finishes successfully when the remaining
        // list of plugins to load gets empty or unsuccessfully if the loading
        // process failed to load at least one of them

        // Attempt to load the complete list
        if (!loadPlugins(lstrPluginFiles, errors)) {

            // Unable to load at least one plugin
            break;
        }
    }

    if (!lstrPluginFiles.empty()) {
        // Unable to load at least one plugin
        string strPluginUnloaded;
        CUtility::asString(lstrPluginFiles, strPluginUnloaded, ", ");

        errors.push_back("Unable to load the following plugins: " + strPluginUnloaded + ".");
        return false;
    }

    return true;
}

// Plugin symbol computation
string CSystemClass::getPluginSymbol(const string& strPluginPath)
{
    // Extract plugin type out of file name
    string strPluginSuffix = gpcPluginSuffix;
    string strPluginPrefix = gpcPluginPrefix;

    // Remove folder and library prefix
    size_t iPluginTypePos = strPluginPath.rfind('/') + 1 + strPluginPrefix.length();

    // Get index of -subsystem.so or -subsystem_host.so suffix
    size_t iSubsystemPos = strPluginPath.find(strPluginSuffix, iPluginTypePos);

    // Get type (between iPluginTypePos and iSubsystemPos)
    string strPluginType = strPluginPath.substr(iPluginTypePos, iSubsystemPos - iPluginTypePos);

    // Make it upper case
    std::transform(strPluginType.begin(), strPluginType.end(), strPluginType.begin(), ::toupper);

    // Get plugin symbol
    return gpcPluginSymbolPrefix + strPluginType + gpcPluginSymbolSuffix;
}

// Plugin loading
bool CSystemClass::loadPlugins(list<string>& lstrPluginFiles, core::Results& errors)
{
    assert(lstrPluginFiles.size());

    bool bAtLeastOneSubsystemPluginSuccessfullyLoaded = false;

    list<string>::iterator it = lstrPluginFiles.begin();

    while (it != lstrPluginFiles.end()) {

        string strPluginFileName = *it;

        // Load attempt
        DynamicLoadedLibrary *library;
        try {
            library = new DynamicLoadedLibrary(strPluginFileName);

        } catch (std::exception& e) {
            errors.push_back(e.what());

            // Next plugin
            ++it;

            continue;
        }

        // Store libraries handles
        _subsystemLibraryHandleList.push_back(library);

        // Get plugin symbol
        string strPluginSymbol = getPluginSymbol(strPluginFileName);

        // Load symbol from library
        GetSubsystemBuilder pfnGetSubsystemBuilder =
                    reinterpret_cast<GetSubsystemBuilder>(library->getSymbol(strPluginSymbol.c_str()));

        if (!pfnGetSubsystemBuilder) {

            errors.push_back("Subsystem plugin " + strPluginFileName +
                             " does not contain " + strPluginSymbol + " symbol.");
            // Next plugin
            ++it;

            continue;
        }

        // Account for this success
        bAtLeastOneSubsystemPluginSuccessfullyLoaded = true;

        // Fill library
        pfnGetSubsystemBuilder(_pSubsystemLibrary, _logger);

        // Remove successfully loaded plugin from list and select next
        lstrPluginFiles.erase(it++);
    }

    return bAtLeastOneSubsystemPluginSuccessfullyLoaded;
}

const CSubsystemLibrary* CSystemClass::getSubsystemLibrary() const
{
    return _pSubsystemLibrary;
}

void CSystemClass::checkForSubsystemsToResync(CSyncerSet& syncerSet, core::Results& infos)
{
    size_t uiNbChildren = getNbChildren();
    size_t uiChild;

    for (uiChild = 0; uiChild < uiNbChildren; uiChild++) {

        CSubsystem* pSubsystem = static_cast<CSubsystem*>(getChild(uiChild));

        // Collect and consume the need for a resync
        if (pSubsystem->needResync(true)) {

            infos.push_back("Resynchronizing subsystem: " + pSubsystem->getName());
            // get all subsystem syncers
            pSubsystem->fillSyncerSet(syncerSet);
        }
    }
}

void CSystemClass::cleanSubsystemsNeedToResync()
{
    size_t uiNbChildren = getNbChildren();
    size_t uiChild;

    for (uiChild = 0; uiChild < uiNbChildren; uiChild++) {

        CSubsystem* pSubsystem = static_cast<CSubsystem*>(getChild(uiChild));

        // Consume the need for a resync
        pSubsystem->needResync(true);
    }
}
