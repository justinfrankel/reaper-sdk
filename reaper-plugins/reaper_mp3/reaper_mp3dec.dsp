# Microsoft Developer Studio Project File - Name="reaper_mp3dec" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=reaper_mp3dec - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "reaper_mp3dec.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "reaper_mp3dec.mak" CFG="reaper_mp3dec - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "reaper_mp3dec - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "reaper_mp3dec - Win32 Nitpicker" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "reaper_mp3dec - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "reaper_mp3dec - Win32 Release Profile" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=xicl6.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "reaper_mp3dec - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "REAPER_WAVE_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /Zd /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "REAPER_WAVE_EXPORTS" /D "USE_ICC" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /map /machine:I386 /out:"../Release/Plugins/reaper_mp3dec.dll" /mapinfo:lines /opt:nowin98
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "reaper_mp3dec - Win32 Nitpicker"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Nitpicker"
# PROP BASE Intermediate_Dir "Nitpicker"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Nitpicker"
# PROP Intermediate_Dir "Nitpicker"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "REAPER_WAVE_EXPORTS" /c /FR /FD /Zi
# ADD CPP /nologo /W3 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "REAPER_WAVE_EXPORTS" /D "USE_ICC" /YX /c /FR /FD /Zi /MT /Ot /Og /D "DEBUG_TIGHT_ALLOC"
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /fixed:no
# ADD LINK32 ../../nitpicker/libcmt_nitpick.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /map /machine:I386 /nodefaultlib:"LIBCMT" /out:"../Nitpicker/Plugins/reaper_mp3dec.dll" /mapinfo:lines /opt:nowin98 /debug
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "reaper_mp3dec - Win32 Debug"
# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "REAPER_WAVE_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "REAPER_WAVE_EXPORTS" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /out:"../Debug/Plugins/reaper_mp3dec.dll" /pdbtype:sept

!ELSEIF  "$(CFG)" == "reaper_mp3dec - Win32 Release Profile"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "reaper_mp3dec___Win32_Release_Profile"
# PROP BASE Intermediate_Dir "reaper_mp3dec___Win32_Release_Profile"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release_Profile"
# PROP Intermediate_Dir "Release_Profile"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /Zd /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "REAPER_WAVE_EXPORTS" /D "USE_ICC" /YX /FD /c
# ADD CPP /nologo /MT /W3 /Zi /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "REAPER_WAVE_EXPORTS" /D "USE_ICC" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /map /machine:I386 /out:"../Release/Plugins/reaper_mp3dec.dll" /mapinfo:lines /opt:nowin98
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /out:"../Release_Profile/Plugins/reaper_mp3dec.dll" /mapinfo:lines /opt:nowin98 /fixed:no
# SUBTRACT LINK32 /pdb:none

!ENDIF 

# Begin Target

# Name "reaper_mp3dec - Win32 Release"
# Name "reaper_mp3dec - Win32 Nitpicker"
# Name "reaper_mp3dec - Win32 Debug"
# Name "reaper_mp3dec - Win32 Release Profile"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "mpglib"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\mpglib\common.cpp
# End Source File
# Begin Source File

SOURCE=.\mpglib\common.h
# End Source File
# Begin Source File

SOURCE=.\mpglib\dct64_i386.cpp
# End Source File
# Begin Source File

SOURCE=.\mpglib\dct64_i386.h
# End Source File
# Begin Source File

SOURCE=.\mpglib\decode_i386.cpp
# End Source File
# Begin Source File

SOURCE=.\mpglib\decode_i386.h
# End Source File
# Begin Source File

SOURCE=.\mpglib\huffman.h
# End Source File
# Begin Source File

SOURCE=.\mpglib\interface.cpp
# End Source File
# Begin Source File

SOURCE=.\mpglib\l2tables.h
# End Source File
# Begin Source File

SOURCE=.\mpglib\layer2.cpp
# End Source File
# Begin Source File

SOURCE=.\mpglib\layer2.h
# End Source File
# Begin Source File

SOURCE=.\mpglib\layer3.cpp
# End Source File
# Begin Source File

SOURCE=.\mpglib\layer3.h
# End Source File
# Begin Source File

SOURCE=.\mpglib\mpglib.h
# End Source File
# Begin Source File

SOURCE=.\mpglib\StdAfx.cpp
# End Source File
# Begin Source File

SOURCE=.\mpglib\tabinit.cpp
# End Source File
# Begin Source File

SOURCE=.\mpglib\tabinit.h
# End Source File
# End Group
# Begin Source File

SOURCE=..\..\WDL\lameencdec.cpp
# End Source File
# Begin Source File

SOURCE=..\..\WDL\lameencdec.h
# End Source File
# Begin Source File

SOURCE=.\mp3_index.cpp
# End Source File
# Begin Source File

SOURCE=.\mp3_index.h
# End Source File
# Begin Source File

SOURCE=.\mp3dec.cpp
# End Source File
# Begin Source File

SOURCE=.\pcmsink_mp3lame.cpp
# End Source File
# Begin Source File

SOURCE=.\pcmsrc_mp3dec.cpp
# End Source File
# Begin Source File

SOURCE=.\res.rc
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\main.h
# End Source File
# Begin Source File

SOURCE=.\mp3dec.h
# End Source File
# Begin Source File

SOURCE=..\reaper_plugin.h
# End Source File
# Begin Source File

SOURCE=.\resource.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# End Target
# End Project
