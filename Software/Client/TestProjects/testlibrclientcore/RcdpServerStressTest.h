#pragma once

#include "cxxtest/TestSuite.h"
#include "rclient/RcdpHandler.h"
#include "rclient/RcdpRequest.h"
#include "rclient/Settings.h"
#include "rclient/Common.h"
#include "resept/util.h"
#include "ta/certutils.h"
#include "ta/timeutils.h"
#include "ta/netutils.h"
#include "ta/logger.h"
#include "ta/scopedresource.hpp"
#include "ta/url.h"
#include "ta/utils.h"
#include "ta/common.h"

#include "boost/assign/list_of.hpp"
#include "boost/thread/thread.hpp"
#include "boost/thread/condition_variable.hpp"
#include "boost/thread/mutex.hpp"
#include "boost/assign/list_of.hpp"
#include <string>
#include <vector>

unsigned int theNumberOfThreadsCreated = 0;
unsigned int theNumberOfSucceededTests = 0;
boost::mutex theNumberOfThreadsCreatedMutex;
boost::mutex theNumberOfSucceededTestsMutex;

bool theThreadToRun = false;
boost::condition_variable theThreadToRunCond;
boost::mutex theThreadToRunMutex;

class RcdpServerStressTest : public CxxTest::TestSuite
{
    class GetCertEngine
    {
    public:
        GetCertEngine(const ta::NetUtils::RemoteAddress& aSvr) : theSvr(aSvr)
        {}

        void operator()()
        {
            try
            {
                {
                    boost::unique_lock<boost::mutex> lock(theNumberOfThreadsCreatedMutex);
                    ++theNumberOfThreadsCreated;
                }
                waitForThreadToRun();
                if (requestCertificate())
                {
                    boost::unique_lock<boost::mutex> lock(theNumberOfSucceededTestsMutex);
                    ++theNumberOfSucceededTests;
                }
            }
            catch (std::exception& e)
            {
                TS_ASSERT(false);
                TS_TRACE(e.what());
            }
            catch (...)
            {
                TS_ASSERT(!"Unknown exception");
            }
        }

        //@return success flag
        bool requestCertificate()
        {
            using namespace resept::rcdpv2;
            using resept::Credential;
            using boost::assign::list_of;
            using boost::assign::map_list_of;
            using std::string;

            const resept::Credentials myCreds = list_of(Credential(resept::credUserId, "DemoUser"))
                                                  (Credential(resept::credHwSig, "does-not-matter"));

            try
            {
                rclient::RcdpHandler myRcdp(theSvr);

                 myRcdp.hello();
                 myRcdp.handshake();
                 myRcdp.getAuthRequirements(Service);
                 if (myRcdp.authenticate(Service, myCreds).auth_result.type != resept::AuthResult::Ok)
                 {
                    return false;
                 }
                 const rclient::CertResponse myCertResult = myRcdp.getCert(resept::certformatP12, false);
                 if (ta::CertUtils::parsePfx(myCertResult.cert, myCertResult.password) != 1)
                 {
                    return false;
                 }
                 myRcdp.eoc();
                 // OK!
                return true;
            }
            catch (std::exception& e)
            {
                TS_TRACE(e.what());
            }
            catch (...)
            {
                TS_TRACE("Unknown exception");
            }
            return false;
        }
    private:
        const ta::NetUtils::RemoteAddress theSvr;
    }; // GetCertEngine

    static void waitForThreadToRun()
    {
        {
            boost::unique_lock<boost::mutex> lock(theThreadToRunMutex);
            while(!theThreadToRun)
            {
                theThreadToRunCond.wait(lock);
            }
        }
    }

    static void notifyThreadToRun()
    {
        {
            boost::lock_guard<boost::mutex> lock(theThreadToRunMutex);
            theThreadToRun = true;
        }
        theThreadToRunCond.notify_all();
    }

