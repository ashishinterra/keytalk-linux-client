//----------------------------------------------------------------------------
//
//  Name          BrokerProxy.h
//  Description : Proxy class for the IE Broker implementataion
//
//----------------------------------------------------------------------------
#include "BrokerProxy.h"
#include "rclient/Bbp.h"
#include "rclient/CommonUtils.h"
#include "rclient/Settings.h"
#include "rclient/NativeCertStore.h"
#include "rclient/Common.h"
#include "ta/InternetExplorer.h"
#include "ta/thread.h"
#include "ta/logger.h"
#include "ta/logconfiguration.h"
#include "ta/utils.h"
#include "ta/process.h"
#include "ta/tcpserver.h"
#include "ta/common.h"

#include <windows.h>
#include <cstdlib>
#include <ctime>
#include "boost/static_assert.hpp"

using std::string;
using std::vector;
using namespace rclient;

static const unsigned int AcceptTimeout = 5000;


//
// BrokerProxy implementation
//

BrokerProxy::BrokerProxy()
    : theState(None)
{
    initLogger();
}

BrokerProxy::~BrokerProxy()
{
    deinitLogger();
}

//
//  Start the broker, submit a request job to it and waits for a response
//
//  throw std::exception on error
//
void BrokerProxy::invoke(const std::vector<char>& aSerReq, std::vector<char>& aSerResp)
{
    unsigned int myListeningPort;
    boost::uint32_t mySid = ta::genRand(RAND_MAX);
    IeBhoTcpSvrEngine mySvrEngine(myListeningPort, mySid, aSerReq, aSerResp, theState, theStateMtx, theStateCv);
    boost::thread myTcpServerThread(mySvrEngine);
    waitForListening(myTcpServerThread);

    const string myLaunchDir = rclient::Settings::getReseptInstallDir();
    const string myBrokerLaunchCmd = str(boost::format("%s\\%s %u %u") % myLaunchDir % rclient::ReseptIeBroker % myListeningPort % mySid);
    DEBUGLOG(boost::format("Server is listening on port %u, launching the broker %s...") % myListeningPort % myBrokerLaunchCmd);
    STARTUPINFO si = { sizeof(STARTUPINFO) };
    PROCESS_INFORMATION pi = {0};
    char* myCmd = new char[myBrokerLaunchCmd.length() + 1];
    strcpy(myCmd, myBrokerLaunchCmd.c_str());
    BOOL myRet = ::CreateProcess ( NULL, myCmd, NULL, NULL, TRUE, NORMAL_PRIORITY_CLASS, NULL, myLaunchDir.c_str(), &si, &pi );
    delete []myCmd;
    if (!myRet)
    {
        DWORD myLastError = ::GetLastError();
        waitForExit(myTcpServerThread);
        TA_THROW_MSG(BrokerProxyError, boost::format("Failed to launch the broker, Last Error: %d") % myLastError);
    }
    ::CloseHandle (pi.hProcess);
    ::CloseHandle (pi.hThread);

    if (!waitForExit(myTcpServerThread))
        TA_THROW_MSG(BrokerProxyError, "Server thread finished with error");
}

bool BrokerProxy::log(ta::LogLevel::val aLogLevel, const std::string& aMsg)
{
    const std::vector<char> mySerializedReq = bbp::serializeLogRequest(aLogLevel, aMsg);
    std::vector<char> mySerializedResp;
    try
    {
        DEBUGLOG("******* Submitting log request *******");
        invoke(mySerializedReq, mySerializedResp);
        bool myLogRetVal = bbp::deserializeLogResponse(mySerializedResp);

        DEBUGLOG(boost::format("******* Log request finished with retval %s *******") % (myLogRetVal ? "true":"false"));
        return myLogRetVal;
    }
    catch (std::exception& e)
    {
        ERRORDEVLOG(e.what());
        TA_THROW_MSG(BrokerError, e.what());
    }
}

unsigned int BrokerProxy::validateCert()
{
    const std::vector<char> mySerializedReq = bbp::serializeValidateCertRequest();
    std::vector<char> mySerializedResp;
    int myNumOfValidCerts;
    try
    {
        DEBUGLOG("******* Submitting validateCert request *******");
        invoke(mySerializedReq, mySerializedResp);
        myNumOfValidCerts = bbp::deserializeValidateCertResponse(mySerializedResp);
    }
    catch (std::exception& e)
    {
        ERRORDEVLOG(e.what());
        TA_THROW_MSG(BrokerError, e.what());
    }
    if (myNumOfValidCerts < 0)
        TA_THROW_MSG(rclient::NativeCertStoreValidateError, "Validate certs failed");
    DEBUGLOG(boost::format("******* Validate certs has been successfully finished with a number of valid certs %u *******") % myNumOfValidCerts);
    return static_cast<unsigned int>(myNumOfValidCerts);
}

