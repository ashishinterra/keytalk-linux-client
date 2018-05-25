param (
    [parameter(mandatory)][string]$taskName,
    [switch]$skipCheck = $FALSE
)

Set-StrictMode -Version 3.0
################################################################################
# IIS HTTPS binding certificate update script
################################################################################
# This script uses the KeyTalk client to obtain a new certificate and assigns it
# to your IIS HTTPS binding.
#
# By default this script runs every minute, invoked by the KeyTalk scheduled taskName
#
# When running  this script in isolation ensure the following:
# 1. The KeyTalk service configuration has serverAuth (OID 1.3.6.1.5.5.7.3.1) enabled
# 2. If the KeyTalk service definition contains an Subject Alternative name,
#    it is equal to your IIS host name
# 3. If the KeyTalk service definition does not contain an Subject Alternative name,
#    the Common Name matches with your IIS host name
# 4. The server running IIS
#     - installed the KeyTalk client
#     - installed the RCCD containing required services and CA
#     - configured an IIS HTTPS binding certificate update task
#
# For advanced usage (e.g. network) it is also possible to customize below functions:
# 1. whether a certificate renewal is needed (IsCertRenewalNeeded)
# 2. how a certificate is retrieved (see: RetrieveKeyTalkCertificate)
# 3. how to apply the obtained certificate to your system (see: ApplyRetrievedCertificate)

################################################################################
# Global initialization
################################################################################
import-module -name $PSScriptRoot\KeyTalkUtils
import-module -name WebAdministration

InitLoggingFacility

# Pre-initialization so log files can be written even in case configuration fails
[string]$scriptLogFilePath      = "${env:TEMP}\keytalk_task_${taskName}.log"
[bool]$emailReporting           = $FALSE # Without configuration e-mail reporting is not possible

################################################################################
# Main
################################################################################
Function Main() {
    try {
        LogVerbose "Executing IIS certificate update Task '$taskName'"

        LoadBasicConfiguration # Minimal configuration to enable error reporting

        [bool]$taskEnabled = StringToBool (Invoke-ConfigToolChecked @( 'task', 'getparam', $taskName, 'Enabled' ))
        if (-not $taskEnabled) {
            ExitWithReport $SUCCESS "Skipping execution of disabled task '$taskName'" -suppressMail
        }

        PreValidateConfiguration
        LoadTaskConfiguration
        PostValidateConfiguration

        if (-not (Test-Administrator)) {
            ExitWithReport $ERR_ERROR "No administrative rights. This script requires administrative rights"
        }

        if (-not $skipCheck)
        {
            LogVerbose 'Determining if certificate needs replacement'
            $renewalNeeded = IsCertRenewalNeeded
            if (-not $renewalNeeded) {
                if ($sendEmailIfApplyNotRequired) {
                    ExitWithReport $SUCCESS "Nothing to be done. No new certificate needs to be applied"
                }
                else {
                    ExitWithReport $SUCCESS "Nothing to be done. No new certificate needs to be applied" -suppressMail
                }
            }
        }

        LogVerbose "Retrieving new KeyTalk SSL certificate"
        ($pfxCertificateFilePath, $pfxPrivateKeyPassword) = (RetrieveKeyTalkCertificate)

        LogVerbose "Applying retrieved certificate"
        ApplyRetrievedCertificate $pfxCertificateFilePath $pfxPrivateKeyPassword

        ExitWithReport $SUCCESS 'The new certificate has been successfully applied'
    } catch {
        ExitWithReport $ERR_ERROR "An error occurred while applying the new certificate"
    }
}

################################################################################
# Step 1: Check if a certificate needs to be renewed (customizable)
################################################################################
<#
# Simple example of IsCertRenewalNeeded which states that replacement is always needed
Function IsCertRenewalNeeded() {
   return $TRUE
}
#>

