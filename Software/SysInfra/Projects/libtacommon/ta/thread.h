#pragma once

#include "boost/function.hpp"
#include "boost/thread/recursive_mutex.hpp"
#include "boost/thread/thread.hpp"

namespace ta
{
    typedef  boost::recursive_mutex::scoped_lock ScopedLock;
    typedef  boost::recursive_mutex Mutex;
    typedef  boost::thread Thread;
    typedef  boost::thread_resource_error ThreadError;

    namespace ThreadUtils
    {
        /**
          Gives up the remainder of the current thread's time slice, to allow other threads to run
         */
        void yield();

        /**
          Retrieve current thread ID

          @return Current thread ID
         */
        unsigned int getSelfId();

        /**
          Start detached thread

          @param[in] aFunc Thread function
         */
        void startDetachedThread(boost::function<void (void)> aFunc);
    }
}
