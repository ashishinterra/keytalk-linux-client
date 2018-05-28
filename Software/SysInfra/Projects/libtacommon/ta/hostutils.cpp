#ifndef _WIN32
#include "ta/hostutils.h"
#include "ta/netutils.h"
#include "ta/process.h"
#include "ta/utils.h"
#include "ta/common.h"

#include <unistd.h>
#include <errno.h>

namespace ta
{
    namespace HostUtils
    {
        using std::string;

        namespace hostname
        {
            string get()
            {
                char myHostName[HOST_NAME_MAX+1] = {};
                if (gethostname(myHostName, sizeof(myHostName)) != 0)
                {
                    TA_THROW_MSG(std::runtime_error, boost::format("Failed to retrieve hostname. %s")  % strerror(errno));
                }
                return ta::NetUtils::normalizeDomainName(myHostName);
            }

            void set(const string& aHostName)
            {
                // validate
                const string myNormalizedHostName = ta::NetUtils::normalizeDomainName(aHostName);
                ta::NetUtils::DomainNameValidationResult validationResult;
                if (!ta::NetUtils::isValidDomainName(myNormalizedHostName, validationResult, ta::NetUtils::hostName))
                {
                    throw ta::NetUtils::DomainNameValidationError(validationResult, aHostName);
                }

                // effectuate
                ta::Process::checkedShellExecSync("sudo hostname " + myNormalizedHostName);
                //@note we use "sudo hostname" iso sethostname(2) to make it easier to deal with permissions)

                // make the changes persistent
                ta::writeData("/etc/hostname", myNormalizedHostName + "\n");
            }


        } // namespace hostname

    }
}
#endif
