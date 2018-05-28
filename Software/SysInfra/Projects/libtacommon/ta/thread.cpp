#include "thread.h"
#ifdef _WIN32
# include <windows.h>
#elif defined(__linux__)
# include <sys/syscall.h>
# include <unistd.h>
# include <pthread.h>
#endif

namespace ta
{
    namespace ThreadUtils
    {
        void yield()
        {
#ifdef _WIN32
            ::SwitchToThread();
#else
            pthread_yield();
#endif
        }

        unsigned int getSelfId()
        {
#ifdef _WIN32
            return ::GetCurrentThreadId();
#elif defined(__linux__)
            return syscall(SYS_gettid);
#endif
        }

        void startDetachedThread(boost::function<void (void)> aFunc)
        {
            Thread myThread(aFunc);
        }
    }
}
