# As a rule all file generation occurs in the GenerateFiles project and all projects depend on GenerateFiles.
# This is especially important when multiple projects directly or indirectly depend on these files.
# Currently there are some exceptions where files are generated locally:
#   * talogger public key embedding
#   * In various UI apps: QT .ui -> _moc.cpp generation

build:	..\..\..\Common\Projects\libreseptcommon\resept\common.h \
		..\librclientcore\rclient\Version.h \
		..\librclientcore\rclient\SettingsDefs.h

rebuild: clean build

clean:
	@echo Cleaning...
	@FOR %I IN (..\..\..\Common\Projects\libreseptcommon\resept\common.h ..\librclientcore\rclient\Version.h ..\librclientcore\rclient\SettingsDefs.h) DO @IF EXIST $(OutDir)%I DEL /F /Q $(OutDir)%I >NUL

# Most of these generated header files have many (indirect) dependencies, so they have been made individual targets to ensure minimal rebuilds.
..\..\..\Common\Projects\libreseptcommon\resept\common.h: ..\..\..\Tools\codegen\gen.py ..\..\..\Common\Projects\libreseptcommon\resept\common.h.templ ..\..\..\Common\common_config.py
	@echo Generating common.h
	@..\..\..\Import\python-2.7\python ..\..\..\Tools\codegen\gen.py --cxx ..\..\..\Common\Projects\libreseptcommon\resept\common.h.templ ..\..\..\Common\common_config.py > ..\..\..\Common\Projects\libreseptcommon\resept\common.h

..\librclientcore\rclient\Version.h: ..\librclientcore\VersionGen.pl ..\..\version
	@echo Generating version.h
	@..\..\..\Import\perl\bin\perl ..\librclientcore\VersionGen.pl ..\..\version > ..\librclientcore\rclient\Version.h

..\librclientcore\rclient\SettingsDefs.h: ..\..\..\Tools\codegen\gen.py ..\librclientcore\rclient\SettingsDefs.h.templ ..\..\settings_config.py
	@echo Generating settings constants
	@..\..\..\Import\python-2.7\python ..\..\..\Tools\codegen\gen.py --cxx ..\librclientcore\rclient\SettingsDefs.h.templ ..\..\settings_config.py > ..\librclientcore\rclient\SettingsDefs.h
