build: deploylibs

rebuild: clean deploylibs

clean:
	@echo Cleaning...
	@FOR %I IN (Qt5Core.dll Qt5Gui.dll Qt5Widgets.dll platforms\qwindows.dll icudt53.dll icuin53.dll icuuc53.dll libwapi.dll msvcp120.dll msvcr120.dll vccorlib120.dll) DO @IF EXIST $(OutDir)%I DEL /F /Q $(OutDir)%I >NUL
	@IF EXIST $(OutDir)\platforms rd /S/Q $(OutDir)\platforms\ > NUL


deploylibs:
	@IF NOT EXIST "$(OutDir)\" MKDIR "$(OutDir)"
	@echo Deploying VC runtime libs...
	@FOR %I IN (msvcp120.dll msvcr120.dll vccorlib120.dll) DO @COPY "$(VcInstallDir)redist\x86\Microsoft.VC120.CRT\%I" "$(OutDir)" >NUL || (@ECHO "$(VcInstallDir)redist\x86\Microsoft.VC120.CRT\%I" does not exist! & EXIT 1)
	@echo Deploying Qt libs...
	@IF NOT EXIST "$(OutDir)\platforms" MKDIR "$(OutDir)\platforms"
	@COPY $(QTDIR)\plugins\platforms\qwindows.dll "$(OutDir)\platforms" >NUL
	@FOR %I IN (Qt5Core.dll Qt5Gui.dll Qt5Widgets.dll icudt54.dll icuin54.dll icuuc54.dll) DO @COPY $(QTDIR)\bin\%I "$(OutDir)" >NUL || (@ECHO $(QTDIR)\bin\%I does not exist! & EXIT 1)
	@echo Deploying Zlib...
	@copy  ..\..\..\Import\$(ZLIB_LIB_DIR)\zlibwapi.dll "$(OutDir)" >NUL