# Return $TRUE if the existing SSL certificate needs to be renewed
# Return $FALSE if the existing SSL certificate doesn't need to be renewed. This exits the script with success status.
# The $sendEmailIfApplyNotRequired variable determines if a report is sent or not.
Function IsCertRenewalNeeded() {
    $httpsBindingPath = "IIS:\SslBindings\${httpsBindingIp}!${httpsBindingPort}"
    ($oldCertificate, $oldCertificateStore) = GetIISHttpsCertificate $httpsBindingPath

    $renewalNeeded = $TRUE
    if ($oldCertificate) {

        if (-not (IsCertificateForServerAuthentication $oldCertificate)) {
            LogVerbose 'Certificate needs update because current certificate is not suitable for Server Authentication'
            # No need to continue checking, certificate update is required
            return $TRUE
        }

        # Check whether the cert is expired
        $now = (Get-Date)
        $expirationMarginSec = ($expirationMarginPercent / 100) * ($oldCertificate.NotAfter.Subtract($oldCertificate.NotBefore).TotalSeconds)
        $certificateReplacementDueTime = $oldCertificate.NotAfter.AddSeconds(-$expirationMarginSec)
        $renewalNeeded = ($now -ge $certificateReplacementDueTime)

        if ($renewalNeeded) {
            LogVerbose "Certificate update due since $certificateReplacementDueTime and needs renewal"
        } else {
            LogVerbose "Next certificate update due $certificateReplacementDueTime "

            # Check whether the cert is revoked
            $renewalNeeded = IsCertRevoked($oldCertificate)
            if ($renewalNeeded) {
                LogVerbose "Certificate is revoked and needs renewal"
            } else {
                LogVerbose "Certificate is not revoked"
            }
        }
    } else {
        $renewalNeeded = $TRUE
    }

    return $renewalNeeded
}

################################################################################
# Step 2: Retrieve KeyTalk certificate (customizable)
################################################################################
<#
# Simple example of a RetrieveKeyTalkCertificate function without error handling:
Function RetrieveKeyTalkCertificate() {
   $keyTalkInstallPath = GetStringFromIniFile $env:allusersprofile\KeyTalk\resept.ini 'Install'
   $keyTalkConsoleClientExecutable = "$keyTalkInstallPath\\ReseptConsoleClient.exe"
   &$keyTalkConsoleClientExecutable --provider $keyTalkProvider --service $keyTalkService --user $keyTalkUser --password $keyTalkPassword --save-pfx
   $certFilePath = "${env:TEMP}\keytalk.pfx"
   $certPasswordFilePath = "${env:TEMP}\keytalk.pfx.pass"
   $password = (Get-Content $certPasswordFilePath)
   return ($certFilePath, $password)
}
#>

# Returns: (PFX file path, PFX password)
Function RetrieveKeyTalkCertificate() {
    try {
        LogVerbose "Retrieving certificate for provider '$keyTalkProvider', service '$keyTalkService', and user '$keyTalkUser' from '$keyTalkServerHost`:$keyTalkServerPort'"

        $certFilePath = "${env:TEMP}\keytalk.pfx"
        $certPasswordFilePath = "${env:TEMP}\keytalk.pfx.pass"
        if (Test-Path $certFilePath) {
            remove-item $certFilePath
        }
        if (Test-Path $certPasswordFilePath) {
            remove-item $certPasswordFilePath
        }

        $args = @( )
        $args += "--provider"
        $args += "`"$keyTalkProvider`""
        $args += "--service"
        $args += "`"$keyTalkService`""
        if ($keyTalkUser) {
            $args += "--user"
            $args += "`"$keyTalkUser`""
        }
        if ($keyTalkPassword) {
            $args += "--password"
            $args += "`"$keyTalkPassword`""
        }
        $args += "--save-pfx"

        $retrievalResult = ''
        [string]$keyTalkConsoleClientExecutable = GetKeytalkConsoleClientPath
        &$keyTalkConsoleClientExecutable $args 2>&1 | Tee-Object -Variable retrievalResult | Out-Null
        $retrievalExitCode = $LASTEXITCODE

        if ($retrievalResult) {
            LogVerbose "KeyTalk client message:
$retrievalResult"
        }

        if ($retrievalExitCode -ne 0) {
            LogVerbose "More error information may be found in ${env:appdata}\KeyTalk\ktclient.log"
            throw "Error retrieving KeyTalk Certificate:
KeyTalk client exit code:
$retrievalExitCode

KeyTalk client message:
$retrievalResult

Note: more info may be found in ${env:appdata}\KeyTalk\ktclient.log"
        }

        if ((-not (Test-Path $certFilePath)) -or (-not (Test-Path $certPasswordFilePath))) {
            throw "Error retrieving KeyTalk Certificate. Files not retrieved successfully"
        }

        $password = (Get-Content $certPasswordFilePath)
        return ($certFilePath, $password)
    } catch {
        ExitWithReport $ERR_ERROR "Failed to retrieve new KeyTalk certificate"
    }
}

