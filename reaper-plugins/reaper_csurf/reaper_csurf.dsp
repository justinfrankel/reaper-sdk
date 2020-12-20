# Microsoft Developer Studio Project File - Name="reaper_csurf" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=reaper_csurf - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "reaper_csurf.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "reaper_csurf.mak" CFG="reaper_csurf - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "reaper_csurf - Win32 Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "reaper_csurf - Win32 Nitpicker" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "reaper_csurf - Win32 Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=xicl6.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "reaper_csurf - Win32 Release"

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
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "REAPER_CSURF_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /Zd /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "REAPER_CSURF_EXPORTS" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /dll /map /machine:I386 /out:"../Release/Plugins/reaper_csurf.dll" /mapinfo:lines /opt:nowin98
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "reaper_csurf - Win32 Nitpicker"

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
# ADD BASE CPP /nologo /MT /W3 /GX /Zi /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "REAPER_CSURF_EXPORTS" /FR /FD /c
# ADD CPP /nologo /MT /W3 /Zi /Ot /Og /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "REAPER_CSURF_EXPORTS" /D "DEBUG_TIGHT_ALLOC" /FR /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /fixed:no
# ADD LINK32 ../../nitpicker/libcmt_nitpick.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /dll /map /debug /machine:I386 /nodefaultlib:"LIBCMT" /out:"../Nitpicker/Plugins/reaper_csurf.dll" /mapinfo:lines /opt:nowin98
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "reaper_csurf - Win32 Debug"

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
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "REAPER_CSURF_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "REAPER_CSURF_EXPORTS" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=xilink6.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /dll /debug /machine:I386 /out:"../Debug/Plugins/reaper_csurf.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "reaper_csurf - Win32 Release"
# Name "reaper_csurf - Win32 Nitpicker"
# Name "reaper_csurf - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Group "csurfs"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\csurf_01X.cpp
# End Source File
# Begin Source File

SOURCE=.\csurf_alphatrack.cpp
# End Source File
# Begin Source File

SOURCE=.\csurf_babyhui.cpp
# End Source File
# Begin Source File

SOURCE=.\csurf_bcf2000.cpp
# End Source File
# Begin Source File

SOURCE=.\csurf_faderport.cpp
# End Source File
# Begin Source File

SOURCE=.\csurf_mcu.cpp
# End Source File
# Begin Source File

SOURCE=.\csurf_osc.cpp
# End Source File
# Begin Source File

SOURCE=.\csurf_tranzport.cpp
# End Source File
# Begin Source File

SOURCE=.\csurf_www.cpp
# End Source File
# End Group
# Begin Group "jnetlib"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\WDL\jnetlib\asyncdns.cpp
# End Source File
# Begin Source File

SOURCE=..\..\WDL\jnetlib\connection.cpp
# End Source File
# Begin Source File

SOURCE=..\..\WDL\jnetlib\httpserv.cpp
# End Source File
# Begin Source File

SOURCE=..\..\WDL\jnetlib\listen.cpp
# End Source File
# Begin Source File

SOURCE=..\..\WDL\jnetlib\util.cpp
# End Source File
# Begin Source File

SOURCE=..\..\WDL\jnetlib\webserver.cpp
# End Source File
# End Group
# Begin Source File

SOURCE=.\csurf.h
# End Source File
# Begin Source File

SOURCE=.\csurf_main.cpp
# End Source File
# Begin Source File

SOURCE=.\osc.cpp
# End Source File
# Begin Source File

SOURCE=.\osc.h
# End Source File
# Begin Source File

SOURCE=.\osc_message.cpp
# End Source File
# Begin Source File

SOURCE=.\res.rc
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
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
