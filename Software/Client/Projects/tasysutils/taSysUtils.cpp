#include "ta/certutils.h"
#include "ta/signutils.h"
#include "ta/opensslapp.h"
#include "ta/utils.h"
#include "ta/process.h"
#include "ta/hashutils.h"
#include "ta/common.h"
#include "boost/cstdint.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

using namespace std;
using namespace ta;

#define SIGN_ARG     "--sign"
#define SIGNKEY_ARG  "--signkey"

#define ERROR_MSG "error"

static ta::OpenSSLApp theOpenSSLApp;

// static void printUsage()
// {
//     cout << "Usage:\n";
//     cout << "       taSysUtils " << SIGNKEY_ARG << " <PemCert> <PemSignKey> <SignKeyPass> <OutSignedKey> " << " SMINE-sign the pubkey extracted from the given certificate with the provided signing key\n";
//     cout << "       taSysUtils " << SIGN_ARG    << " <InFile> <PemSignKey> <SignKeyPass> <OutFile> " << " SMIME-sign the input from with the provided signing key\n";
// }


int main(int argc, char* argv[])
{
    vector<string> myArgs;
    myArgs.assign(&argv[1], &argv[1] + argc-1);

    if (myArgs.size() == 5)
    {
        if (find(myArgs.begin(), myArgs.end(), SIGNKEY_ARG) != myArgs.end())
        {
            try
            {
                const string myPemCertPath = myArgs[1];
                const string myPemSigningKeyPath = myArgs[2];
                const string mySignedKeyPass = myArgs[3];
                const string mySignedKeyOutPath = myArgs[4];

                vector<unsigned char> myPubKeyBuf = ta::CertUtils::extractPemPubKeyFile(myPemCertPath);
                const string myPubKeyFilePath = str(boost::format("%spubkey_%d.tmp") % ta::Process::getTempDir() % (int)time(NULL));
                ta::writeData(myPubKeyFilePath, myPubKeyBuf);
                try
                {
                    ta::SignUtils::signPKCS7(myPubKeyFilePath, mySignedKeyOutPath, mySignedKeyPass, myPemSigningKeyPath);
                    remove(myPubKeyFilePath.c_str());
                    return 0;
                }
                catch (std::exception&)
                {
                    remove(myPubKeyFilePath.c_str());
                    throw;
                }
            }
            catch (std::exception& e)
            {
                std::cout << e.what() << "\n";
                return 1;
            }
        }
        if (find(myArgs.begin(), myArgs.end(), SIGN_ARG) != myArgs.end())
        {
            try
            {
                const string myInFilePath = myArgs[1];
                const string myPemSigningKeyPath = myArgs[2];
                const string mySignedKeyPass = myArgs[3];
                const string myOutFilePath = myArgs[4];

                ta::SignUtils::signPKCS7(myInFilePath, myOutFilePath, mySignedKeyPass, myPemSigningKeyPath);
                return 0;
            }
            catch (std::exception& e)
            {
                std::cout << e.what() << "\n";
                return 1;
            }
        }
    }
    cout << ERROR_MSG;
    return 1;
}
