#include "Common.h"
#include "Settings.h"
#include "ta/process.h"
#include "ta/strings.h"
#include "ta/timeutils.h"
#include "ta/utils.h"
#include "ta/common.h"

#include <string>
#include "boost/filesystem/operations.hpp"
#include "boost/filesystem/convenience.hpp"

using std::string;

namespace rclient
{
    string getLogDir()
    {
        const string myLogDir = Settings::getUserConfigDir();
        if (!ta::isDirExist(myLogDir))
        {
            try {
                boost::filesystem::create_directories(myLogDir);
            } catch (std::exception& e) {
                TA_THROW_MSG(std::runtime_error, boost::format("Failed to create directory '%s' for log file. %s") % myLogDir % e.what());
            }
        }
        return myLogDir;
    }

    std::string getInstallerDataBackupDir()
    {
        return str(boost::format("%sReseptInstallerDataBackup") % ta::Process::getTempDir());
    }

    string formatMessages(const Messages& aMessages)
    {
        ta::StringArray myMsgs;
        foreach (const Message& msg, aMessages)
        {
            myMsgs.push_back(str(boost::format("[%s] %s") % ta::TimeUtils::timestampToIso8601(msg.utc) % msg.text));
        }
        return ta::Strings::join(myMsgs, ", ");
    }

}
