Set-StrictMode -Version 3.0
################################################################################
# Error codes
################################################################################

# Generic errors
$global:SUCCESS     = 0
$global:ERR_ERROR   = 1
$global:ERR_CONFIG  = 2
$global:ERR_LOGGING = 3

################################################################################
# Dependency testing
################################################################################
Function Test-Administrator() {
    $user = [Security.Principal.WindowsIdentity]::GetCurrent();
    return (New-Object Security.Principal.WindowsPrincipal $user).IsInRole([Security.Principal.WindowsBuiltinRole]::Administrator)
}

Function IsPowershellModuleInstalled(
        [parameter(mandatory)][string]$aModuleName) {
    if (Get-Module -ListAvailable -Name $aModuleName) {
        Write-Output "Found powershell module '$aModuleName'"
        exit 0
    } else {
        Write-Output "Powershell module '$aModuleName' does not exist"
        exit 1
    }
}

Function IsPowershellVersionSupported(
        [parameter(mandatory)][int]$aMajorVersion) {
     $isCompatible = $PSVersionTable.PSCompatibleVersions.Major.Contains($aMajorVersion)
     if ($isCompatible) {
        Write-Output "Powershell installation is compatible with PowerShell $aMajorVersion scripts."
        exit 0
     }
     Write-Output "Powershell installation not compatible with PowerShell $aMajorVersion scripts."
     exit 1
}

################################################################################
# Certificate operations
################################################################################

Function HasCertificateExtension(
        [parameter(mandatory)][System.Security.Cryptography.X509Certificates.X509Certificate2]$certificate,
        [parameter(mandatory)][string]$oid) {
    foreach ($ext in $certificate.Extensions) {
        if ($ext.Oid.Value -eq $oid) {
            return $TRUE
        }
    }
    return $FALSE
}

Function IsCertificateForServerAuthentication (
        [parameter(mandatory)][System.Security.Cryptography.X509Certificates.X509Certificate2]$certificate) {
    foreach ($ext in $certificate.Extensions) {
        # If certificate has enhanced key usage extension
        if ($ext.Oid.Value -eq '2.5.29.37') {
            foreach ($keyUsage in $ext.EnhancedKeyUsages) {
                # If certificate may be used for Server authentication
                if ($keyUsage.Value -eq '1.3.6.1.5.5.7.3.1') {
                   return $TRUE
                }
            }
        }
    }
    return $FALSE
}

Function GetCertificateFromFile(
        [parameter(mandatory)][string]$certPath,
        [parameter(mandatory)][string]$pfxPass) {
    $pfx = new-object System.Security.Cryptography.X509Certificates.X509Certificate2
    $pfx.import($certPath, $pfxPass, "Exportable,PersistKeySet,MachineKeySet")
    return $pfx
}

Function SaveCertificateToTempFile(
        [parameter(mandatory)][System.Security.Cryptography.X509Certificates.X509Certificate2]$certificate) {

        $tempFileName = [System.Guid]::NewGuid().ToString()
        $systemTempDir = $env:Temp
        $filePath = "${systemTempDir}\${tempFileName}.der"
        $certDerBytes = $certificate.Export("Cert")
        [io.file]::WriteAllBytes($filePath, $certDerBytes)
        return $filePath
}

Function IsCertRevoked(
        [parameter(mandatory)][System.Security.Cryptography.X509Certificates.X509Certificate2]$certificate) {

        $certDerPath = SaveCertificateToTempFile($certificate)
        $exitCode, $result = Invoke-ConfigTool @( 'cert', 'is-revoked', $certDerPath)
        Remove-Item -Path $certDerPath -Force

        if ($exitCode -ne $SUCCESS) {
            throw "Cannot retrieve certificate revocation information, error code $exitCode"
        }

        if ($result -eq 'revoked') {
            return $TRUE
        } else {
            return $FALSE
        }
 }