################################################################################
# Step 3: Apply retrieved certificate (customizable)
################################################################################
<#
# Simple example of ApplyRetrievedCertificate without error handling and with the assumption
# that there is an existing certificate an old certificate is applied
Function ApplyRetrievedCertificate(
       [parameter(mandatory)][string]$pfxFilePath,
       [parameter(mandatory)][string]$pfxPassword) {
   $httpsBindingPath = "IIS:\SslBindings\${httpsBindingIp}!${httpsBindingPort}"

   LogVerbose 'Import certificate into store'
   $newCertificate = GetCertificateFromFile $pfxCertificateFilePath $pfxPrivateKeyPassword
   ImportCertificateToStore $newCertificate $certificateRootStore $certificateStore

   if (Test-Path $httpsBindingPath) {
       LogVerbose "Removing current certificate from IIS HTTPS binding '$httpsBindingPath'"
       Remove-Item $httpsBindingPath
   }

   AssignIISHttpsCertificate $httpsBindingPath $certificateRootStore $certificateStore $newCertificate
}
#>

# Inputs: (PFX file path, PFX password)
Function ApplyRetrievedCertificate(
        [parameter(mandatory)][string]$pfxFilePath,
        [parameter(mandatory)][string]$pfxPassword) {
    try {
        $httpsBindingPath = "IIS:\SslBindings\${httpsBindingIp}!${httpsBindingPort}"
        LogVerbose 'Retrieving current IIS HTTPS binding configuration'
        ($oldCertificate, $oldCertificateStore) = GetIISHttpsCertificate $httpsBindingPath
        if ($oldCertificate) {
            $oldCertificateHash = $oldCertificate.GetCertHashString()
            $oldCertificatePath = "Cert:\$certificateRootStore\$oldCertificateStore\$oldCertificateHash"
        }

        try {
        LogVerbose "Loading new certificate"
            $now = (Get-Date)
            $newCertificate = GetCertificateFromFile $pfxCertificateFilePath $pfxPrivateKeyPassword

            if ($oldCertificate -and ($oldCertificate.GetCertHashString() -eq $newCertificate.GetCertHashString())) {
                ExitWithReport $ERR_ERROR 'Given certificate is the same as the currently installed one.'
            }

            if (-not (IsCertificateForServerAuthentication $newCertificate)) {
                ExitWithReport $ERR_ERROR "Retrieved certificate is not suitable for Server Authentication. Please check that the 'Extended Key Usage' parameter of your KeyTalk service is configured for Server Authentication (OID 1.3.6.1.5.5.7.3.1)"
            }

            if ($now -ge $newCertificate.NotAfter) {
                ExitWithReport $ERR_ERROR 'Certificate to be installed is already expired'
            }

            LogVerbose "Importing newly obtained certificate into certificate store '$certificateStore'"
            ImportCertificateToStore $newCertificate $certificateRootStore $certificateStore
        } catch {
            ExitWithReport $ERR_ERROR 'Unable to import certificate'
        }

        if (Test-Path $httpsBindingPath) {
            LogVerbose "Removing current certificate from IIS HTTPS binding '$httpsBindingPath'"
            Remove-Item $httpsBindingPath
        }

        LogVerbose "Assigning new certificate to IIS HTTPS binding '$httpsBindingPath'"
        AssignIISHttpsCertificate $httpsBindingPath $certificateRootStore $certificateStore $newCertificate

        if ($oldCertificate -and $shouldRemoveOldCertificate) {
            LogVerbose "Removing old certificate from certificate store"
            remove-item $oldCertificatePath
        }

        ($appliedCertificate, $appliedCertificateStore) = GetIISHttpsCertificate $httpsBindingPath
        $appliedCertificateExpirationTime = $appliedCertificate.NotAfter

        LogVerbose "New certificate successfully applied, certificate valid until '$appliedCertificateExpirationTime'"
    } catch {
        ExitWithReport $ERR_ERROR "Failed to apply certificate"
    }
}

################################################################################
# Utility functions
################################################################################
Function ExitWithReport(
        [parameter(mandatory)][int]$errorCode,
        [parameter(mandatory)][string]$message,
        [switch]$suppressMail
        ) {

    if ($emailReporting) {
        if ($suppressMail) {
            ExitVerbose $errorCode $message $scriptLogFilePath -emailReporting $emailFrom $emailTo $emailSubject $smtpServer -suppressMail
        } else {
            ExitVerbose $errorCode $message $scriptLogFilePath -emailReporting $emailFrom $emailTo $emailSubject $smtpServer
        }
    } else {
        ExitVerbose $errorCode $message $scriptLogFilePath
    }
}

