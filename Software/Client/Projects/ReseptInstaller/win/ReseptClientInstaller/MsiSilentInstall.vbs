' The script installs, upgrades and customizes KeyTalk Client msi in GUI-less mode
' To display usage, please run:
' cscript /nologo MsiSilentInstall.vbs --help
' return 0 on success, <>0 on error
' Note: you should be admin to execute this script

'#####################################################
' Imports
'#####################################################
Dim fso
Set fso = CreateObject("Scripting.FileSystemObject")

Dim sho
Set sho = CreateObject("Wscript.Shell")

'#####################################################
' Constants
'#####################################################
Const RetSuccess = 0
Const RetError   = 2

Const OptionWithIis  = "--with-iis"
Const OptionTasksIni = "--tasks-ini"
Const OptionInstallUserName = "--installuser-name"
Const OptionInstallUserPassword = "--installuser-password"
Const OptionTaskAfterLogin = "--tasks-after-login" ' No special action is executed for this option, this option is added to make an explicit choice on the command line between --tasks-after-login and --tasks-at-boot
Const OptionTaskSystemStartup = "--tasks-at-boot"
Const OptionUninstall = "--uninstall"
Const OptionHelp     = "--help"

FlagOptions = Array(OptionWithIis, OptionTaskAfterLogin, OptionTaskSystemStartup, OptionUninstall, OptionHelp)
ValueOptions = Array(OptionTasksIni, OptionInstallUserName, OptionInstallUserPassword)

ConfigManagerLogPath                = sho.ExpandEnvironmentStrings("%TEMP%") & "\ktconfig.log"
ReseptIniPath                       = sho.ExpandEnvironmentStrings("%ALLUSERSPROFILE%") & "\KeyTalk\resept.ini"
Const ReseptConfigManagerExecutable = "ReseptConfigManager.exe"

Const ScheduledTaskName = "KeyTalkScheduledScripts"

'#####################################################
' Main
'#####################################################
If hasOption(OptionHelp) Then
    printUsage()
    WScript.Quit RetSuccess
End If

If hasOption(OptionUninstall) And getPositionalArgumentCount() = 1 Then
    myMsiPath = fso.GetAbsolutePathName(WScript.Arguments.Item(0))
    If Not(uninstallKeyTalk(myMsiPath)) Then
        WScript.Quit RetError
    End If
    WScript.Quit RetSuccess
End If

checkUsageValid()

installKeyTalk()

'#####################################################
' Installation functions
'#####################################################
Function printUsage()
    WScript.StdErr.WriteLine
    WScript.StdErr.WriteLine "Usage:"
    WScript.StdErr.WriteLine "cscript /nologo " & WScript.ScriptName & " " & OptionHelp
    WScript.StdErr.WriteLine "cscript /nologo " & WScript.ScriptName & " <path\to\msi> [ <path\to\rccd> ] [ " & OptionWithIis & " [" & OptionTaskAfterLogin & "] " & " [" & OptionTaskSystemStartup & "] "& " [ " & OptionTasksIni & " <path\to\tasks.ini> ] ]"
    WScript.StdErr.WriteLine "cscript /nologo " & WScript.ScriptName & " " & OptionUninstall
    WScript.StdErr.WriteLine
    WScript.StdErr.WriteLine " " & OptionWithIis & " : Install the IIS certificate renewal functionality"
    WScript.StdErr.WriteLine " " & OptionTasksIni & " <path\to\tasks.ini> : After installation, install a task configuration (only with an RCCD file and " & OptionWithIis & ")"
    WScript.StdErr.WriteLine " " & OptionTaskAfterLogin & " : Scheduled tasks start running only when logged into this machine (only with " & OptionWithIis & ")"
    WScript.StdErr.WriteLine " " & OptionTaskSystemStartup & " : Scheduled tasks start running at system startup, requires credentials of user running this installer (only with " & OptionWithIis & ", " & OptionInstallUserName & ", and " & OptionInstallUserPassword & ")"
End Function