Function ImportCertificateToStore(
        [parameter(mandatory)][System.Security.Cryptography.X509Certificates.X509Certificate2]$certificate,
        [parameter(mandatory)][string]$certRootStore,
        [parameter(mandatory)][string]$certStore) {
    $store = new-object System.Security.Cryptography.X509Certificates.X509Store($certStore,$certRootStore)
    $store.open("MaxAllowed")
    $store.add($certificate)
    $store.close()

    # It can take a few seconds before the certificate store is updated, so we need to wait until the certificate is available
    $waitForCertificateTimeoutSec = 5
    $now = Get-Date
    $certDeadline = $now.addSeconds($waitForCertificateTimeoutSec)
    $certHash = $certificate.GetCertHashString()
    $certPath = "Cert:\$certRootStore\$certStore\$certHash"
    while (-not (Test-Path $certPath)) {
        if ((Get-Date) -ge $certDeadline) {
            throw 'Unable to import certificate: timeout while waiting for certificate'
        }
        sleep 0.1
    }
}

################################################################################
# Configuration
################################################################################

Function GetKeytalkConfigToolPath() {
    [string]$keyTalkInstallPath = GetStringFromIniFile $env:allusersprofile\KeyTalk\resept.ini 'Install'
    [string]$keyTalkConfigToolExecutable = "$keyTalkInstallPath\\ReseptConfigTool.exe"

    if (-not (Test-Path $keyTalkConfigToolExecutable)) {
        throw "KeyTalk configuration tool executable '$keyTalkConfigToolExecutable' does not exist"
    }

    return $keyTalkConfigToolExecutable
}

Function GetKeytalkConsoleClientPath() {
    [string]$keyTalkInstallPath = GetStringFromIniFile $env:allusersprofile\KeyTalk\resept.ini 'Install'
    [string]$keyTalkConsoleClientExecutable = "$keyTalkInstallPath\\ReseptConsoleClient.exe"

    if (-not (Test-Path $keyTalkConsoleClientExecutable)) {
        throw "KeyTalk configuration tool executable '$keyTalkConsoleClientExecutable' does not exist"

    }

    return $keyTalkConsoleClientExecutable
}

Function GetStringFromIniFile(
        [parameter(mandatory)][string]$iniFile,
        [parameter(mandatory)][string]$key) {
    $iniContent = Get-Content $iniFile
    [string]$regex = "^\s*${key}\s*=\s*`"(?<value>.*)`"\s*;\s*$"

    $matchingLines = $iniContent -match $regex
    if ($matchingLines.Length -lt 1) {
        throw "Key '$key' not found in INI file '$iniFile'"
    } elseif ($matchingLines.Length -gt 1) {
        throw "More than one '$key' key found in INI file '$iniFile'"
    }
    # At this point we know the regular expression matches and there is exactly one match
    $matchingLines[0] -match $regex | Out-Null
    return $matches.value
}

Function Invoke-ConfigTool(
        [parameter(mandatory)][string[]]$anArgs) {
    $retrievalResult = ''
    [string]$keyTalkConfigToolExecutable = GetKeytalkConfigToolPath
    &$keyTalkConfigToolExecutable $anArgs 2>&1 | Tee-Object -Variable retrievalResult | Out-Null
    $retrievalExitCode = $LASTEXITCODE
    return $retrievalExitCode, $retrievalResult
}

Function Invoke-ConfigToolChecked(
        [parameter(mandatory)][string[]]$anArgs) {
    $exitCode, $result = Invoke-ConfigTool $anArgs
    if ($exitCode -ne $SUCCESS)
    {
        throw "Error running config tool with parameters '$anArgs'"
    }
    return $result
}

Function IsTaskConfigurationValid(
        [parameter(mandatory)][string]$aTaskName) {
    $exitCode, $retrievalResult = Invoke-ConfigTool @( 'task', 'validate', $aTaskName )

    if ($retrievalResult -eq 'valid') {
        return $TRUE
    } else {
        return $FALSE
    }
}

Function GetValidityPercentage(
        [parameter(mandatory)][string]$aProviderName,
        [parameter(mandatory)][string]$aServiceName) {
    $exitCode, $stringValue = Invoke-ConfigTool @( 'service', 'getparam', $aProviderName, $aServiceName, 'CertValidPercent' )

    if ($exitCode -ne $SUCCESS)
    {
        throw "Cannot retrieve validity percentage for service '$aServiceName' of provider '$aProviderName', error code $exitCode"
    }

    try {
        [int]$result = $stringValue;
        return $result;
    } catch {
        throw "Non integer value found in validity percentage for service '$aServiceName' of provider '$aProviderName': $stringValue"
    }

    return $result
}

Function GetTaskList() {
    $exitCode, $retrievalResult = Invoke-ConfigTool @( 'task', 'list' )
    if ($exitCode -ne $SUCCESS)
    {
        throw "Cannot retrieve task list $exitCode"
    }
    return $retrievalResult -split '`n'
}

