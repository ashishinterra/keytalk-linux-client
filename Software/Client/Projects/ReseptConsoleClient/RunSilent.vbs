command = ""
For Each arg in WScript.Arguments
    command = command & """" & arg & """ "
Next

Set WshShell = WScript.CreateObject("WScript.Shell")
Return = WshShell.Run(command, 0, true)