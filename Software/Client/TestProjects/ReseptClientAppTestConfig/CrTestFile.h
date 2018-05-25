/*
 * CrTestFile.h
 *
 *  Created on: 22 mei 2014
 *      Author: vereijkenj
 */

#ifndef CRTESTFILE_H_
#define CRTESTFILE_H_

#include "rclient/CRFile.h"
#include "resept/util.h"

#include "boost/assign/list_of.hpp"
#include <string>
#include <fstream>


class CrTestFile
{
public:
    enum ContentPoisoning
    {
        contentNotPoison,
        contentPoisonResponses,
        contentRemoveResponses
    };

    CrTestFile(const std::string& filename, ContentPoisoning aContentValidity = contentNotPoison):
        file( filename.c_str(), std::ios::trunc ),
        name(filename)
    {
        using std::string;

        file << rclient::crfile::challengesList << " = (" << std::endl;

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // UMTS user
         string user = resept::UmtsUserName;
        {
            ta::StringDict myChallenges = boost::assign::map_list_of (resept::UmtsAutnChallengeName   , resept::UmtsAutnChallengeValue)
                                                                      (resept::UmtsRandomChallengeName , resept::UmtsRandomChallengeValue);
            ta::StringDict myResponses = resept::calcUmtsResponses(user, myChallenges, resept::UmtsResponseNames);
            myResponses = poisonValidResponses(myResponses, aContentValidity);

            addUserSection(file, user, myChallenges, myResponses);
        }

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // Demo user
        {
            user = resept::DemoUserName;
            ta::StringDict myChallenges = boost::assign::map_list_of(resept::Challenge, resept::DemoUserChallenge);
            ta::StringDict myResponses = boost::assign::map_list_of(rclient::crfile::ResponseKey, resept::calcResponse(user, resept::DemoUserChallenge));
            myResponses = poisonValidResponses(myResponses, aContentValidity);

            file << "," << std::endl;
            addUserSection(file, user, myChallenges, myResponses);
        }

        string myToken;
        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // SecuridNewUserPinUser
        {
            user = resept::securid::NewUserPinUserName;
            myToken = resept::securid::NewUserPinInitialTokenCode;
            ta::StringDict myChallenges = boost::assign::map_list_of(resept::PasswordChallenge, resept::securid::NewUserPinInitialTokenRequest);
            ta::StringDict  myResponses = boost::assign::map_list_of(rclient::crfile::ResponseKey, resept::securid::NewUserPinNewPincode);
            myResponses = poisonValidResponses(myResponses, aContentValidity);

            file << "," << std::endl;
            addUserSectionwithToken(file, user, myToken, myChallenges, myResponses);
        }

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // SecuridNewUserPinUser
        {
            ta::StringDict myChallenges = boost::assign::map_list_of(resept::PasswordChallenge, resept::securid::NewUserPinReEnterRequest);
            ta::StringDict myResponses = boost::assign::map_list_of(rclient::crfile::ResponseKey, resept::securid::NewUserPinNewPincode);
            myResponses = poisonValidResponses(myResponses, aContentValidity);

            file << "," << std::endl;
            addUserSectionwithToken(file, user, myToken, myChallenges, myResponses);
        }

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // SecuridNewUserPinUser
        {
            ta::StringDict myChallenges = boost::assign::map_list_of(resept::PasswordChallenge, resept::securid::NewUserPinNewTokenRequest);
            ta::StringDict myResponses = boost::assign::map_list_of(rclient::crfile::ResponseKey, resept::securid::NewUserPinNewTokenCode);
            myResponses = poisonValidResponses(myResponses, aContentValidity);

            file << "," << std::endl;
            addUserSectionwithToken(file, user, myToken, myChallenges, myResponses);
        }

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // GSM_2_354162120787078
        {
            user = resept::GsmUserName;
            ta::StringDict myChallenges = boost::assign::map_list_of(resept::GsmRandomChallengeName, resept::GsmRandomChallenge1Value);
            ta::StringDict myResponses = resept::calcGsmResponses(user, myChallenges, resept::GsmResponseNames);
            myResponses = poisonValidResponses(myResponses, aContentValidity);

            file << "," << std::endl;
            addUserSection(file, user, myChallenges, myResponses);
        }

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // GSM_2_354162120787078 continue
        {
            ta::StringDict myChallenges = boost::assign::map_list_of(resept::GsmRandomChallengeName, resept::GsmRandomChallenge2Value);
            ta::StringDict myResponses = resept::calcGsmResponses(user, myChallenges, resept::GsmResponseNames);

            file << "," << std::endl;
            addUserSection(file, user, myChallenges, myResponses);
        }

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // GSM_2_354162120787078 continue
        {
            ta::StringDict myChallenges = boost::assign::map_list_of(resept::GsmRandomChallengeName, resept::GsmRandomChallenge3Value);
            ta::StringDict myResponses = resept::calcGsmResponses(user, myChallenges, resept::GsmResponseNames);
            myResponses = poisonValidResponses(myResponses, aContentValidity);

            file << "," << std::endl;
            addUserSection(file, user, myChallenges, myResponses);
        }

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // SecuridNewSystemPinUser
        {
            user = resept::securid::NewSystemPinUserName;
            myToken = resept::securid::NewSystemPinInitialTokenCode;
            ta::StringDict myChallenges = boost::assign::map_list_of(resept::PasswordChallenge, resept::securid::NewSystemPinNewRequestConfirm);
            ta::StringDict myResponses = boost::assign::map_list_of(rclient::crfile::ResponseKey, resept::securid::YesResponse);
            myResponses = poisonValidResponses(myResponses, aContentValidity);

            file << "," << std::endl;
            addUserSectionwithToken(file, user, myToken, myChallenges, myResponses);
        }

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // SecuridNewSystemPinUser
        {
            ta::StringDict myChallenges = boost::assign::map_list_of(resept::PasswordChallenge, resept::securid::NewSystemPinConfirm);
            ta::StringDict myResponses = boost::assign::map_list_of(rclient::crfile::ResponseKey, resept::securid::YesResponse);
            myResponses = poisonValidResponses(myResponses, aContentValidity);

            file << "," << std::endl;
            addUserSectionwithToken(file, user, myToken, myChallenges, myResponses);
        }

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // SecuridNewSystemPinUser
        {
            ta::StringDict myChallenges = boost::assign::map_list_of(resept::PasswordChallenge, resept::securid::NewSystemPinNewTokenRequest);
            ta::StringDict myResponses = boost::assign::map_list_of(rclient::crfile::ResponseKey, resept::securid::NewSystemPinNewTokenCode);
            myResponses = poisonValidResponses(myResponses, aContentValidity);

            file << "," << std::endl;
            addUserSectionwithToken(file, user, myToken, myChallenges, myResponses);
        }

        ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
        // SecuridNextTokenUser
        {
            user = resept::securid::NextTokenUserName;
            myToken = resept::securid::NextTokenInitialTokenCode;
            ta::StringDict myChallenges = boost::assign::map_list_of(resept::PasswordChallenge, resept::securid::NextTokenNewTokenRequest);
            ta::StringDict myResponses = boost::assign::map_list_of(rclient::crfile::ResponseKey, resept::securid::NextTokenNewTokenCode);
            myResponses = poisonValidResponses(myResponses, aContentValidity);

            file << "," << std::endl;
            addUserSectionwithToken(file, user, myToken, myChallenges, myResponses);
        }

        // no more user sections so no "," needed
        file << std::endl;

        file << ");" << std::endl;

        file.close();
    }