Function GetIISHttpsCertificate(
        [parameter(mandatory)][string]$httpsBindingPath) {
    $certificate = $null
    if (Test-Path "$httpsBindingPath") {
        $httpsBinding = Get-Item $httpsBindingPath
        $certificateHash = $httpsBinding.Thumbprint
        $certificateStore = $httpsBinding.Store
        if ($certificateHash -and $certificateStore) {
            $certificatePath = "Cert:\LocalMachine\$certificateStore\$certificateHash"
            if (Test-Path $certificatePath) {
                $certificate = Get-Item $certificatePath
            }
        }
    }

    if ($certificate) {
        return ($certificate, $certificateStore)
    } else {
        return ($null, $null)
    }
}

Function AssignIISHttpsCertificate(
        [parameter(mandatory)][string]$httpsBindingPath,
        [parameter(mandatory)][string]$certificateRootStore,
        [parameter(mandatory)][string]$certificateStore,
        [parameter(mandatory)][System.Security.Cryptography.X509Certificates.X509Certificate2]$certificate) {
    $certificateHash = $certificate.GetCertHashString()
    $certificatePath = "Cert:\$certificateRootStore\$certificateStore\$certificateHash"
    $httpsBinding = (get-item $certificatePath | new-item $httpsBindingPath)

    if ($httpsBinding.Thumbprint -ne $newCertificate.GetCertHashString()) {
        throw "Could not apply new certificate: New certificate has not been successfully assigned to the IIS HTTPS binding"
    }
}

################################################################################
# Configuration
################################################################################

Function LoadBasicConfiguration() {
    try
    {
        try {
            $ignore = GetKeytalkConfigToolPath # Check for existence of the configuration tool
        } catch {
            ExitWithReport $ERR_ERROR $error[0].Exception.Message
        }

        # Best effort loading of logging and e-mail error reporting information
        # This is duplicated from the actual configuration loading (which is done after task validation)
        # Reason: even when a task does not have a fully correct definition, logging and e-mail reporting should
        # be performed as well as possible
        try {
            [string]$global:scriptLogFilePath     = Invoke-ConfigToolChecked @( 'task', 'getparam', $taskName, 'ScriptLogFilePath' )
        } catch {
            # Best effort
        }

        try {
            $global:emailReporting                = StringToBool (Invoke-ConfigToolChecked @( 'task', 'getparam', $taskName, 'EmailReporting' ))
            if ($global:emailReporting) {
                [string]$global:emailFrom         = Invoke-ConfigToolChecked @( 'task', 'getparam', $taskName, 'EmailFrom' )
                [string]$global:emailTo           = Invoke-ConfigToolChecked @( 'task', 'getparam', $taskName, 'EmailTo' )
                [string]$global:smtpServer        = Invoke-ConfigToolChecked @( 'task', 'getparam', $taskName, 'SmtpServer' )
                [string]$global:emailSubject      = Invoke-ConfigToolChecked @( 'task', 'getparam', $taskName, 'EmailSubject' )
            }
        } catch {
            # Best effort, if no proper config for e-mail reporting exists, skip e-mail
            $global:emailReporting                = $FALSE
        }
    } catch {
        ExitWithReport $ERR_CONFIG "Could not load basic KeyTalk configuration."
    }
}

