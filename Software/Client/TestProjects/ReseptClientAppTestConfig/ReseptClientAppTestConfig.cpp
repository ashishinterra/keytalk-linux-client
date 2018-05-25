#include "ReseptClientAppTestConfig.h"
#include "ta/common.h"


// Ignore warnings caused by the usage of exception specification in libconfig
#ifdef _MSC_VER
#pragma warning (disable: 4290)
#endif
#include "libconfig.h++"
#ifdef _MSC_VER
#pragma warning (default: 4290)
#endif


using std::string;
using namespace ta;

const string ConfigFileName = "ReseptClientAppTest.conf";

const string ServicesList    = "Services";
const string ServiceName     = "name";
const string ServiceUserid   = "user";
const string ServiceUserLocked = "locked";
const string ServicePassword = "password";
const string ServiceNewPassword = "new_password";
const string ServicePincode  = "pincode";

const string crfile_required = "crfile_required";

static unsigned int getListSize(const libconfig::Config& aConfig, const string& aPath)
{
    if (!aConfig.exists(aPath))
        TA_THROW_MSG(ReseptClientAppTestConfigError, boost::format("%s setting does not exist") % aPath);
    libconfig::Setting& myListSetting = aConfig.lookup(aPath);
    if (!myListSetting.isList())
        TA_THROW_MSG(ReseptClientAppTestConfigError, boost::format("%s setting is not a list") % aPath);
    int myListSize = myListSetting.getLength();
    if (myListSize < 0)
        TA_THROW_MSG(ReseptClientAppTestConfigError, boost::format("Negative number of elements in the %s list?!") % aPath);
    return static_cast<unsigned int>(myListSize);
}

// @return whether the option exists
template <class OptionType>
bool getOptionValue(const libconfig::Config& aConfig, const string& aServiceName, const string& aUserId, const string& anOptionName, OptionType& anOptionVal)
 {
     const unsigned int myNumElems = getListSize(aConfig, ServicesList);
     for (size_t i=0; i<myNumElems; ++i)
     {
         string myPath = str(boost::format("%s.[%d].%s") % ServicesList % i % ServiceName);
         string myServiceName;
         if (!aConfig.lookupValue(myPath, myServiceName))
             TA_THROW_MSG (ReseptClientAppTestConfigError, boost::format("Path %s not found in %s or it is not a string") % myPath % ConfigFileName);

         if (myServiceName == aServiceName)
         {
             myPath = str(boost::format("%s.[%d].%s") % ServicesList % i % ServiceUserid);
             string userid;
             if (aConfig.lookupValue(myPath, userid) && userid == aUserId)
             {
                 myPath = str(boost::format("%s.[%d].%s") % ServicesList % i % anOptionName);
                 if (aConfig.lookupValue(myPath, anOptionVal))
                     return true;
             }
         }
     }
     return false;
 }

template <class OptionType>
void setOptionValue(const libconfig::Config& aConfig, const string& aServiceName, const string& aUserId, const string& anOptionName, const OptionType& anOptionValue)
{
    const unsigned int myNumElems = getListSize(aConfig, ServicesList);
    for (size_t i=0; i< myNumElems; ++i)
     {
         string myPath = str(boost::format("%s.[%d].%s") % ServicesList % i % ServiceName);
         string myServiceName;
         if (!aConfig.lookupValue(myPath, myServiceName))
             TA_THROW_MSG (ReseptClientAppTestConfigError, boost::format("Path %s not found in %s or it is not a string") % myPath % ConfigFileName);

         if (myServiceName == aServiceName)
         {
             myPath = str(boost::format("%s.[%d].%s") % ServicesList % i % ServiceUserid);
             string userid;
             if (aConfig.lookupValue(myPath, userid) && userid == aUserId)
             {
                 myPath = str(boost::format("%s.[%d].%s") % ServicesList % i % anOptionName);
                 aConfig.lookup(myPath) = anOptionValue;
                 return;
             }
         }
     }
     TA_THROW_MSG(ReseptClientAppTestConfigError, "No option " + anOptionName + " found for service " + aServiceName + " and user " + aUserId);
}

ReseptClientAppTestConfig::ReseptClientAppTestConfig()
:  theConfig(NULL)
{
    try
    {
        theConfig = new libconfig::Config();
        theConfig->readFile(ConfigFileName.c_str());
    }
    catch (libconfig::ParseException& e)
    {
        TA_THROW_MSG(ReseptClientAppTestConfigError, boost::format("Failed to parse %s. line %d : %s") % ConfigFileName % e.getLine() % e.getError());
    }
    catch (std::exception& e)
    {
        TA_THROW_MSG(ReseptClientAppTestConfigError, boost::format("Failed to open %s. %s.") % ConfigFileName % e.what());
    }
}

ReseptClientAppTestConfig::~ReseptClientAppTestConfig()
{
    delete theConfig;
}

