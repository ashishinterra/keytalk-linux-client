#pragma once

#include "ta/common.h"
#include <string>
#include <vector>
#include <stdexcept>

namespace rclient
{
    struct NativeCertStoreError : std::runtime_error
    {
        explicit NativeCertStoreError(const std::string& aMessage = "") : std::runtime_error(aMessage) {}
    };

    struct NativeCertStoreDeleteError: NativeCertStoreError
    {
        explicit NativeCertStoreDeleteError(const std::string& aMessage = "") : NativeCertStoreError(aMessage) {}
    };

    struct NativeCertStoreValidateError: NativeCertStoreError
    {
        explicit NativeCertStoreValidateError(const std::string& aMessage = "") : NativeCertStoreError(aMessage) {}
    };

    struct NativeCertStoreImportError: NativeCertStoreError
    {
        explicit NativeCertStoreImportError(const std::string& aMessage = "") : NativeCertStoreError(aMessage) {}
    };


    struct Pfx;

    namespace NativeCertStore
    {
        enum ErrorPolicy { proceedOnError, failOnError};

        /**
           Check the existence of valid certificates issued with the current KeyTalk service

           Certificate validity is determined by the 'CertValidPercent' value associated with the latest service
           and, of course, the validity of the certificate itself

           @return The number of valid certificates found
           @throw NativeCertStoreValidateError
        */
        unsigned int validateReseptUserCert();

        /**
          Delete certificates form the personal store associated with the current KeyTalk service
          @return  a number of removed certificates
          @throw NativeCertStoreDeleteError
        */
        unsigned int deleteAllReseptUserCerts();

        /**
          Delete invalid certificates form the personal store associated with the current KeyTalk service

          Certificate validity is determined by the 'CertValidPercent' value associated with the current service
          and, of course, the validity of the certificate itself

          @return  a number of removed certificates
          @throw NativeCertStoreDeleteError
        */
        unsigned int deleteInvalidReseptUserCerts();

        /**
          Delete non-CA certificates from the personal store issued by the given CN, tolerating when cert deletion fails.
          @return  a number of removed certificates
          @throw NativeCertStoreDeleteError
        */
        unsigned int deleteUserCertsForIssuerCN(const std::string& anIssuerCn, ErrorPolicy anErrorPolicy);

        /**
           Imports PKCS#12 PFX into the personal store.

           @param aPfx The PKCS#12 package containing certificate along with private key to be imported
           @post on success the function updates the configuration of the current service in the user configuration with a fingerprint of the imported cert
           @return SHA-1 fingerprint (lowercase hex) of the imported certificate
           @throw NativeCertStoreImportError
        */
        std::string importPfx(const Pfx& aPfx);


        /**
           Installs the given CAs certificate into the appropriate trusted stores and update KeyTalk app settings accordingly
           @param aUcaPath, anScaPath, aPcaPath, anRcaPath location of DER-encoded certificates user-, server-, primary and root KeyTalk CAs respectively
           @param anExtraSigningCAaPemPaths extra Signing PEM CAs ordered from child towards parent
           @note When anRcaDerPath is not empty, RCA cert is root CA otherwise PCA is root CA
           @note This function likely elevated system privileges and is normally called by an installer
           @throw NativeCertStoreError
        */
        void installCAs(const std::string& aUcaDerPath,
                        const std::string& anScaDerPath,
                        const std::string& aPcaDerPath,
                        const std::string& anRcaDerPath,
                        const ta::StringArray& anExtraSigningCAsPemPaths);

        // retrieve the list of all installed KeyTalk CAs in PEM format
        void getInstalledCAs(ta::StringArray& aUCAs,
                             ta::StringArray& anSCAs,
                             ta::StringArray& aPCAs,
                             ta::StringArray& anRCAs,
                             ta::StringArray& anExtraSigningCAs);

        /**
          Delete certificates from the intermediate or trusted root store for the given criteria and error policy.
          @return  a number of deleted certificates
          @note This function likely elevated system privileges and is normally called by an installer
          @throw NativeCertStoreDeleteError
        */
        unsigned int deleteFromRootStoreByCN(const std::string& aSubjCN, ErrorPolicy anErrorPolicy);
        unsigned int deleteFromIntermediateStoreByCN(const std::string& aSubjCN, ErrorPolicy anErrorPolicy);
        unsigned int deleteFromRootStoreByFingerprint(const std::string& aSha1Fingerprint, ErrorPolicy anErrorPolicy);
        unsigned int deleteFromIntermediateStoreByFingerprint(const std::string& aSha1Fingerprint, ErrorPolicy anErrorPolicy);

#ifdef _WIN32
        /**
         @return The list of all windows store names for root store "LocalMachine"
        */
        ta::StringArray getStoreNames();

        /**
         @return true if the specified store name exists in root store "LocalMachine", false otherwise
        */
        bool isStoreExists(const std::string& aStoreName);
#endif
    }
}
