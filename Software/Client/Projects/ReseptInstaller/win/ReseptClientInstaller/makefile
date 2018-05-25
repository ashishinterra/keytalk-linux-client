WIX_BIN_DIR = ..\..\..\..\..\Import\$(WIX_VERSION_DIR)
PERL_BIN    = ..\..\..\..\..\Import\perl\bin\perl
PYTHON_BIN  = ..\..\..\..\..\Import\python-2.7\python
candle      = $(WIX_BIN_DIR)\candle.exe
light       = $(WIX_BIN_DIR)\light.exe
CANDLE_ARGS =
# Suppress warning 1076: Some action falls between InstallInitialize and RemoveExistingProducts
# Reason for suppression: BackupData and UninstallProviderData actions are placed correctly after InstallInitialize (they change the system) but before RemoveExistingProducts
# See http://msdn.microsoft.com/en-us/library/aa369008%28v=vs.85%29.aspx for more details
LIGHT_ARGS = -wx -sw1076
IN_DIR     = ..\..\..\Export
OUT_DIR     = ..\..\..\Export
!INCLUDE ..\..\..\..\version
OUT_FILE    = KeyTalkClient-$(version).msi

!IFDEF NOSIGN
MakeInstaller: $(OUT_FILE) CopyMsiHelperScripts
!ELSE
SIGNTOOL_PATH = ..\..\..\..\..\Import\win32_utils\signtool.exe
SIGNCERT_PATH = ..\..\..\..\..\CertKeys\CodeSigning\CodesigningWindows.pfx
!INCLUDE ..\..\..\..\..\CertKeys\CodeSigning\CodesigningWindows.pfx.passwd
SIGNCERT_PASSWD = $(PASSWORD)
TIMESTAMP_URL = http://timestamp.globalsign.com/scripts/timstamp.dll
MakeInstaller: SignBinaries $(OUT_FILE) SignMsi CopyMsiHelperScripts
!ENDIF


$(OUT_FILE): ReseptClientInstaller.wixobj
  $(light) $(LIGHT_ARGS) -ext WixUIExtension -ext WixUtilExtension -cultures:en-us  $** /nologo /out $(OUT_DIR)\$@

ReseptClientInstaller.wixobj: ReseptClientInstaller.wxs ReseptClientInstaller.wxi
  $(candle) $(CANDLE_ARGS) ReseptClientInstaller.wxs /nologo
  
ReseptClientInstaller.wxi: VersionGen.pl ReseptClientInstaller.wxi.templ ..\..\..\..\version
  $(PERL_BIN) VersionGen.pl ReseptClientInstaller.wxi.templ ..\..\..\..\version > $@.tmp
  $(PYTHON_BIN) ..\..\..\..\..\Tools\codegen\gen.py --raw $@.tmp ..\..\..\..\..\Common\common_config.py > $@
  @DEL $@.tmp

SignBinaries: $(SIGNTOOL_PATH)
	@$(SIGNTOOL_PATH) sign /f $(SIGNCERT_PATH) /p $(SIGNCERT_PASSWD) /t $(TIMESTAMP_URL) $(OUT_DIR)\ReseptDesktopClient.exe $(OUT_DIR)\ReseptConsoleClient.exe $(OUT_DIR)\ReseptConfigManager.exe $(OUT_DIR)\ReseptIeClient.dll $(OUT_DIR)\ReseptIeBroker.exe $(OUT_DIR)\ReseptBrokerService.exe $(OUT_DIR)\libtalogger.dll $(OUT_DIR)\ConfigUpdater.exe $(OUT_DIR)\MessageBoxManager.exe $(OUT_DIR)\Sweeper.exe $(OUT_DIR)\IeUserExeRestarter.exe $(OUT_DIR)\ReseptPrGenerator.exe

SignMsi: $(SIGNTOOL_PATH)
	@$(SIGNTOOL_PATH) sign /f $(SIGNCERT_PATH) /p $(SIGNCERT_PASSWD) /t $(TIMESTAMP_URL) $(OUT_DIR)\$(OUT_FILE)

CopyMsiHelperScripts: MsiSilentInstall.vbs
    @copy /Y MsiSilentInstall.vbs $(OUT_DIR)\

rebuild: clean MakeInstaller
  
clean:
  @echo Cleaning...
  del *.wixobj >NUL 2>&1
  del ReseptClientInstaller.wxi >NUL 2>&1
  del $(OUT_DIR)\$(OUT_FILE) >NUL 2>&1


  