bool ReseptClientAppTestConfig::isServiceExist(const string& aServiceName) const
{
   const unsigned int myNumElems = getListSize(*theConfig, ServicesList);
    for (size_t i=0; i<myNumElems; ++i)
    {
        string myPath = str(boost::format("%s.[%d].%s") % ServicesList % i % ServiceName);
        string myServiceName;
        if (!theConfig->lookupValue(myPath, myServiceName))
            TA_THROW_MSG (ReseptClientAppTestConfigError, boost::format("Path %s not found in %s or it is not a string") % myPath % ConfigFileName);
        if (myServiceName == aServiceName)
            return true;
    }
    return false;
}

bool ReseptClientAppTestConfig::isUserExist(const string& aServiceName, const string& aUserId) const
{
    const unsigned int myNumElems = getListSize(*theConfig, ServicesList);
    for (size_t i=0; i<myNumElems; ++i)
    {
        string myPath = str(boost::format("%s.[%d].%s") % ServicesList % i % ServiceName);
        string myServiceName;
        if (!theConfig->lookupValue(myPath, myServiceName))
            TA_THROW_MSG (ReseptClientAppTestConfigError, boost::format("Path %s not found in %s or it is not a string") % myPath % ConfigFileName);

        if (myServiceName == aServiceName)
        {
            myPath = str(boost::format("%s.[%d].%s") % ServicesList % i % ServiceUserid);
            string userid;
            if (theConfig->lookupValue(myPath, userid) && userid == aUserId)
                return true;
        }
    }
    return false;
}

bool ReseptClientAppTestConfig::isUserLocked(const string& aServiceName, const string& aUserId) const
{
    bool myIsLocked;
    if (!getOptionValue(*theConfig, aServiceName, aUserId, ServiceUserLocked, myIsLocked))
        TA_THROW_MSG (ReseptClientAppTestConfigError, boost::format("No user lock flag found for service %s, user %s in %s") % aServiceName % aUserId % ConfigFileName);
    return myIsLocked;
}

bool ReseptClientAppTestConfig::isPasswordExist(const string& aServiceName, const string& aUserId) const
{
    string myDummyCredVal;
    return getOptionValue(*theConfig, aServiceName, aUserId, ServicePassword, myDummyCredVal);
}

string ReseptClientAppTestConfig::getPassword(const string& aServiceName, const string& aUserId) const
{
    string myRetVal;
    if (!getOptionValue(*theConfig, aServiceName, aUserId, ServicePassword, myRetVal))
        TA_THROW_MSG (ReseptClientAppTestConfigError, boost::format("No pincode found for service %s, user %s in %s") % aServiceName % aUserId % ConfigFileName);
    return myRetVal;
}

void ReseptClientAppTestConfig::setPassword(const string& aServiceName, const string& aUserId, const string& aPassword) const
{
    setOptionValue(*theConfig, aServiceName, aUserId, ServicePassword, aPassword);
}

bool ReseptClientAppTestConfig::isNewPasswordExist(const string& aServiceName, const string& aUserId) const
{
    string myDummyCredVal;
    return getOptionValue(*theConfig, aServiceName, aUserId, ServiceNewPassword, myDummyCredVal);
}

string ReseptClientAppTestConfig::getNewPassword(const string& aServiceName, const string& aUserId) const
{
    string myRetVal;
    if (!getOptionValue(*theConfig, aServiceName, aUserId, ServiceNewPassword, myRetVal))
        TA_THROW_MSG (ReseptClientAppTestConfigError, boost::format("No new Password found for service %s, user %s in %s") % aServiceName % aUserId % ConfigFileName);
    return myRetVal;
}

void ReseptClientAppTestConfig::setNewPassword(const string& aServiceName, const string& aUserId, const string& aNewPassword) const
{
    setOptionValue(*theConfig, aServiceName, aUserId, ServiceNewPassword, aNewPassword);
}

bool ReseptClientAppTestConfig::isPincodeExist(const string& aServiceName, const string& aUserId) const
{
    string myDummyCredVal;
    return getOptionValue(*theConfig, aServiceName, aUserId, ServicePincode, myDummyCredVal);
}

string ReseptClientAppTestConfig::getPincode(const string& aServiceName, const string& aUserId) const
{
    string myRetVal;
    if (!getOptionValue(*theConfig, aServiceName, aUserId, ServicePincode, myRetVal))
        TA_THROW_MSG (ReseptClientAppTestConfigError, boost::format("No pincode found for service %s, user %s in %s") % aServiceName % aUserId % ConfigFileName);
    return myRetVal;
}

bool ReseptClientAppTestConfig::isCrFileRequired(const string& aServiceName, const string& aUserId) const
{
    bool myIsRequired = false;
    if (getOptionValue(*theConfig, aServiceName, aUserId, crfile_required, myIsRequired) && myIsRequired)
        return true;
    else
        return false;
}

