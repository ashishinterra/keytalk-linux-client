#pragma once

#include "ta/common.h"

#include <string>
#include "boost/serialization/access.hpp"

namespace ta
{
    namespace version
    {
        class Version
        {
        public:
            // PRE: major and minor cannot be both 0
            // valid devstages are (in the sorting order): a1,a2,...,b1,b2,...,<empty>,p1,p2,...
            Version(const int aMajor = 1, const int aMinor = 0, const int aSubMinor = 0, const std::string& aDevStage = "");

            bool operator==(const Version& other) const;
            bool operator!=(const Version& other) const;
            bool operator<(const Version& other) const;
            bool operator<=(const Version& other) const;
            bool operator>(const Version& other) const;
            bool operator>=(const Version& other) const;

            inline int major() const { return theMajor; }
            inline int minor() const { return theMinor; }
            inline int subminor() const { return theSubMinor; }
            inline std::string devstage() const { return theDevStage; }

            friend class boost::serialization::access;
            template<class Archive> void serialize(Archive& ar, const unsigned int UNUSED(version))
            {
                ar & theMajor;
                ar & theMinor;
                ar & theSubMinor;
                ar & theDevStage;
            }

        private:
            int theMajor;
            int theMinor;
            int theSubMinor;
            std::string theDevStage;
        };


        /**
          Creates version from string representation as "major.minor[.subminor[.devstage]]
          major and minor cannot be simultaneously 0.
          subminor defaults to 0.
          devstage defaults to empty string.
         */
        Version parse(const std::string& anStr);

        enum Format
        {
            fmtMajorMinor, // "5.2"
            fmtMajorMinorSubminor, // "5.2.1"
            fmtMajorMinorSubminorDevstage // "5.2.1.b3"
        };
        std::string toStr(const Version& aVersion, const Format aFormat = fmtMajorMinorSubminorDevstage);
    }
}

