/*********************************************************************
*
* (c) Copyright 2016, XWH.
*
* File: $RCSfile: OWlFusionApp.cpp,v $
* ID: $Id: OWlFusionApp.cpp,v 1.2 2016/03/16 16:11:11 rew Exp $
*
* Purpose:
*	OWlFusion program.
*
* Revision Control:
*   16/03/16 v1.2    REW	Support option for 1-colour display.
*
* Previous Changes:
*   15/03/16 1.1    REW	Initial Version.
**********************************************************************/

/*
 * Include files.
 */
/* Standard headers. */
#include "stdafx.h"

/* Library headers. */
#include "SPxLibUtils/SPxGetOpt.h"

/* Our own headers. */
#include "OWlFusionApp.h"
#include "OWlFusionDlg.h"

/*
 * Macros.
 */
#ifdef _DEBUG
#define new DEBUG_NEW
#endif

/*
 * Message map.
 */
BEGIN_MESSAGE_MAP(OWlFusionApp, CWinApp)
END_MESSAGE_MAP()


/*
 * Constants.
 */


/*
 * Types.
 */


/*
 * Public variables.
 */
/* The one and only OWlFusionApp object. */
OWlFusionApp TheApp;


/*********************************************************************
 *
 *	OWlFusionApp class functions.
 *
 *********************************************************************/

/*====================================================================
 *
 * OWlFusionApp::OWlFusionApp
 *	Constructor for application.
 *
 * Params:
 *	None
 *
 * Returns:
 *	Nothing
 *
 * Notes:
 *	Does nothing.  All significant initialisation should be done
 *	in InitInstance() below.
 *
 *===================================================================*/
OWlFusionApp::OWlFusionApp()
{
    /* Nothing to do at present */
    return;
} /* OWlFusionApp::OWlFusionApp() */


/*====================================================================
 *
 * OWlFusionApp::InitInstance
 *	Function to initialise the main application.
 *
 * Params:
 *	None
 *
 * Returns:
 *	FALSE to indicate the application has finished.
 *
 * Notes
 *
 *===================================================================*/
BOOL OWlFusionApp::InitInstance(void)
{
	AfxInitRichEdit2();
    // InitCommonControlsEx() is required on Windows XP if an application
    // manifest specifies use of ComCtl32.dll version 6 or later to enable
    // visual styles.  Otherwise, any window creation will fail.
    // Set this to include all the common control classes you want to use
    // in your application.
    INITCOMMONCONTROLSEX InitCtrls;
    InitCtrls.dwSize = sizeof(InitCtrls);
    InitCtrls.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&InitCtrls);

    /* Initialise the base class. */
    CWinApp::InitInstance();

    /* Enable control containers. */
    AfxEnableControlContainer();

    /* Standard initialization
     * Change the registry key under which our settings are stored
     * You should modify this string to be something appropriate
     * such as the name of your company or organization.
     */
    SetRegistryKey(_T("XWH"));

    /* Initialise GDI+. */
    GdiplusStartup(&m_gdiplusToken, &m_gdiplusStartupInput, NULL);

    /* Initialise SPx. */
    SPxInit();

    /* Initialise license dongle (if present). */
    SPxLicInit();

    /* Parse command line options. */
    int c = 0;
    int dualColour = TRUE;
 //   while((c = getopt(__argc, __argv, "1?")) != -1)
 //   {
	//switch(c)
	//{
	//    case '1':
	//	dualColour = FALSE;
	//	break;
	//    case '?':   /* Fall through. */
	//    default:
	//	fprintf(stderr, "Unknown option (%d).\n", c);
	//	break;
	//}
 //   }

 //   /* Use filename left over (if any) as the name of the config file to
 //    * read in (this allows config files to be dropped onto a shortcut to
 //    * the application, for example).  Use a default name otherwise.
 //    */
 //   const char *configFilename = "OWlFusion.rpi";
 //   if( optind < __argc )
 //   {
	///* Switch to the specified filename. */
	//configFilename = __targv[optind];
 //   }

 //   /* Read in our configuration file, warning about duplicate params. */
 //   SPxLoadConfig(configFilename, NULL, TRUE);

    /*
     * Create our dialog.
     */
    OWlFusionDlg *dlg = new OWlFusionDlg(NULL,dualColour);
    m_pMainWnd = dlg;

    /*
     * Run the dialog, and hence the application.
     */
	INT_PTR nResponse = IDCANCEL;//初始为无效

	EncryptHasp eh;
	if (eh.CheckHaspIsExist())
	{
		nResponse = dlg->DoModal();
	}
	else
	{
		AfxMessageBox(_T("未检测到加密狗！"));
	}

    /* See what happened. */
    if( nResponse == IDOK )
    {
	/* It was dismissed with the OK button (or equivalent).
	 * We don't actually do anything here.
	 */
	; /* Do nothing */
    }
    else if( nResponse == IDCANCEL )
    {
	/* It was dismissed with the Cancel button (or equivalent).
	 * We don't actually do anything here.
	 */
	; /* Do nothing */
    }

    /* Delete the dialog. */
    delete dlg;

    /* Delete everything else. */
    SPxDeleteAllSystem();

    /* At this point, the dialog has been closed so the application has
     * basically finished.  Return FALSE so that it exits rather than
     * starting a message pump.
     */
    return FALSE;

} /* OWlFusionApp::InitInstance() */

/*********************************************************************
 *
 * End of file
 *
 *********************************************************************/
