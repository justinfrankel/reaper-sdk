// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__10BCB4FD_A010_4249_A3B0_2F4E2102C712__INCLUDED_)
#define AFX_STDAFX_H__10BCB4FD_A010_4249_A3B0_2F4E2102C712__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "mpglib.h"

#include "common.h"
#include "tabinit.h"
#include "dct64_i386.h"
#include "decode_i386.h"
#include "layer3.h"
#include "layer2.h"
#include "l2tables.h"

#if 0
#include <windows.h>
#include "../../../pfc/profiler.h"
#else
#define profiler(x)
#endif

// TODO: reference additional headers your program requires here

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__10BCB4FD_A010_4249_A3B0_2F4E2102C712__INCLUDED_)
