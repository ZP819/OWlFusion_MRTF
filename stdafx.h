// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently

#pragma once

#ifndef _SECURE_ATL
#define _SECURE_ATL 1
#endif

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers
#endif

// Modify the following defines if you have to target a platform prior to the ones specified below.
// Refer to MSDN for the latest info on corresponding values for different platforms.
#ifndef WINVER				// Allow use of features specific to Windows XP or later.
#define WINVER 0x0501		// Change this to the appropriate value to target other versions of Windows.
#endif

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif						

#ifndef _WIN32_WINDOWS		// Allow use of features specific to Windows 98 or later.
#define _WIN32_WINDOWS 0x0410 // Change this to the appropriate value to target Windows Me or later.
#endif

#ifndef _WIN32_IE			// Allow use of features specific to IE 6.0 or later.
#define _WIN32_IE 0x0600	// Change this to the appropriate value to target other versions of IE.
#endif

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS	// some CString constructors will be explicit

// turns off MFC's hiding of some common and often safely ignored warning messages
#define _AFX_ALL_WARNINGS

#ifndef NO_WARN_MBCS_MFC_DEPRECATION
#define NO_WARN_MBCS_MFC_DEPRECATION
#endif

#pragma warning(push)
#pragma warning(disable: 4068 4996)

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxdisp.h>        // MFC Automation classes

#ifndef _AFX_NO_OLE_SUPPORT
#include <afxdtctl.h>		// MFC support for Internet Explorer 4 Common Controls
#endif
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT

#pragma warning(pop)

/* Include GDI+ support */
#include <gdiplus.h>
using namespace Gdiplus;
#pragma comment(lib, "gdiplus.lib")

#pragma comment ( lib,"NRPClient.lib" ) //SPxUniChannelDatabase依赖


#include "SPx.h"

/* Our headers. */
#include "Resource.h"
#include "SPxLibWin\SPxTablePopupDlgWin.h"

#include "CoordinateTransformation.h"
#include <afxcontrolbars.h>

#include <string>
#include <fstream>
#include <algorithm>
#include <vector>
using namespace std;

struct FusedTrackUserData
{
	/*UINT8 sensorNums = 0;
	UINT32 trackUniqueID[8] = { 0 };*/
	vector<UINT32> trackVec; /* UniqueID */
};

#include "EncryptHasp.h"
#include "VMProtectSDK.h"
#include "OWlEncryptFile.h"

#include "OWlRadarTracksFusion.h"

#include "OWlRadarSensorManager.h"

#include "OWlRadarSensorViewConfigDlg.h"
#include "OWlRadarSensorOverviewDlg.h"
#include "OWlRadarSensorRender.h"

#include "OWlUniTrackRender.h"

/* math */
static const double EPS = 1e-6;//double型比较大小的精度
static const double PI = 3.14159265358979323846264338328;

static const char* ConfigFileName = "config.ini";
static const char* OWL_LANG_FILE_NAME = "la.csv";

enum OWlErrorType
{
	OWL_ERR_ERROR,
	OWL_ERR_WARNING,
	OWL_ERR_INFO,
};
extern void OWlError(OWlErrorType errType, const char* msg);

/* 20221215 通过构造和析构保护一段代码 */
class OWlVMProtect
{
public:
	OWlVMProtect(const char* markerName)
	{
		VMProtectBeginUltra(markerName);
	}

	~OWlVMProtect()
	{

		VMProtectEnd();
	}
};


struct ChannelRadarTracksUserData
{
	SPxTrackRendererWin* trackRender = NULL;
};
struct ChannelNetRadarUserData
{
	SPxScSourceLocal *sc = NULL;		/* Scan converter */
	SPxScDestDisplayWin *dispWin = NULL;/* Destination for scan conversion */
	SPxRunProcess *scProcess = NULL;	/* Process for scan converter */
};

#define OWL_MAX_SENSOR_NUMS 10

#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif



