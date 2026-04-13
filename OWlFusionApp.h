/*********************************************************************
*
* (c) Copyright 2016, XWH.
*
* File: $RCSfile: OWlFusionApp.h,v $
* ID: $Id: OWlFusionApp.h,v 1.1 2016/03/15 16:38:07 rew Exp $
*
* Purpose:
*	VTSSer program.
*
*	Main application class.
*
*
* Revision Control:
*   15/03/16 v1.1    REW	Initial Version.
*
* Previous Changes:
**********************************************************************/

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "Resource.h"		// main symbols

/*
 * Define the class that "is" the application.
 *
 * See OWlFusionApp.cpp for the implementation of this class.
 */
class OWlFusionApp : public CWinApp
{
public:
    /* Constructor. */
    OWlFusionApp();

    /* Initialisation. */
    virtual BOOL InitInstance();

protected:
    /* Implementation. */
    DECLARE_MESSAGE_MAP()

private:
    /*
     * Private variables.
     */

    /* GDI+ fields. */
    GdiplusStartupInput m_gdiplusStartupInput;
    ULONG_PTR m_gdiplusToken;
};

/* Extern the actual application handle. */
extern OWlFusionApp TheApp;

/*********************************************************************
*
* End of file
*
**********************************************************************/
