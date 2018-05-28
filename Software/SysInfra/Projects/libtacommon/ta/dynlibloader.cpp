#include "dynlibloader.h"
#include "strings.h"
#include "utils.h"
#include "scopedresource.hpp"
#include "common.h"
#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#else
# include <dlfcn.h>
#endif
#include <cassert>

using std::string;

namespace ta
{
#ifdef WIN32
    namespace
    {
        enum FindRefRetVal { Ok, InvalidLib};
        FindRefRetVal findUnresolvedReferencedLibs(const string& aLibName, string& anUnresolvedRefs)
        {
            anUnresolvedRefs.clear();
            ScopedResource<HMODULE> myLib(::LoadLibraryEx(aLibName.c_str(), NULL, DONT_RESOLVE_DLL_REFERENCES), ::FreeLibrary);
            if (!myLib)
                return InvalidLib;
            DWORD dwSize;
            PIMAGE_IMPORT_DESCRIPTOR myImportDesc = ( PIMAGE_IMPORT_DESCRIPTOR )::ImageDirectoryEntryToData( myLib, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &dwSize );
            if (!myImportDesc)
                return InvalidLib;
            for(; myImportDesc->Name; myImportDesc++ )
            {
                string myReferencedLibName = (PSTR)( (PBYTE)(HMODULE)myLib + myImportDesc->Name );
                HMODULE myReferencedLib = ::LoadLibrary(myReferencedLibName.c_str());
                if (!myReferencedLib)
                {
                    if (!anUnresolvedRefs.empty())
                        anUnresolvedRefs += ", ";
                    anUnresolvedRefs += myReferencedLibName;
                }
                ::FreeLibrary(myReferencedLib);
            }
            return Ok;
        }
    }
#endif

    struct DynLibLoader::DynLibLoaderImpl
    {
        DynLibLoaderImpl(UnloadOnExit anUnloadOnExit)
            : unloadOnExit(anUnloadOnExit), libHandle(NULL)
        {}
        ~DynLibLoaderImpl()
        {}
        const UnloadOnExit unloadOnExit;
#ifdef _WIN32
        HMODULE libHandle;
#else
        void* libHandle;
#endif
    };

    DynLibLoader::DynLibLoader(const string& aLibName, UnloadOnExit anUnloadOnExit)
        : theImplPtr(new DynLibLoaderImpl(anUnloadOnExit))
    {
        load(aLibName);
    }

    DynLibLoader::~DynLibLoader()
    {
        if (theImplPtr->unloadOnExit == unloadOnExitYes)
            unload();
        delete theImplPtr;
    }

    void* DynLibLoader::getFuncPtr(const string& aFuncName)
    {
        assert(theImplPtr->libHandle);
        void* myRetVal = NULL;
#ifdef _WIN32
        myRetVal = ::GetProcAddress(theImplPtr->libHandle, aFuncName.c_str());
        if (!myRetVal)
            TA_THROW_MSG(GetFuncPtrError, boost::format("Failed to get address of %1%. Last error %2%") % aFuncName % ::GetLastError());
#else
        myRetVal = ::dlsym(theImplPtr->libHandle, aFuncName.c_str());
        if (!myRetVal)
        {
            const char* myErr = dlerror();
            TA_THROW_MSG(GetFuncPtrError, boost::format("Failed to get address of %1%. %2%") % aFuncName % myErr);
        }
#endif
        return myRetVal;
    }

    // throw DynLibLoadError on error
    void DynLibLoader::load(const string& aLibName)
    {
        if (aLibName.empty())
            TA_THROW_MSG(DynLibLoadError, "Library name is empty");
#ifdef _WIN32
        ScopedResource<UINT> mySilentErrorMode(::SetErrorMode(SEM_FAILCRITICALERRORS), ::SetErrorMode);
        theImplPtr->libHandle = ::LoadLibrary(aLibName.c_str());
#else
        theImplPtr->libHandle = ::dlopen(aLibName.c_str(), RTLD_LAZY);
#endif
        if (!theImplPtr->libHandle)
        {
#ifdef _WIN32
            int myErr = ::GetLastError();
            if (myErr != ERROR_MOD_NOT_FOUND && myErr != ERROR_FILE_NOT_FOUND)
                TA_THROW_MSG(DynLibLoadError, boost::format("Failed to load '%1%'. Last error %2") % aLibName % myErr);
            string myNotLoadedRefs;
            FindRefRetVal myFindRefRetVal = findUnresolvedReferencedLibs(aLibName, myNotLoadedRefs);
            if (myFindRefRetVal == InvalidLib)
                TA_THROW_MSG(DynLibLoadError, boost::format("Failed to load '%1%'. Last error %2%. The specified path is not valid or does not contain a valid dll") % aLibName % myErr);
            if (myNotLoadedRefs.empty())
                TA_THROW_MSG(DynLibLoadError, boost::format("Failed to load '%1%'. Last error %2. No unresolved referenced libs found.") % aLibName % myErr);
            TA_THROW_MSG(DynLibLoadError, boost::format("Failed to load %1%. Unresolved referenced libs: '%2'") % aLibName % myNotLoadedRefs);
#else
            const char* myErr = dlerror();
            TA_THROW_MSG(DynLibLoadError, boost::format("Failed to load %1%. %2%") % aLibName % myErr);
#endif
        }
    }

    void DynLibLoader::unload()
    {
        if (!theImplPtr->libHandle)
            return;
#ifdef _WIN32
        ::FreeLibrary(theImplPtr->libHandle);
#else
        ::dlclose(theImplPtr->libHandle);
#endif
        theImplPtr->libHandle = NULL;
    }

    string DynLibLoader::makeLibName(const string& aBaseLibName)
    {
#ifdef _WIN32
        return aBaseLibName+".dll";
#else
        return "lib" + aBaseLibName+".so";
#endif
    }

}

