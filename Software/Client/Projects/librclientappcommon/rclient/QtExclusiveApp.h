//----------------------------------------------------------------------------
//
//  Name          QtExclusiveApp.h
//  Description : QtExclusiveApp class declaration
//
//                QtExclusiveApp initializes Qt library by creating a QApplication object and initializing RClientAppCommon resources.
//                When the QtExclusiveApp object is destructed, QApplication object is destructed and RClientAppCommon resources are deinitialized.
//                Only one QtExclusiveApp object can be created within a process at a time, an attempt to create QtExclusiveApp object while another QtExclusiveApp
//                object already exists in another thread of the process will cause the QtExclusiveApp c'tor to throw QtExclusiveAppLockError.
//                QtExclusiveApp is not recursive, thus QtExclusiveAppLockError is also thrown on attempt to create more than one QtExclusiveApp object within the same thread.
//                QtExclusiveApp is non-blocking, that is it tries to lock and, if not successful, throws exception instead of waiting until it manage to acquire the the lock.
//                The rationale for choosing non-blocking behavior is that RESEPT client usage is is not supposed to be multitasking (i.e. running more than one RESEPT browser client at once) thus the inability to acquire the lock is considered as an error.
//
//                A typical usage of QtExclusiveApp is to initialize Qt library in multi-threaded applications, where Qt GUI operations can be performed by any thread.
//                Within RESEPT such an applications is ReseptIeClient (IE creates a separate thread for each tab)
//                Usage:
//                {
//                   QtExclusiveApp myQtExclusiveApp;
//                   ... here go Qt UI operations
//                }
//                catch (QtExclusiveAppLockError& e)
//                {
//                  another instance of UI is already running...
//                }
//                catch (...)
//                {
//                   system error occurred...
//                }
//
//                Because of increased complexity it is recommended to minimize the usage of this class and redesign your app such that
//                Qt GUI operations are performed in the same thread the Qt library was initialized in (e.g. putting BL and UI to separate threads).
//                Such applications should use QtAppSingleton class instead of QtExclusiveApp.
//
//
//----------------------------------------------------------------------------
#ifndef RCLIENT_QTEXCLUSIVEAPP_H
#define RCLIENT_QTEXCLUSIVEAPP_H

#include "boost/thread/recursive_mutex.hpp"
#include "boost/thread/tss.hpp"
#include "boost/utility.hpp"
#include <memory>
#include <stdexcept>

class QApplication;

namespace rclient
{

    struct QtExclusiveAppLockError : std::runtime_error
    {
        explicit QtExclusiveAppLockError(const std::string& aMessage = "") : std::runtime_error(aMessage) {}
    };

    //
    // Non-recursive non-blocking behavior
    //
    class QtExclusiveApp: private boost::noncopyable
    {
    public:
        //
        // Never blocks, throws QtExclusiveAppLockError if QtExclusiveApp
        // instance already exists in the same or another thread
        // @throws std::runtime_error on other errors
        //
        QtExclusiveApp();
        ~QtExclusiveApp();

        // Check where QtExclusiveApp instance exists
        enum Location
        {
            existsSameThread,
            existAnotherThread,
            notExists
        };
        // @throws std::runtime_error
        static Location exists();
    private:
        void init();
        void deinit();
        static boost::thread_specific_ptr<QApplication> theQtAppPtr;
        static boost::recursive_mutex theMutex;
        std::auto_ptr<boost::recursive_mutex::scoped_try_lock> theLock;
    };
}

#endif