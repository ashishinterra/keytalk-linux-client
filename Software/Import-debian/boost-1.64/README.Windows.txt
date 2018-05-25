Instructions for building and installing boost-1.64 with MSVS 2013 for KeyTalk on Windows 7
------------------------------------------------------------------------------------------------

1. Open MSVS command prompt (from Tools > Visual Studio Command Prompt or call C:\Program Files\Microsoft Visual Studio 12.0\Common7\Tools\vsvars32.bat in your command prompt)

2. setup build engine:
  cd <boost-src-dir>\tools\build
  .\bootstrap.bat
  mkdir bjam-inst
  .\b2 --prefix=.\bjam-inst install

3. Build the required boost libraries
  cd ..\..\libs\<lib-name>\build

chrono library:
 ..\..\..\tools\build\bjam-inst\bin\bjam release toolset=msvc-12.0 threading=multi link=static

thread library:
 ..\..\..\tools\build\bjam-inst\bin\bjam release toolset=msvc-12.0 link=static

the rest libraries:
 ..\..\..\tools\build\bjam-inst\bin\bjam release toolset=msvc-12.0 threading=multi link=static stage


4. Copy the resulted libraries along with header files to Import\boost-1.64
   Remove old libs and headers.

5. Change in the value of BOOST_VERSION_DIR in WinClientCommon.props

6. Rebuild KeyTalk and retest KeyTalk client with new boost

