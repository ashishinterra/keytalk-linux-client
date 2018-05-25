#pragma once

#include <stdexcept>
#include <string>

namespace libconfig { class Config; }

//
// Configuration file containing test configuration such as valid and invalid credentials to test client against the server
//
struct ReseptClientAppTestConfigError : std::logic_error
{
    explicit ReseptClientAppTestConfigError(const std::string& aMessage = "") : std::logic_error(aMessage) {}
};

class ReseptClientAppTestConfig
{
public:
    ReseptClientAppTestConfig();
    ~ReseptClientAppTestConfig();

    bool isServiceExist(const std::string& aServiceName) const;
    bool isUserExist(const std::string& aServiceName, const std::string& aUserId) const;
    bool isUserLocked(const std::string& aServiceName, const std::string& aUserId) const;

    bool isPasswordExist(const std::string& aServiceName, const std::string& aUserId) const;
    std::string getPassword(const std::string& aServiceName, const std::string& aUserId) const;
    void setPassword(const std::string& aServiceName, const std::string& aUserId, const std::string& aPassword) const;

    bool isNewPasswordExist(const std::string& aServiceName, const std::string& aUserId) const;
    std::string getNewPassword(const std::string& aServiceName, const std::string& aUserId) const;
    void setNewPassword(const std::string& aServiceName, const std::string& aUserId, const std::string& aNewPassword) const;

    bool isPincodeExist(const std::string& aServiceName, const std::string& aUserId) const;
    std::string getPincode(const std::string& aServiceName, const std::string& aUserId) const;

    bool isCrFileRequired(const std::string& aServiceName, const std::string& aUserId) const;
private:
    bool getCredential(const std::string& aServiceName, const std::string& aUserId, const std::string& aCredName, std::string& aCredVal) const;
    libconfig::Config* theConfig;
};
