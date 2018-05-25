#include "resept/computeruuid.h"
#include "ta/strings.h"
#include "ta/process.h"
#include "ta/logger.h"
#include "ta/logconfiguration.h"
#include "ta/common.h"

#include <iostream>
#include <stdexcept>
#include <map>

using namespace resept::ComputerUuid;

static void dumpHwsig(const Components::id aCompId)
{
    const std::string myCompName = Components::str(aCompId);
    const std::string myFormula = ta::Strings::toString(aCompId);
    std::map<Components::id, std::string> myHwIds;

    const std::string myHwSig = calcCs(myFormula, NULL, NULL, &myHwIds);
    std::string myHwId;
    if (!ta::findValueByKey(aCompId, myHwIds, myHwId))
    {
        std::cerr << "WARNING: failed to calculate HWID of " << myCompName << " (" << aCompId << std::endl;
        std::cout << myCompName << " (ID " << aCompId << "): HWSIG -> " << myHwSig << std::endl;
    }
    else
    {
        std::cout << myCompName << " (ID " << aCompId << "): HWSIG -> " << myHwSig << ", HWID -> " << myHwId << std::endl;
    }
}

static void initLogger()
{
    const std::string myLogFilePath = ta::Process::getTempDir() + ta::Process::getSelfShortName() + ".log";
    ta::LogConfiguration::Config myMemConfig;
    myMemConfig.fileAppender = true;
    myMemConfig.fileAppenderLogThreshold = ta::LogLevel::Debug;
    myMemConfig.fileAppenderLogFileName = myLogFilePath;
    ta::LogConfiguration::instance().load(myMemConfig);
}

int main(int UNUSED(argc), char* UNUSED(argv)[])
{

    try
    {
        initLogger();
        dumpHwsig(Components::Predefined);
        for (int compid = Components::First; compid < Components::Last; ++compid)
        {
            dumpHwsig(static_cast<Components::id>(compid));
        }
        return 0;
    }
    catch (std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << std::endl;
    }
    catch (...)
    {
        std::cerr << "ERROR: unexpected error occurred" << std::endl;
    }

}