################################################################################
# Logging
################################################################################
# This function should be called right after importing this module since it
# resets the builtin error variable that accumulates errors
Function InitLoggingFacility() {
    $global:verbosePreference = 'Continue' # From this point on, print verbose messages to the console
    $global:error.clear() # Clear all previously logged errors from this session
    $global:keyTalkLog = @()
    $curTime = GetLogTime
    $global:keyTalkLog += "$curTime`: Started logging"
}

Function LogVerbose(
        [parameter(mandatory)][string]$msg) {
    $curTime = GetLogTime
    Write-Verbose "$curTime`: $msg"

    if (-not $global:keyTalkLog) {
        # Print error, but do not interrupt the rest of the program if logging fails
        Write-Error "Logging facility was not initialized"
    } else  {
        $global:keyTalkLog += "$curTime`: $msg"
    }
}

Function GetLog() {
    return $global:keyTalkLog
}

Function GetLogTime() {
    return Get-Date -Format "yyyy/MM/dd HH:mm:ss.ffff"
}

Function ExitVerbose(
        [parameter(mandatory)][int]$errorCode,
        [parameter(mandatory)][string]$message,
        [parameter(mandatory)][string]$scriptLogFilePath,
        [switch]$emailReporting,
        [string]$emailFrom,
        [string]$emailTo,
        [string]$myEmailSubject,
        [string]$smtpServer,
        [switch]$suppressMail = $false
        ) {
    if ($errorCode -ne $SUCCESS) {
        LogVerbose "ERROR: $message"
    } else {
        LogVerbose "SUCCESS: $message"
    }

    $tempId = [System.Guid]::NewGuid().ToString()
    $systemTempDir = $env:Temp
    $tempDirectoryPath = "$systemTempDir\$tempId"
    $tempDirectory = New-Item -ItemType directory -Path $tempDirectoryPath
    $errorFilePath = "$tempDirectoryPath\errors.txt"
    $logFilePath = "$tempDirectoryPath\log.txt"
    Set-Content $logFilePath (GetLog)

    if ($errorCode -ne $SUCCESS) {
        Write-Error "Error code ${errorCode}: ${message}"
        # Because write-error was used, the message and call context is already included in the error variable

        $lastErrorDetails = ''
        if ($global:error[0])
        {
            $lastError = $global:error[0]
        }
    } else {
        Write-Output $message
    }

    # Note that collection of errors has to be done after Write-Error, because Write-Error updates the
    # $global:error variable with the written error message.

    $fullErrorDetails = @()
    if ($global:error) {
        foreach ($e in $global:error) {
            $cat = $e.CategoryInfo
            $ex = $e.Exception
            $pos = $e.InvocationInfo.PositionMessage
            $fullErrorDetails += "$pos`r`n"
            $fullErrorDetails += "$cat`r`n"
            $fullErrorDetails += "$ex`r`n"
            $fullErrorDetails += "----------------------------------------`r`n"
        }
        Set-Content $errorFilePath $fullErrorDetails
    }


    Write-Verbose "Writing log file to '${scriptLogFilePath}'"
    Set-Content $scriptLogFilePath (GetLog)

    if ($fullErrorDetails) {
        Write-Verbose '========================================'
        Write-Verbose 'Full error details'
        Write-Verbose '========================================'
        Write-Verbose ($fullErrorDetails -join "`r`n")
        Write-Verbose '========================================'

        Add-Content $scriptLogFilePath ''
        Add-Content $scriptLogFilePath '========================================'
        Add-Content $scriptLogFilePath 'Full error details'
        Add-Content $scriptLogFilePath '========================================'
        Add-Content $scriptLogFilePath $fullErrorDetails
    }

    if ($errorCode -ne $SUCCESS) {
        Write-Verbose "Writing log files complete. Please check the log file for a full error details."
    }

    if ($emailReporting -and (-not $suppressMail)) {
        if (($errorCode -ne $SUCCESS) -or (($errorCode -eq $SUCCESS) -and ($sendEmailOnSuccess))) {
            Write-Verbose 'Sending notification via E-mail'

            if ($errorCode -ne $SUCCESS) {
                $lastErrorDetails = "Last error:`r`n"
                $lastErrorDetails += "<font color='#900'><pre>`r`n"
                $lastErrorDetails += "$lastError`r`n"
                $lastErrorDetails += "</pre></font>`r`n"
                $lastErrorDetails += "`r`n"
                $lastErrorDetails += "More information about the errors can be found in the attachments.`r`n"

                $myEmailSubject = "${emailSubject}: FAILED"

                $myEmailBody = "<h3>Status</h3>`r`n"
                $myEmailBody += "$myEmailSubject <br /><br />`r`n"
                $myEmailBody += "`r`n"
                $myEmailBody += "$lastErrorDetails`r`n"
            } else {
                $myEmailSubject = "${emailSubject}: OK"

                $myEmailBody = "<h3>Status</h3>`r`n"
                $myEmailBody += "$myEmailSubject<br /><br />`r`n"
                $myEmailBody += "`r`n"
                $myEmailBody += "Message:`r`n"
                $myEmailBody += "<font color='gray'><pre>$message</pre></font>`r`n"
                $myEmailBody += "</pre>`r`n"
            }

            $attachments = @()
            $attachments += $logFilePath
            if (Test-Path $errorFilePath) {
                $attachments += $errorFilePath
            }
            if (Test-Path "${env:appdata}\KeyTalk\ktclient.log") {
                $attachments += "${env:appdata}\KeyTalk\ktclient.log"
            }
            Send-MailMessage -From "$emailFrom" -To "$emailTo" -Subject "$myEmailSubject" -BodyAsHtml -Body "$myEmailBody" -SmtpServer "$smtpServer" -Attachments $attachments
        }
    }

    try {
        Remove-Item -Recurse -Force $tempDirectoryPath
    } catch {
        # Best effort cleanup: ignore errors if cleanup fails
    }

    exit $errorCode
}

################################################################################
# Miscellaneous
################################################################################

Function StringToBool(
    [parameter(mandatory)][string]$aString) {
    if ($aString -eq '0') {
        return $FALSE
    } elseif ($aString -eq '1') {
        return $TRUE
    } else {
        throw "Non-boolean value found for boolean task parameter '$aParamName' for task '$aTaskName': '$aString'"
    }
}

Function SendTestMail(
    [parameter(mandatory)][string]$aFrom,
    [parameter(mandatory)][string]$aTo,
    [parameter(mandatory)][string]$aSubject,
    [parameter(mandatory)][string]$aSmtpServer
    ) {
    $global:error.Clear()
    try {
        Send-MailMessage -From "$aFrom" -To "$aTo" -Subject "$aSubject" -Body "This is an e-mail to test if your E-mail settings are set up properly." -SmtpServer "$aSmtpServer"
        if ($global:error.Count -gt 0) {
            throw "Errors occurred while sending the test mail." # This also adds a message to the $global:error variable
        }
        Write-Host "Test e-mail sent successfully to '$aTo'."
        return $TRUE
    } catch {
        foreach ($err in $global:error) {
            Write-Host $err.Exception.Message
        }
        return $FALSE
    }
}

Export-ModuleMember -function *