unsigned int BrokerProxy::deleteAllReseptUserCerts()
{
    const std::vector<char> mySerializedReq = bbp::serializeDeleteCertRequest();
    std::vector<char> mySerializedResp;
    int myNumOfDeletedCerts;
    try
    {
        DEBUGLOG("******* Submitting deleteAllReseptUserCerts request *******");
        invoke(mySerializedReq, mySerializedResp);
        myNumOfDeletedCerts = bbp::deserializeDeleteCertResponse(mySerializedResp);
    }
    catch (std::exception& e)
    {
        ERRORDEVLOG(e.what());
        TA_THROW_MSG(BrokerError, e.what());
    }
    if (myNumOfDeletedCerts < 0)
        TA_THROW_MSG(NativeCertStoreDeleteError, "Delete certs failed");
    DEBUGLOG(boost::format("******* Delete certs has been successfully finished with a number of deleted certs %u *******") % myNumOfDeletedCerts);
    return static_cast<unsigned int>(myNumOfDeletedCerts);
}

bool BrokerProxy::loadReseptClientAuthUi(const std::vector<std::pair<std::string, std::string> >& aProviderServicePairs, const string& aReqestedUri, string& anUri2Go)
{
    const std::vector<char> mySerializedReq = bbp::serializeLoadAuthUiRequest(aProviderServicePairs, aReqestedUri);
    std::vector<char> mySerializedResp;
    bool myRetVal;
    try
    {
        DEBUGLOG("******* Submitting LoadAuthUi request *******");
        invoke(mySerializedReq, mySerializedResp);
        myRetVal = bbp::deserializeLoadAuthUiResponse(mySerializedResp, anUri2Go);
    }
    catch (std::exception& e)
    {
        ERRORDEVLOG(e.what());
        TA_THROW_MSG(BrokerError, e.what());
    }
    if (myRetVal)
        DEBUGLOG(boost::format("******* LoadAutUi has been successfully finished with a retval %s, Uri2Go %s *******") % (myRetVal ? "true": "false") % anUri2Go);
    else
        DEBUGLOG(boost::format("******* LoadAutUi has been successfully finished with a retval %s *******") % (myRetVal ? "true": "false"));
    return myRetVal;
}

//
// Waits for a server thread to turn into the listening or error state
// If server thread turns into the listening state, return true
// If server thread turns into the error state, wait for the thread to finish (joins) and return false
//
bool BrokerProxy::waitForListening(boost::thread& aTcpServerThread)
{
    {
        boost::recursive_mutex::scoped_lock lock(theStateMtx);
        while (theState != Listening && theState != ExitError)
            theStateCv.wait(lock);
        if (theState == Listening)
            return true;
    }
    DEBUGLOG("Server thread is in the error state, joining it end exiting...");
    aTcpServerThread.join();
    return false;
}

//
// Wait for the server thread to enter exit success / exit error state, wait for the thread to finish (join) and return true / false
//
bool BrokerProxy::waitForExit(boost::thread& aTcpServerThread)
{
    bool myRetVal = false;
    {
        boost::recursive_mutex::scoped_lock lock(theStateMtx);
        while (theState != ExitSuccess && theState != ExitError)
            theStateCv.wait(lock);
        myRetVal = (theState == ExitSuccess);
    }
    DEBUGLOG(boost::format("Server is in %d state, joining and exiting...") % theState);
    aTcpServerThread.join();
    return myRetVal;
}

