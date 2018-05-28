/**
@brief DynLibLoader class wrapping platform-dependent usage of dynamic/shared libraries.
*/
#pragma once

#include <string>
#include <stdexcept>
#include "boost/utility.hpp"

namespace ta
{
    struct DynLibLoadError : std::runtime_error
    {
        explicit DynLibLoadError(const std::string& aMessage = "")	: std::runtime_error(aMessage) {}
    };

    struct GetFuncPtrError : std::runtime_error
    {
        explicit GetFuncPtrError(const std::string& aMessage = "")	: std::runtime_error(aMessage) {}
    };

    /**
      Dynamic library loader class
    */
    class DynLibLoader: boost::noncopyable
    {
    public:


        enum UnloadOnExit { unloadOnExitYes, unloadOnExitNo };
        /**
          Load library dynamically during runtime

          @param[in] aLibName path to the library
          @param[in] anUnloadOnExit whether the librasy should be unloaded when d'tor is called
          @throw DynLibLoadError
         */
        DynLibLoader(const std::string& aLibName, UnloadOnExit anUnloadOnExit = unloadOnExitYes);
        ~DynLibLoader();

        /**
          Retrieve function from library

          @param[in] aFuncName function name

          @return Pointer to the exported function
          @post Return value is not NULL
          @throw GetFuncPtrError
        */
        void* getFuncPtr(const std::string& aFuncName);

        /**
          Make a library name from the base library name by adding a platform-dependent prefix and suffix
          Example: Win32 "MyLibrary" -> "MyLibrary.dll"
                   Linux    "MyLibrary" -> "libMyLibrary.so"
                   Mac OS X "MyLibrary" -> "libMyLibrary.dylib"

          @param[in] aBaseLibName String of base library name
          @return String of library name
        */
        static std::string makeLibName(const std::string& aBaseLibName);

    private:
        void load(const std::string& aLibName);
        void unload();
    private:
        struct DynLibLoaderImpl;
        DynLibLoaderImpl* theImplPtr;
    };
}