    inline std::string filename() const
    {
        return name;
    }

    ~CrTestFile()
    {
        if (file.is_open()) file.close();
        remove(name.c_str());
    }

private:
    std::ofstream file;
    std::string name;

    ta::StringDict poisonValidResponses(const ta::StringDict& aValidResponses, ContentPoisoning aContentValidity)
    {
        switch (aContentValidity)
        {
        case contentNotPoison:
            return aValidResponses;
        case contentPoisonResponses:
        {
            ta::StringDict myPoisonedResponses = aValidResponses;
            foreach (ta::StringDict::value_type& chall_resp, myPoisonedResponses)
            {
                chall_resp.second += ".invalid";
            }
            return myPoisonedResponses;
        }
        case contentRemoveResponses:
            return ta::StringDict();
        default:
            TA_THROW_MSG(std::invalid_argument, boost::format("Unsupported content validity %d") % aContentValidity);
        }
    }

    void addUserSection(std::ofstream& refFile, const std::string user, const ta::StringDict aChallenges, const ta::StringDict aResponses)
    {
        refFile << "{" << std::endl;
        addNameUser(refFile, user);
        addChallenges(refFile, aChallenges);
        addResponses (refFile, aResponses);
        refFile << "}";
    }

    void addUserSectionwithToken(std::ofstream& refFile, const std::string user, const std::string token, const ta::StringDict aChallenges, const ta::StringDict aResponses)
    {
        refFile << "{" << std::endl;
        addNameUser(refFile, user);
        addInitialToken(refFile, token);
        addChallenges(refFile, aChallenges);
        addResponses (refFile, aResponses);
        refFile << "}";
    }

    void addNameUser(std::ofstream& refFile, const std::string user)
    {
        refFile << "\t" << rclient::crfile::UserKey <<" =  \"" << user << "\";" << std::endl;
    }

    void addInitialToken(std::ofstream& refFile, const std::string token)
    {
        refFile << "\t" << rclient::crfile::InitialTokenKey <<" =  \"" << token << "\";" << std::endl;
    }

    void addChallenges(std::ofstream& refFile, const ta::StringDict aMap)
    {
        refFile << "\t"<< rclient::crfile::challengeList << " = (" << std::endl;
        addNameValue(refFile, aMap);
        refFile << "\t"<< ");" << std::endl;
    }

    void addResponses(std::ofstream& refFile, const ta::StringDict aMap)
    {
        refFile << "\t"<< rclient::crfile::ResponseKey << " = (" << std::endl;
        addNameValue(refFile, aMap);
        refFile << "\t"<< ");" << std::endl;
    }

    void addNameValue(std::ofstream& refFile, const ta::StringDict aMap)
    {
        bool first = true;
        foreach (const ta::StringDict::value_type& key, aMap)
        {
            if (first)
            {
                first = false;
            }
            else
            {
                refFile << ", " << std::endl;
            }
            refFile << "\t" << "{" << std::endl;
            refFile << "\t\t" << rclient::crfile::keyname  << "  = \"" << key.first << "\";" << std::endl;
            refFile << "\t\t" << rclient::crfile::keyvalue << "  = \"" << key.second  <<"\";" << std::endl;
            refFile << "\t" << "}";
        }
        refFile << std::endl;
    }
};


#endif /* CRTESTFILE_H_ */