    void doTestConcurrent(const unsigned int aNumThreads, const unsigned int aDeadlineSec)
    {
        TS_TRACE(str(boost::format("Test getting certificate from %u concurrent threads") % aNumThreads).c_str());
        try
        {
            // Prepare
            while (true)
            {
                {
                    boost::unique_lock<boost::mutex> lock(theNumberOfThreadsCreatedMutex);
                    if (theNumberOfThreadsCreated == aNumThreads)
                    {
                        break;
                    }
                }
                HandlerEntry myHandlerEntry;
                myHandlerEntry.handler = new GetCertEngine(theSvr);
                myHandlerEntry.thread = new boost::thread(*myHandlerEntry.handler);
                theHandlers.push_back(myHandlerEntry);
                ta::TimeUtils::sleep(1);
            }

            // Start
            notifyThreadToRun();
            const time_t myStartTime = time(NULL);
            foreach (HandlerEntry& entry, theHandlers)
            {
                entry.thread->join();
            }
            const unsigned int myElapsedTimeSec = time(NULL) - myStartTime;

            // Wait for done, verify
            if (theNumberOfSucceededTests == aNumThreads)
            {
                TS_TRACE(str(boost::format("Get %d certificates (concurrently) within %d seconds (%.02lf sec per certificate)\n") % aNumThreads % myElapsedTimeSec % ((double)myElapsedTimeSec/(double)aNumThreads)).c_str());
                TS_ASSERT(myElapsedTimeSec <= aDeadlineSec);
            }
            else
            {
                TS_WARN(str(boost::format("Got %d certificates (concurrently) out of %d expected within %d seconds\n") % theNumberOfSucceededTests % aNumThreads % myElapsedTimeSec).c_str());
                TS_ASSERT_EQUALS(theNumberOfSucceededTests, aNumThreads);// will fail
            }
        }
        catch (std::exception& e)
        {
            TS_TRACE(e.what());
            TS_ASSERT(false);
        }
        catch (...)
        {
            TS_ASSERT(!"Unknown exception");
        }

        foreach (HandlerEntry entry, theHandlers)
        {
            delete entry.thread;
            delete entry.handler;
        }
    }

public:
    void setUp()
    {
        CxxTest::setAbortTestOnFail(false);
        theSvr = rclient::Settings::getReseptSvrAddress();

        // Warm-up curl DNS cache by handling dummy request since we don't want to measure DNS resolution time in our tests
        rclient::RcdpHandler(theSvr).eoc();

        theNumberOfThreadsCreated = 0;
        theNumberOfSucceededTests = 0;
    }


    void _testConcurrent_10_threads()
    {
        const unsigned int myNumThreads = 10;
        const unsigned int myDeadlineSec = 10;

        doTestConcurrent(myNumThreads, myDeadlineSec);
    }

    void testConcurrent_20_threads()
    {
        const unsigned int myNumThreads = 20;
        const unsigned int myDeadlineSec = 20;

        doTestConcurrent(myNumThreads, myDeadlineSec);
    }

    void testSequential()
    {
        const unsigned int myNumberOfRequests = 10;
        const unsigned int myExpectedMaxAvgRequestLatencySec = 2;
        int myTotalCertsReceived = 0;

        TS_TRACE(str(boost::format("Test getting certificate using %d sequential requests") % myNumberOfRequests).c_str());
        const time_t myStartTime = time(NULL);
        GetCertEngine myEngine(theSvr);
        for (unsigned int i=0; i < myNumberOfRequests; ++i)
        {
            const bool myIsCertReceived = myEngine.requestCertificate();
            if (myIsCertReceived)
            {
                ++myTotalCertsReceived;
            }
            TS_ASSERT(myIsCertReceived);
        }
        const unsigned int myElapsedTimeSec = time(NULL) - myStartTime;
        TS_TRACE(str(boost::format("Get %d certificates (sequentially) within %d seconds (%.02lf sec per RCDP session)\n") % myTotalCertsReceived % myElapsedTimeSec % ((double)myElapsedTimeSec/(double)myTotalCertsReceived)).c_str());
        TS_ASSERT(myElapsedTimeSec <= myExpectedMaxAvgRequestLatencySec * myNumberOfRequests);
    }

private:
    static const std::string Service;

    struct HandlerEntry
    {
       HandlerEntry(): handler(NULL), thread(NULL){}
       GetCertEngine* handler;
       boost::thread* thread;
    };
    std::vector<HandlerEntry> theHandlers;
    ta::NetUtils::RemoteAddress theSvr;
};

const std::string RcdpServerStressTest::Service = "CUST_ANO_INTERNAL";
