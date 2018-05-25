#ifndef QtExclusiveAppTest_H
#define QtExclusiveAppTest_H

#include "rclient/QtExclusiveApp.h"
#include "ta/timeutils.h"
#include "boost/thread/thread.hpp"
#include "boost/thread/condition_variable.hpp"
#include "boost/thread/mutex.hpp"
#include "cxxtest/TestSuite.h"
#include <string>

bool theQtAppCreated = false;
boost::condition_variable theQtAppCreatedCond;
boost::mutex theQtAppCreatedMutex;

class QtExclusiveAppTest : public CxxTest::TestSuite
{
    class QtThreadEngine
    {
    public:
        void operator()()
        {
            using namespace rclient;
            try
            {
                TS_ASSERT_EQUALS(QtExclusiveApp::exists(), QtExclusiveApp::notExists);
                {
                    rclient::QtExclusiveApp myApp;
                    TS_ASSERT_EQUALS(QtExclusiveApp::exists(), QtExclusiveApp::existsSameThread);
                    notifyQtAppCreated();
                    ta::TimeUtils::sleep(500);
                }
                TS_ASSERT_EQUALS(QtExclusiveApp::exists(), QtExclusiveApp::notExists);
            }
            catch (QtExclusiveAppLockError& e)
            {
                TS_ASSERT(false);
                TS_TRACE(e.what());
            }
            catch (std::runtime_error& e)
            {
                TS_ASSERT(false);
                TS_TRACE(e.what());
            }
            catch (...)
            {
                TS_ASSERT(!"Unknown exception");
            }
        }
    };
    static void waitForQtAppCreated()
    {
        {
            boost::unique_lock<boost::mutex> lock(theQtAppCreatedMutex);
            while(!theQtAppCreated)
            {
                theQtAppCreatedCond.wait(lock);
            }
        }
    }
    static void notifyQtAppCreated()
    {
        {
            boost::lock_guard<boost::mutex> lock(theQtAppCreatedMutex);
            theQtAppCreated = true;
        }
        theQtAppCreatedCond.notify_one();
    }
public:
	void testSameThread()
	{
        using namespace rclient;
        TS_ASSERT_EQUALS(QtExclusiveApp::exists(), QtExclusiveApp::notExists);
        {
            QtExclusiveApp myApp;
            TS_ASSERT_EQUALS(QtExclusiveApp::exists(), QtExclusiveApp::existsSameThread);
            TS_ASSERT_THROWS(QtExclusiveApp(), QtExclusiveAppLockError);
        }
        TS_ASSERT_EQUALS(QtExclusiveApp::exists(), QtExclusiveApp::notExists);
	}

    void testAnotherThread()
    {
        using namespace rclient;
        QtThreadEngine myQtThreadEngine;
        TS_ASSERT_EQUALS(QtExclusiveApp::exists(), QtExclusiveApp::notExists);
        boost::thread myQtThread(myQtThreadEngine);
        waitForQtAppCreated();
        TS_ASSERT_EQUALS(QtExclusiveApp::exists(), QtExclusiveApp::existAnotherThread);
        TS_ASSERT_THROWS(QtExclusiveApp(), QtExclusiveAppLockError);
        myQtThread.join();
        TS_ASSERT_EQUALS(QtExclusiveApp::exists(), QtExclusiveApp::notExists);

    }
};

#endif

