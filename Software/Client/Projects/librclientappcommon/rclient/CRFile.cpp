#include "CRFile.h"

using std::string;

namespace rclient
{
    string fmtFilter(const ta::StringDict& aFilter)
    {
        string myFilterStr;
        foreach (const ta::StringDict::value_type& kvp, aFilter)
        {
            if (!myFilterStr.empty())
            {
                myFilterStr += ", ";
            }
            myFilterStr += str(boost::format("%s=%s") % kvp.first % kvp.second);
        }
        return myFilterStr;
    }

    CRFile::CRFile(const string& aFileName) :
        theConfig(ta::LibConfigWrapper(aFileName))
    {}

    CRFile::~CRFile()
    {}

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static string getChallengesPath(unsigned int aUserIdx, const string& aSettingName)
    {
        return str(boost::format("%s.[%u].%s")  % crfile::challengesList % aUserIdx % aSettingName);
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////

    static string getChallengeNamePath(unsigned int aUserIdx, unsigned int aChallengeIdx)
    {
        return str(boost::format("%s.[%u].%s.[%u].%s")  % crfile::challengesList % aUserIdx % crfile::challengeList % aChallengeIdx % crfile::keyname);
    }

    static string getChallengeValuePath(unsigned int aUserIdx, unsigned int aChallengeIdx)
    {
        return str(boost::format("%s.[%u].%s.[%u].%s")  % crfile::challengesList % aUserIdx % crfile::challengeList % aChallengeIdx % crfile::keyvalue);
    }


    //////////////////////////////////////////////////////////////////////////////////////////////////////////////

    static string getResponseNamePath(unsigned int aUserIdx, unsigned int aChallengeIdx)
    {
        return str(boost::format("%s.[%u].%s.[%u].%s")  % crfile::challengesList % aUserIdx % crfile::responseList % aChallengeIdx % crfile::keyname);
    }

    static string getResponseValuePath(unsigned int aUserIdx, unsigned int aChallengeIdx)
    {
        return str(boost::format("%s.[%u].%s.[%u].%s")  % crfile::challengesList % aUserIdx % crfile::responseList % aChallengeIdx % crfile::keyvalue);
    }

    static string getUserChallengeList(unsigned int aUserIdx)
    {
        return str(boost::format("%s.[%u].%s")  % crfile::challengesList % aUserIdx % crfile::challengeList);
    }

    static string getUserResponseList(unsigned int aUserIdx)
    {
        return str(boost::format("%s.[%u].%s")  % crfile::challengesList % aUserIdx % crfile::responseList);
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::string CRFile::getResponse(const string& aKey, const string& aUser, const ta::StringDict& aFilter) const
    {
        size_t myNumChallenges = 0 ;
        theConfig.getListInfo(crfile::challengesList, myNumChallenges);

        for (size_t iChallenge=0; iChallenge < myNumChallenges; ++iChallenge)
        {
            string myUser;
            if ( !theConfig.getValue(getChallengesPath(iChallenge, crfile::UserKey), myUser, ta::LibConfigWrapper::settingGetTolerateIfNotExist) )
            {
                continue;
            }

            if(myUser != aUser)
            {
                continue;
            }

            size_t myNumUserChallenges = 0 ;
            theConfig.getListInfo(getUserChallengeList(iChallenge), myNumUserChallenges);

            bool found = false;

            // all supplied key-value pairs must match
            foreach (const ta::StringDict::value_type& kvp, aFilter)
            {
                found = false; // reset every pair check
                for (size_t iUserChallenge=0; iUserChallenge < myNumUserChallenges; ++iUserChallenge)
                {
                    string myName;
                    string myValue;
                    if( theConfig.getValue(getChallengeNamePath(iChallenge, iUserChallenge), myName, ta::LibConfigWrapper::settingGetTolerateIfNotExist)   &&
                            theConfig.getValue(getChallengeValuePath(iChallenge, iUserChallenge), myValue, ta::LibConfigWrapper::settingGetTolerateIfNotExist)  )
                    {
                        if ( (myName== kvp.first) && (myValue==kvp.second) )
                            found = true;
                    }
                }

                if ( !found)
                {
                    // pair not in current location, try next
                    break;
                }

            } // foreach

            if (!found)
            {
                // not all pairs match
                continue;
            }

            // all search info matches
            // check the current response set for existence of the response name
            size_t myNumUserRespones = 0 ;
            theConfig.getListInfo(getUserResponseList(iChallenge), myNumUserRespones);

            for (size_t iResponse=0; iResponse < myNumUserRespones; ++iResponse)
            {
                string myName;
                if( theConfig.getValue(getResponseNamePath(iChallenge, iResponse), myName, ta::LibConfigWrapper::settingGetTolerateIfNotExist) )
                {
                    if (myName == aKey)
                    {
                        if( theConfig.getValue(getResponseValuePath(iChallenge, iResponse), myName, ta::LibConfigWrapper::settingGetTolerateIfNotExist) )
                        {
                            return myName;
                        }
                    }
                }
            }
        }

        TA_THROW_MSG(std::runtime_error, boost::format("No Challenge response \"%s\" info found for user \"%s\" with filter \"%s\" in CR file %s") % aKey % aUser % fmtFilter(aFilter) % theConfig.getConfigFilePath());
    }

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////

    std::string CRFile::getKey(const string& aKeyName, const ta::StringDict& aFilter) const
    {
        size_t myNumChallenges = 0 ;
        theConfig.getListInfo(crfile::challengesList, myNumChallenges);

        for (size_t iChallenge=0; iChallenge < myNumChallenges; ++iChallenge)
        {
            bool   valid = true;

            // all supplied key-value pairs must match
            foreach (const ta::StringDict::value_type& kvp, aFilter)
            {
                string keyvalue;
                if ( theConfig.getValue(getChallengesPath(iChallenge, kvp.first), keyvalue, ta::LibConfigWrapper::settingGetTolerateIfNotExist) )
                {
                    if ( kvp.second != keyvalue )
                    {
                        // not matching try next challenge blok from the file (next iChallenge)
                        valid = false;
                        break;
                    }

                }
            } //foreach

            if( valid ) // this means all key-value pairs match.
            {
                string myRequestedKey;
                if ( theConfig.getValue(getChallengesPath(iChallenge, aKeyName), myRequestedKey, ta::LibConfigWrapper::settingGetTolerateIfNotExist) )
                {
                    return myRequestedKey;
                }
                // if function did not return the requested key was not available in the found section
                // continue searching
            }
        }

        TA_THROW_MSG(std::runtime_error, boost::format("No Challenge response \"%s\" info found with filter \"%s\"  in CR file %s") % aKeyName % fmtFilter(aFilter) % theConfig.getConfigFilePath());

    }

}