Function LoadTaskConfiguration() {
    try
    {
        # User-defined parameters
        [string]$global:scriptLogFilePath         = Invoke-ConfigToolChecked @( 'task', 'getparam', $taskName, 'ScriptLogFilePath' )

        $global:emailReporting                    = StringToBool (Invoke-ConfigToolChecked @( 'task', 'getparam', $taskName, 'EmailReporting' ))
        if ($global:emailReporting) {
            [string]$global:emailFrom                 = Invoke-ConfigToolChecked @( 'task', 'getparam', $taskName, 'EmailFrom' )
            [string]$global:emailTo                   = Invoke-ConfigToolChecked @( 'task', 'getparam', $taskName, 'EmailTo' )
            [string]$global:smtpServer                = Invoke-ConfigToolChecked @( 'task', 'getparam', $taskName, 'SmtpServer' )
            [string]$global:emailSubject              = Invoke-ConfigToolChecked @( 'task', 'getparam', $taskName, 'EmailSubject' )
        }

        [bool]$global:sendEmailOnSuccess          = StringToBool (Invoke-ConfigToolChecked @( 'task', 'getparam', $taskName, 'SendEmailOnSuccess' ))
        [bool]$global:sendEmailIfApplyNotRequired = StringToBool (Invoke-ConfigToolChecked @( 'task', 'getparam', $taskName, 'SendEmailIfApplyNotRequired' ))

        [string]$global:httpsBindingIp            = Invoke-ConfigToolChecked @( 'task', 'getparam', $taskName, 'HttpsBindingIp' )
        [int]$global:httpsBindingPort             = Invoke-ConfigToolChecked @( 'task', 'getparam', $taskName, 'HttpsBindingPort' )

        [string]$global:keyTalkProvider           = Invoke-ConfigToolChecked @( 'task', 'getparam', $taskName, 'KeyTalkProvider' )
        [string]$global:keyTalkService            = Invoke-ConfigToolChecked @( 'task', 'getparam', $taskName, 'KeyTalkService' )
        [string]$global:keyTalkUser               = Invoke-ConfigToolChecked @( 'task', 'getparam', $taskName, 'KeyTalkUser' )
        [string]$global:keyTalkPassword           = Invoke-ConfigToolChecked @( 'task', 'getparam', $taskName, 'KeyTalkPassword' )

        [string]$global:certificateRootStore      = 'LocalMachine'
        [string]$global:certificateStore          = Invoke-ConfigToolChecked @( 'task', 'getparam', $taskName, 'CertificateStore' )

        [bool]$global:shouldRemoveOldCertificate  = StringToBool (Invoke-ConfigToolChecked @( 'task', 'getparam', $taskName, 'ShouldRemoveOldCertificate' ))

        # Certificate refresh margin
        # For example: 0 means never apply a new certificate until it is expired
        # 60 means apply a new certificate only when it has one minute until expiration (not before that)
        # Mind that it may take a few seconds to obtain a certificate
        [int]$global:expirationMarginPercent      = GetValidityPercentage $global:keyTalkProvider $global:keyTalkService

        [string]$global:rootCertStoreLocation     = "Cert:\$certificateRootStore"
        [string]$global:certStoreLocation         = "$rootCertStoreLocation\$certificateStore"

        [string]$global:keyTalkServerHost         = Invoke-ConfigToolChecked @( 'provider', 'getparam', $global:keyTalkProvider, 'ServerHost' )
        [int]$global:keyTalkServerPort            = Invoke-ConfigToolChecked @( 'provider', 'getparam', $global:keyTalkProvider, 'ServerPort' )
    } catch {
        ExitWithReport $ERR_CONFIG "Could not load task configuration for task '$taskName', please use the configuration manager to configure this task."
    }
}

Function PreValidateConfiguration() {
    try {
        if (-not (IsTaskConfigurationValid $taskName)) {
            ExitWithReport $ERR_CONFIG "Error in configuration of task '$taskName', please use the configuration manager to configure this task."
        }
    } catch {
        ExitWithReport $ERR_CONFIG "Error in configuration of task '$taskName', please use the configuration manager to configure this task."
    }
}

Function PostValidateConfiguration() {
    try {
        try {
            $ignore = GetKeytalkConsoleClientPath # Check for existence of the console client
        } catch {
            ExitWithReport $ERR_ERROR $error[0].Exception.Message
        }

        [bool]$bindingExists = $FALSE
        foreach ($site in (get-childitem 'IIS:\sites')) {
            foreach ($binding in $site.bindings.Collection) {
                if ($binding.protocol -eq 'https') {
                    $hasMatch = $binding.bindingInformation -match '^.*:(.*):.*$'
                    if ($hasMatch -and ($matches[1] -eq $httpsBindingPort)) {
                        $bindingExists = $TRUE
                    }
                }
            }
        }
        if (-not $bindingExists){
            ExitWithReport $ERR_CONFIG "Cannot find an IIS HTTPS binding for '$httpsBindingIp!$httpsBindingPort'"
        }
    } catch {
        ExitWithReport $ERR_CONFIG "Error in configuration of task '$taskName', please use the configuration manager to configure this task."
    }
}

################################################################################
# Main
################################################################################

Main