// throw std::exception on error
void BrokerProxy::initLogger()
{
    string myIeInfo = "IE not installed";
    if (ta::InternetExplorer::isInstalled())
    {
        try
        {
            const ta::InternetExplorer::Version myVer = ta::InternetExplorer::getVersion();
            myIeInfo = str(boost::format("IE-%u.%u.%u") % myVer.major % myVer.minor % myVer.subminor);

            ta::InternetExplorer::ProtectedMode myProtectedMode = ta::InternetExplorer::getProtectedMode();
            if (myProtectedMode == ta::InternetExplorer::protectedModeOn)
                myIeInfo += " protected mode On";
            else if (myProtectedMode == ta::InternetExplorer::protectedModeOff)
                myIeInfo += " protected mode Off";
        }
        catch (std::runtime_error&)
        {}
    }
    string myEnvInfo = str(boost::format("%s Client-%s, %s") % resept::ProductName % toStr(rclient::ClientVersion) % myIeInfo);

    string myLogLevelStr = Settings::getLogLevel();
    ta::LogLevel::val myLogLevel;
    if (!ta::LogLevel::parse(myLogLevelStr.c_str(), myLogLevel))
        TA_THROW_MSG(LoggerInitError, "Failed to parse logging level " + myLogLevelStr);
    const string myLogDir = (ta::InternetExplorer::getProtectedMode() == ta::InternetExplorer::protectedModeOn) ? ta::InternetExplorer::getProtectedModeTempDir() : rclient::getLogDir();
    const string myLogFileName =  myLogDir + ta::getDirSep() + rclient::IeBrokerProxyLogName;

    ta::LogConfiguration::Config myMemConfig;
    myMemConfig.fileAppender = true;
    myMemConfig.fileAppenderLogThreshold = myLogLevel;
    myMemConfig.fileAppenderLogFileName = myLogFileName;
    ta::LogConfiguration::instance().load(myMemConfig);

    PROLOG(myEnvInfo);
}


void BrokerProxy::deinitLogger()
{
    EPILOG("RESEPT Client-" + toStr(rclient::ClientVersion));
}


//
// BrokerProxy::IeBhoTcpSvrEngine implementation
//

BrokerProxy::IeBhoTcpSvrEngine::IeBhoTcpSvrEngine(unsigned int& aPort,
        boost::uint32_t anSid,
        const std::vector<char>& aSerializedReq,
        std::vector<char>& aSerializedResp,
        State& aState,
        boost::recursive_mutex& aStateMtx,
        boost::condition& aStateCv)
    : thePort(aPort)
    , theSid(anSid)
    , theSerializedReq(aSerializedReq)
    , theSerializedResp(aSerializedResp)
    , theState(aState)
    , theStateMtx(aStateMtx)
    , theStateCv(aStateCv)
{}

// Core TCP server logic
void BrokerProxy::IeBhoTcpSvrEngine::operator()()
{
    try
    {
        ta::TcpServer mySvr;
        thePort = mySvr.listen("127.0.0.1");
        DEBUGLOG(boost::format("Listening on 127.0.0.1:%u") % thePort);
        notifyListening();
        std::auto_ptr<ta::TcpClient> myConnection(mySvr.accept(AcceptTimeout));
        DEBUGLOG("Incoming connection");

        std::vector<char> myReceivedData =  myConnection->receiveAll(sizeof(boost::uint32_t));
        boost::uint32_t mySid = bbp::deserializeSid(myReceivedData);
        if (mySid != theSid)
        {
            ERRORLOG(boost::format("Received wrong session id from the broker. Received %u, expected %u. Exiting with error") % mySid % theSid);
            notifyExitError();
            return;
        }
        DEBUGLOG("Handshake succeeded, sending request");
        myConnection->sendAll(theSerializedReq);

        vector<char> mySerializedBrokerRetVal =  myConnection->receiveAll(sizeof(boost::uint32_t));
        boost::uint32_t myBrokerRetVal = bbp::deserializeBrokerRetVal(mySerializedBrokerRetVal);
        DEBUGLOG(boost::format("Received broker retval: %u") % myBrokerRetVal);
        if (myBrokerRetVal != bbp::BrokerRetVal::Ok)
        {
            ERRORLOG("Exiting with error (bad broker retval)");
            notifyExitError();
            return;
        }
        boost::uint32_t myRespSize = bbp::deserializeSize(myConnection->receiveAll(sizeof(boost::uint32_t)));
        theSerializedResp =  myConnection->receiveAll(myRespSize);
        DEBUGLOG(boost::format("Received response size %u. Exiting with success") % (unsigned int)theSerializedResp.size());
        notifyExitSuccess();
    }
    catch (std::exception& e)
    {
        ERRORLOG2("Broker error", e.what());
        notifyExitError();
    }
}

void BrokerProxy::IeBhoTcpSvrEngine::notifyListening()
{
    {
        boost::lock_guard<boost::recursive_mutex> lock(theStateMtx);
        theState = Listening;
    }
    theStateCv.notify_one();
}

void BrokerProxy::IeBhoTcpSvrEngine::notifyExitSuccess()
{
    {
        boost::lock_guard<boost::recursive_mutex> lock(theStateMtx);
        theState = ExitSuccess;
    }
    theStateCv.notify_one();
}

void BrokerProxy::IeBhoTcpSvrEngine::notifyExitError()
{
    {
        boost::lock_guard<boost::recursive_mutex> lock(theStateMtx);
        theState = ExitError;
    }
    theStateCv.notify_one();
}
