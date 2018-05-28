#include "version.h"
#include "strings.h"
#include "common.h"

#include "boost/regex.hpp"
#include <stdexcept>

using std::string;

namespace ta
{
    namespace version
    {
        struct VersionParseError : std::invalid_argument
        {
            explicit VersionParseError(const string& aMessage = "") : std::invalid_argument(aMessage) {}
        };

        string parseDevStage(const string& anStr)
        {
            string myDevStage = boost::trim_copy(anStr);
            if (!myDevStage.empty())
            {
                boost::regex myRegEx("(?<letter>[abp])(?<num>\\d+)");
                boost::cmatch myMatch;
                if (!regex_match(myDevStage.c_str(), myMatch, myRegEx))
                {
                    TA_THROW_MSG(VersionParseError, boost::format("Cannot parse devstage from '%s'") % myDevStage);
                }
                const string myDevStageLetter = myMatch["letter"];
                // normalize by removing trailing zeroes e.g. "p001" -> "p1"
                const int myDevStageNum = Strings::parse<int>(myMatch["num"]);
                myDevStage = str(boost::format("%s%d") % myDevStageLetter % myDevStageNum);
            }
            return myDevStage;
        }



        Version::Version(const int aMajor, const int aMinor, const int aSubMinor, const string& aDevStage)
            : theMajor(aMajor)
            , theMinor(aMinor)
            , theSubMinor(aSubMinor)
            , theDevStage(parseDevStage(aDevStage))
        {
            if (theMajor < 0 || theMinor < 0 || theSubMinor < 0)
            {
                TA_THROW_MSG(VersionParseError, "Version numeric components cannot be negative");
            }
            if (theMajor == 0 && theMinor == 0)
            {
                TA_THROW_MSG(VersionParseError, "Major and minor parts of the version cannot be both 0");
            }
        }

        bool Version::operator==(const Version& other) const
        {
            return (theMajor == other.theMajor &&
                    theMinor == other.theMinor &&
                    theSubMinor == other.theSubMinor &&
                    theDevStage == other.theDevStage);
        }
        bool Version::operator!=(const Version& other) const
        {
            return !(*this==other);
        }
        bool Version::operator<(const Version& other) const
        {
            if (theMajor < other.theMajor)
                return true;
            else if (theMajor > other.theMajor)
                return false;

            // majors equal

            if (theMinor < other.theMinor)
                return true;
            else if (theMinor > other.theMinor)
                return false;

            // minors equal

            if (theSubMinor < other.theSubMinor)
                return true;
            else if (theSubMinor > other.theSubMinor)
                return false;

            // subminors equal

            // compare devstages
            // use bogus 'p0' for empty devstage to easier sorting
            const string devstage = theDevStage.empty() ? "p0" : theDevStage;
            const string other_devstage = other.theDevStage.empty() ? "p0" : other.theDevStage;

            return devstage < other_devstage;
        }
        bool Version::operator<=(const Version& other) const
        {
            return ((*this) < other) || ((*this) == other);
        }
        bool Version::operator>(const Version& other) const
        {
            return other < (*this);
        }
        bool Version::operator>=(const Version& other) const
        {
            return other <= (*this);
        }


        Version parse(const string& anStr)
        {
            try
            {
                const std::vector<string> myParts = Strings::split(boost::trim_copy(anStr), '.');
                if (myParts.size() < 2 || myParts.size() > 4)
                {
                    TA_THROW_MSG(VersionParseError, boost::format("Cannot parse version from '%s'") % anStr);
                }

                const int myMajor = Strings::parse<int>(myParts[0]);
                const int myMinor = Strings::parse<int>(myParts[1]);
                if (myParts.size() == 2)
                {
                    return Version(myMajor, myMinor);
                }

                const int mySubMinor = Strings::parse<int>(myParts[2]);
                if (myParts.size() == 3)
                {
                    return Version(myMajor, myMinor, mySubMinor);
                }

                const string myDevStage = myParts[3];
                if (myDevStage.empty())
                {
                    TA_THROW_MSG(VersionParseError, boost::format("Cannot parse version from '%s'") % anStr);
                }
                return Version(myMajor, myMinor, mySubMinor, myDevStage);
            }
            catch (const VersionParseError&)
            {
                throw;
            }
            catch (const std::exception& e)
            {
                TA_THROW_MSG(VersionParseError, boost::format("Cannot parse version from '%1%'. %2%") % anStr % e.what());
            }
        }

        string toStr(const Version& aVersion, const Format aFormat)
        {
            switch (aFormat)
            {
            case fmtMajorMinor:
            {
                return str(boost::format("%d.%d") % aVersion.major() % aVersion.minor());
            }
            case fmtMajorMinorSubminor:
            {
                return str(boost::format("%d.%d.%d") % aVersion.major() % aVersion.minor() % aVersion.subminor());
            }
            case fmtMajorMinorSubminorDevstage:
            {
                if (aVersion.devstage().empty())
                {
                    return str(boost::format("%d.%d.%d") % aVersion.major() % aVersion.minor() % aVersion.subminor());
                }
                else
                {
                    return str(boost::format("%d.%d.%d.%s") % aVersion.major() % aVersion.minor() % aVersion.subminor() % aVersion.devstage());
                }
            }
            default:
            {
                TA_THROW_MSG(std::invalid_argument, boost::format("Unsupported version format %d") % aFormat);
            }
            }
        }

    } // version
} // ta
