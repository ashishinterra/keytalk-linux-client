//----------------------------------------------------------------------------
//
//  Name          SingletonHolder.hpp
//  Description : Singleton holder template class
//  Supports:
//    Creation policies.
//    Manual and automatic destruction.
//    Thread-safe (DCL, see Double-Checked Locking pattern at http://en.wikipedia.org/wiki/Double_checked_locking_pattern).
//  Usage:
//   See unit tests
//
//----------------------------------------------------------------------------
#pragma once

#include "boost/thread/recursive_mutex.hpp"
#include <cassert>
#include <cstdlib>

namespace ta
{
    template<class T>
    class DefaultCreationPolicy
    {
    public:
        static T* createInstance() 	{ return new T(); }
    };

    template <class T, class CreationPolicy = DefaultCreationPolicy<T> >
    class SingletonHolder
    {
    public:
        typedef  boost::recursive_mutex::scoped_lock ScopedLock;
        typedef  boost::recursive_mutex Mutex;

        static T& instance();
        static void destroy();
        static bool isInstanceExist();
        static Mutex& getInstanceMutex();
    protected:
        SingletonHolder();
        ~SingletonHolder();
    private:
        SingletonHolder(const SingletonHolder& );
        const SingletonHolder& operator=(const SingletonHolder& );

        static void destroyInstance(T* anInstancePtr);
        static void scheduleForDestruction(void (*)());

        static T* theInstance;
        static Mutex theInstanceMutex;
    };

    template <class T, class CreationPolicy>
    SingletonHolder<T, CreationPolicy>::SingletonHolder()
    {
        assert(!theInstance);
    }

    template <class T, class CreationPolicy>
    SingletonHolder<T, CreationPolicy>::~SingletonHolder()
    {
        theInstance = NULL;
    }

    template <class T, class CreationPolicy>
    T& SingletonHolder<T, CreationPolicy>::instance()
    {
        if (!theInstance)
        {
            ScopedLock myLock(theInstanceMutex);
            if (!theInstance)
            {
                theInstance = CreationPolicy::createInstance();
                scheduleForDestruction(destroy);
            }
        }
        return *theInstance;
    }

    template <class T, class CreationPolicy>
    void SingletonHolder<T, CreationPolicy>::destroy()
    {
        if (theInstance)
        {
            ScopedLock myLock(theInstanceMutex);
            if (theInstance)
            {
                destroyInstance(theInstance);
                theInstance = NULL;
            }
        }
    }

    template <class T, class CreationPolicy>
    bool SingletonHolder<T, CreationPolicy>::isInstanceExist()
    {
        return !!theInstance;
    }

    template <class T, class CreationPolicy>
    typename SingletonHolder<T, CreationPolicy>::Mutex& SingletonHolder<T, CreationPolicy>::getInstanceMutex()
    {
        return theInstanceMutex;
    }

    template<class T, class CreationPolicy>
    void SingletonHolder<T, CreationPolicy>::scheduleForDestruction(void (*aFunPtr)())
    {
        atexit(aFunPtr);
    }

    template<class T, class CreationPolicy>
    void SingletonHolder<T, CreationPolicy>::destroyInstance(T* anInstance)
    {
        delete anInstance;
    }

    template <class T, class CreationPolicy>
    T* SingletonHolder<T, CreationPolicy>::theInstance = NULL;

    template <class T, class CreationPolicy>
    typename SingletonHolder<T, CreationPolicy>::Mutex SingletonHolder<T, CreationPolicy>::theInstanceMutex;
}
