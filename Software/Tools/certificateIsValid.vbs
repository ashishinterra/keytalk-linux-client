' This script determines whether a certificate is currently valid according to
' it's validFrom and validTo fields. It supports DER and PEM encoded certificates.
' Usage:
' cscript /nologo certificateValid <certificate file>
If WScript.Arguments.Count <> 1 Then
    WScript.StdErr.WriteLine "Usage: cscript /nologo " & WScript.ScriptName & " <certificate file>."
    WScript.Quit 2
End If

Set oCert = CreateObject("CAPICOM.Certificate")

On Error Resume Next
oCert.Load(WScript.Arguments.Item(0))
If Err.Number <> 0 Then
    WScript.StdErr.WriteLine "Error: specified file does not contain a PEM or DER certificate."
    Err.Clear
    WScript.Quit 1
End If
On Error Goto 0

dateFrom = oCert.ValidFromDate
dateTo = oCert.ValidToDate
dateNow = Now

If dateFrom <= dateNow And dateNow <= dateTo Then
    WScript.StdOut.WriteLine "The certificate is valid"
    WScript.Quit 0
End if

WScript.StdOut.WriteLine "The certificate is NOT valid"
WScript.Quit 0
