#ifndef RESEPTCONSOLECLIENTDELEGATES_H
#define RESEPTCONSOLECLIENTDELEGATES_H

#include "rclient/ReseptClientApp.h"
#include <string>
#include <vector>

//
// Implementation of console client delegates
//

std::string onPasswordPrompt(const rclient::ReseptClientApp::StringMap& aChallenges, const std::string& aUserId, void* aCookie);
std::string onPincodePrompt(const std::string& aUserId, void* aCookie);
rclient::ReseptClientApp::StringMap onResponsePrompt(const rclient::ReseptClientApp::StringMap& aChallenges, const std::vector<std::string>& aResponseNames, const std::string& aUserId, void* aCookie);
bool onChangePasswordPrompt(const std::string& aMsg, const std::string& aUserId, bool aReasonPasswordExpired, std::string& aNewPassword, void* aCookie);
void onUserMessages(const std::vector<rclient::ReseptClientApp::UserMessage>& aMessages, void* aCookie);
void onAuthenticationDelayed(size_t aDelaySecs, void* aCookie);
void onAuthenticationUserLocked(void* aCookie);
void onSavePfx(const std::vector<unsigned char>& aPfx, const std::string& aPassword, void* aCookie);
void onSavePem(const std::vector<unsigned char>& aCert, const std::string& aPassword, void* aCookie);
void onNotify(const std::string& aMsg, void* aCookie);
void onError(const std::string& anUserErrorMsg, void* aCookie);

#endif
