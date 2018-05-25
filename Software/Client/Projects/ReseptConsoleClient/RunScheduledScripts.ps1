Set-StrictMode -Version 3.0
Import-Module -name $PSScriptRoot\KeyTalkUtils.psm1
################################################################################
# Error codes
################################################################################
$SUCCESS = 0
$ERR_ERROR = 1
################################################################################
# Main
################################################################################

[string]$keyTalkInstallPath             = GetStringFromIniFile $env:allusersprofile\KeyTalk\resept.ini 'Install'
[string]$keyTalkConfigToolExecutable    = GetKeytalkConfigToolPath
[string]$scriptsDirPath                 = "$keyTalkInstallPath\\Scripts"
[string]$iisCertificateUpdateScriptPath = "$scriptsDirPath\\UpdateIISCertificate.ps1"

if (-not (Test-Path $scriptsDirPath)) {
    Write-Error "Scripts directory does not exist: '$scriptsDirPath'"
    exit $ERR_ERROR
}

$result = $SUCCESS
$tasks = GetTaskList
Foreach ($task in $tasks) {
    if ($task.Trim() -ne '')
    {
        Write-Host "Executing task: '$task' using '$iisCertificateUpdateScriptPath'"
        try {
            &powershell -ExecutionPolicy Bypass -file $iisCertificateUpdateScriptPath -taskName $task
            $res = $LastExitCode
            if ($res -ne $SUCCESS) {
                $errorMessage = "Error: '$iisCertificateUpdateScriptPath' exited with error code '$res'. Continuing with remaining tasks."
                Write-Host $errorMessage
                throw $errorMessage
            }
        } catch {
            $result = $ERR_ERROR
        }
    }
}

exit $result