Function checkUsageValid()
    Dim myNumPosArgs
    myNumPosArgs = getPositionalArgumentCount()
    printUsageAndExitIf Not(myNumPosArgs = 1 Or myNumPosArgs = 2 Or myNumPosArgs = 4),_
                        "Incorrect number of positional arguments."

    checkArgumentValuePresence()

    myMsiPath = WScript.Arguments(0)

    printUsageAndExitIf Not(fso.FileExists(myMsiPath)),_
                        "Cannot find MSI installer " & quote(myMsiPath)


    myCustomize = myNumPosArgs >= 2
    If myCustomize Then
        myRccdPath = WScript.Arguments(1)

        printUsageAndExitIf Not(fso.FileExists(myRccdPath)),_
                            "Cannot find specified RCCD file " & quote(myRccdPath)
    End If

    printUsageAndExitIf hasOption(OptionWithIis) And Not(hasOption(OptionTaskSystemStartup)) And Not(hasOption(OptionTaskAfterLogin)),_
                        OptionWithIis & " requires either " & OptionTaskSystemStartup & " or " & OptionTaskAfterLogin

    If hasOption(OptionTaskAfterLogin) Then
        printUsageAndExitIf Not(hasOption(OptionWithIis)),_
                            OptionTaskAfterLogin & " requires " & OptionWithIis

        printUsageAndExitIf hasOption(OptionTaskSystemStartup),_
                            OptionTaskAfterLogin & " cannot be used together with " & OptionTaskSystemStartup
    End If

    If hasOption(OptionTaskSystemStartup) Then
        printUsageAndExitIf Not(hasOption(OptionWithIis)),_
                            OptionTaskSystemStartup & " requires " & OptionWithIis

        printUsageAndExitIf (Not(hasOption(OptionInstallUserName)) Or Not(hasOption(OptionInstallUserPassword))),_
                            OptionTaskSystemStartup & " requires " & OptionInstallUserName & " and " & OptionInstallUserPassword

        printUsageAndExitIf hasOption(OptionTaskAfterLogin),_
                            OptionTaskSystemStartup & " cannot be used together with " & OptionTaskSystemStartup
    End If

    If hasOption(OptionTasksIni) Then
        myTasksIniPath = getOptionValue(OptionTasksIni)

        printUsageAndExitIf Not(hasOption(OptionWithIis)),_
                            "Option " & quote(OptionTasksIni) & " can only be used together with the " & quote(OptionWithIis) & " option."

        printUsageAndExitIf Not(fso.FileExists(myTasksIniPath)),_
                            "Cannot find specified task configuration file " & quote(myTasksIniPath)

        printUsageAndExitIf Not(myCustomize),_
                            "Option " & quote(OptionTasksIni) & " can only be used together with an RCCD file because task definitions depend on installed services."
    End If
End Function

