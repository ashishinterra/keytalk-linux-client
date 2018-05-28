//----------------------------------------------------------------------------
//
//  Name          ScopedResource.hpp
//  Description : Scoped resource RAII class
//  Usage example:
//  ...
//  {
//    ScopedResource<FILE*> myFile(fopen("file.txt", "r"), fclose);
//    char myBuf[1024];
//    fread(myBuf, 1, sizeof(myBuf), myFile);
//    ...
//  }
//  // At this point fclose(myFile) has already been called
//
//----------------------------------------------------------------------------
#pragma once

#include "boost/utility.hpp"
#include <memory>

namespace ta
{
    struct IResourceDeleter
    {
        virtual void Delete() = 0;
        virtual ~IResourceDeleter() {}
    };

    template <class HandleType, class DeleteFunc>
    class ResourceDeleter : public IResourceDeleter
    {
    public:
        ResourceDeleter(HandleType aHandle, DeleteFunc aDeleteFunc) : theHandle(aHandle), theDeleteFunc(aDeleteFunc) {}
        virtual void Delete() {    theDeleteFunc(theHandle); }
    private:
        HandleType theHandle;
        DeleteFunc theDeleteFunc;
    };

    template <class HandleType>
    class ScopedResource: boost::noncopyable
    {
    public:
        ScopedResource()
            : theHandle(0), theInvalidHandle(0)
        {}

        template <class DeleteFunc>
        ScopedResource(HandleType aHandle, DeleteFunc aDeleteFunc, HandleType anInvalidHandle = 0)
            :  theHandle(aHandle), theDeleter(new ResourceDeleter<HandleType, DeleteFunc>(aHandle, aDeleteFunc)), theInvalidHandle(anInvalidHandle)
        {}

        template <class DeleteFunc>
        void assign(HandleType aHandle, DeleteFunc aDeleteFunc)
        {
            try
            {
                if (theHandle != theInvalidHandle)
                    theDeleter->Delete();
            }
            catch(...)
            {}
            theHandle = aHandle;
            theDeleter.reset(new ResourceDeleter<HandleType, DeleteFunc>(aHandle, aDeleteFunc));
        }


        operator HandleType() const { return theHandle; }
        HandleType operator ->() const { return theHandle; }

        HandleType detach()
        {
            HandleType myRetVal = theHandle;
            theHandle = theInvalidHandle;
            return myRetVal;
        }

        ~ScopedResource()
        {
            try
            {
                if (theHandle != theInvalidHandle)
                    theDeleter->Delete();
            }
            catch(...)
            {}
        }
    private:
        HandleType theHandle;
        std::auto_ptr<IResourceDeleter> theDeleter;
        const HandleType theInvalidHandle;
    };
}