Function installKeyTalk()
    myNumPosArgs = getPositionalArgumentCount()
    Dim myPosArgs()
    ReDim myPosArgs(myNumPosArgs - 1) ' [0..myNumPosArgs-1]
    For i=0 To myNumPosArgs - 1
        myPosArgs(i) = WScript.Arguments.Item(i)
    Next

    RCCDCustomization = getArrayLength(myPosArgs) >= 2
    MsiPath = fso.GetAbsolutePathName(myPosArgs(0))

    ' Print feature list
    printIf True                      , "Selected the following features:"
    printIf True                      , "* Windows Client"
    printIf RCCDCustomization         , "* Client customization with " & quote(RccdPath)
    printIf hasOption(OptionWithIis)  , "* IIS certificate renewal"
    printIf hasOption(OptionTasksIni) , "* Install tasks configuration from " & quote(fso.GetAbsolutePathName(getOptionValue(OptionTasksIni)))

    ' Build up command line to pass to msiexec
    CmdLine = "msiexec /i " & quote(MsiPath) & " /qn"

    If RCCDCustomization Then
        RccdPath = fso.GetAbsolutePathName(myPosArgs(1))
        CmdLine = CmdLine & " RCCDPATH=" & quote(RccdPath)
    End If

    myFeatureSet = "CoreFeature,IeFeature"
    If hasOption(OptionWithIis) Then
        myFeatureSet = myFeatureSet & ",IISCertificateUpdateScript"
    End If
    CmdLine = CmdLine & " ADDLOCAL=" & myFeatureSet

    ' Perform installation
    WScript.Stdout.Write "Installing KeyTalk..."

    retval = sho.Run(CmdLine, , true)
    If retval = 0 Then
        WScript.StdOut.WriteLine "done"
        If isIeRunning() Then
            WScript.StdOut.WriteLine "You should restart Internet Explorer to make use of KeyTalk secure connections"
        End If
    Else
        WScript.StdOut.WriteLine "error (code " & retval & ")"
        WScript.StdOut.Write "For more info please consult Event Viewer Application logs"
        If RCCDCustomization Then
            WScript.StdOut.WriteLine " and " & quote(ConfigManagerLogPath)
        Else
            WScript.StdOut.WriteLine ""
        End if
        WScript.Quit retval
    End If

    ' Post-install tasks
    If hasOption(OptionTaskSystemStartup) Then
        success = enableTasksAtSystemStartup(getOptionValue(OptionInstallUserName), getOptionValue(OptionInstallUserPassword))
        if Not(success) Then
            uninstallKeyTalk MsiPath ' best effort cleanup, don't check if successfully uninstalled
            WScript.Quit retval
        End If
    End If

    If hasOption(OptionTasksIni) Then
        Dim myFound
        Dim myInstallPath
        getInstallPath myFound, myInstallPath
        If Not(myFound) Then
            WScript.StdErr.WriteLine "Unable to install task configuration because KeyTalk installation directory cannot be found."
            WScript.Quit RetError
        End If
        CmdLine = quote(myInstallPath & "\" & ReseptConfigManagerExecutable) & " --tasks-ini-path " & quote(getOptionValue(OptionTasksIni))
        retval = sho.Run(CmdLine, , true)
        If retval <> 0 Then
            WScript.StdOut.WriteLine "Could not install new task configuration, see " & quote(ConfigManagerLogPath) & " for more details"
            uninstallKeyTalk MsiPath ' best effort cleanup, don't check if successfully uninstalled
            WScript.Quit retval
        End If
    End If

    WScript.Quit RetSuccess
End Function

Function uninstallKeyTalk(anMsiPath)
    WScript.StdOut.Write "Uninstalling KeyTalk..."
    CmdLine = "msiexec /x " & quote(anMsiPath) & " /qn"
    retval = sho.Run(CmdLine, , true) ' Best effort to bring system to initial state
    If retval <> 0 Then
        WScript.StdOut.WriteLine "failed, please uninstall KeyTalk manually"
        uninstallKeyTalk = false
        Exit Function
    End If
    WScript.StdOut.WriteLine "done"
    uninstallKeyTalk = true
End Function

Function enableTasksAtSystemStartup(aUser, aPassword)
    WScript.StdOut.Write "Enabling scheduled task..."
    CmdLine = "schtasks /change /tn " & ScheduledTaskName & " /ru " & quote(aUser) & " /rp " & quote(aPassword)
    retval = sho.Run(CmdLine, 0, true)
    If retval <> 0 Then
        WScript.StdOut.WriteLine "failed"
        enableTasksAtSystemStartup = false
        Exit Function
    End If
    WScript.StdOut.WriteLine "succeeded"
    enableTasksAtSystemStartup = true
End Function

'#####################################################
' Utilities
'#####################################################
Function getArrayLength(myArray)
    myLength = 0
    For i=0 To UBound(myArray)
        If Not(myArray(ItemIn) = Empty) Then
            myLength = myLength + 1
        End If
    Next
    getArrayLength = myLength
End Function

Function quote(anStr)
    quote = Chr(34) & anStr & Chr(34)
End Function

Function isIeRunning()
    isIeRunning = False
    Set processList = GetObject("Winmgmts:").ExecQuery ("Select * from Win32_Process Where name=" & Chr(34) & "iexplore.exe" & Chr(34))
    For Each objProcess in processList
        isIeRunning = True
    Next
End Function

Function isOption(anArg)
    isOption = (InStr(anArg, "--") = 1)
End Function

Function getInstallPath(anOutFound, anOutPath)
    myReseptIni = ReseptIniPath
    Set myRegex = New RegExp
    myRegex.Pattern = "Install[ \t]*=[ \t]*""(.+)"""
    anOutFound = False
    Set f = fso.OpenTextFile(myReseptIni)
    Do Until f.AtEndOfStream
        Set myMatches = myRegex.Execute(f.ReadLine)
        If myMatches.Count = 1 Then
            anOutFound = True
            anOutPath = myMatches(0).SubMatches(0)
            Exit Do
        End If
    Loop
    f.Close
End Function

Function getArgumentCount()
    getArgumentCount = WScript.Arguments.Count
End Function

Function getPositionalArgumentCount()
    myNumPosArgs = 0
    For Each arg In WScript.Arguments
        If isOption(arg) Then
            Exit For
        End If
        myNumPosArgs = myNumPosArgs + 1
    Next
    getPositionalArgumentCount = myNumPosArgs
End Function

Function getKeywordArgumentCount()
    getKeywordArgumentCount = getArgumentCount() - getPositionalArgumentCount(anArgs)
End Function

Const NOTFOUND = -1
Function getArgumentIndex(anArg)
    myArgIndex = 0
    For Each arg In WScript.Arguments
        If arg = anArg Then
            getArgumentIndex = myArgIndex
            Exit Function
        End If
        myArgIndex = myArgIndex + 1
    Next
    getArgumentIndex = NOTFOUND
End Function

Function hasOption(anArg)
    hasOption = getArgumentIndex(anArg) <> NOTFOUND
End Function

Function hasOptionValue(anArg)
    argumentIndex = getArgumentIndex(anArg)
    If (argumentIndex = NOTFOUND) Then
        hasOptionValue = false
        Exit Function
    End If

    valueIndex = argumentIndex + 1
    If valueIndex > getArgumentCount() - 1 Then ' the option was the last option, no value was specified after this option
        hasOptionValue = false
        Exit Function
    End If

    If isOption(WScript.Arguments(valueIndex)) Then ' instead of the expected value we find the next option, so no the option has no value
        hasOptionValue = false
        Exit Function
    End If

    hasOptionValue = true
End Function

Function getOptionValue(anArg)
    position = getArgumentIndex(anArg) + 1 ' option value is one position after the option in the argument list, e.g. '--my-option "myvalue"'
    getOptionValue = WScript.Arguments(position)
End Function

Function printIf(aCondition, aMessage)
    If (aCondition) Then
        WScript.StdOut.WriteLine aMessage
    End If
End Function

Function printUsageAndExitIf(aCondition, aMessage)
    If (aCondition) Then
        If (aRetValue = RetSuccess) Then
            WScript.StdErr.WriteLine aMessage
        Else
            WScript.StdOut.WriteLine aMessage
        End If

        printUsage()
        WScript.Quit RetError
    End If
End Function

Function checkArgumentValuePresence()
    For Each arg In WScript.Arguments
        If isOption(arg) Then
            myFound = False
            For Each opt in FlagOptions
                If arg = opt Then
                    printUsageAndExitIf hasOptionValue(arg),_
                                        "No value allowed after option " & quote(arg)
                    myFound = True
                End If
            Next

            For Each opt in ValueOptions
                If arg = opt Then
                    printUsageAndExitIf Not(hasOptionValue(arg)),_
                                        "Value expected after option " & quote(arg)
                    myFound = True
                End If
            Next

            printUsageAndExitIf Not(myFound),_
                                "Unknown option " & quote(arg)
        End If
    Next
End Function
