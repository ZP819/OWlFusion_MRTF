/*********************************************************************
*
* (c) Copyright 2016, XWH.
*
* File: $RCSfile: OWlFusionDlg.h,v $
* ID: $Id: OWlFusionDlg.h,v 1.4 2016/05/18 14:04:51 rew Exp $
*
* Purpose:
*	OWlFusion program.
*
*	Main dialog class.
*
*
* Revision Control:
*   18/05/16 v1.4    AGC	Support course-up view.
*
* Previous Changes:
*   17/03/16 1.3    REW	Support no-trails and heading-up views.
*   16/03/16 1.2    REW	Support option for single-colour mode.
*   15/03/16 1.1    REW	Initial Version.
**********************************************************************/

#pragma once
#include "afxcmn.h"

/*
 * Headers.
 */
///* Library headers. */
//#include "SPx.h"
//
///* Our headers. */
//#include "Resource.h"
#include "OWlSystemMesgDlg.h"
#include "OWlFuseConfigDlg.h"
#include "OWlDisplayCtrlDlg.h"
#include "OWlRadarPosConfigDlg.h"

/* Forward declare other classes we need. */
class OWlFusionPpi;

/*
 * OWlFusionDlg dialog.
 */
class OWlFusionDlg : public CDialog
{
public:
    /* Construction and destruction. */
    OWlFusionDlg(CWnd *parentWin = NULL, BOOL dualColour=TRUE);
    virtual ~OWlFusionDlg(void);

    /* Dialog Data. */
    enum { IDD = IDD_MAIN_DIALOG };
   

protected:
    /* Protected fields. */
    HICON m_hIcon;

    /* Protected functions. */
    DECLARE_MESSAGE_MAP()
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    void OnSysCommand(UINT nID, LPARAM lParam);
    HCURSOR OnQueryDragIcon(void);
    void OnSize(UINT nType, int cWidth, int cHeight);

    /* Override OK/Cancel to prevent dialog disappearing. */
    virtual void OnOK(void){ /* Don't call the CDialog handler */ };
    virtual void OnCancel(void){ /* Don't call the CDialog handler */ };

private:
    /*
     * Private fields.
     */
    /* Options. */
    BOOL m_dualColour;			/* Flag */

    ///* Navigation data. */
    //SPxNavData *m_navData;		/* Navigation data handling. */

    ///* Tiled map support. */
    //SPxTiledMapDatabase *m_tiledMapDB;	/* Tiled map database */

    ///* Radar objects. */
    //SPxRIB *m_rib;			/* Radar input buffer */
    //SPxPIM *m_pim;			/* Polar interface module */
    //SPxNetworkReceive *m_srcNet;	/* Network radar source */
    //SPxRunProcess *m_baseProcess;	/* To connect others to */

    /* PPI window. */
    OWlFusionPpi *m_ppi;	/* PPI window */

	OWlSystemMesgDlg* m_SystemMesgDlg;
    /*
     * Private functions.
     */
    void resize(int cWidth, int cHeight);

private:
	SPxWinMenu m_menu;
	CStatusBar m_wndStatusBar;


	OWlFuseConfigDlg* m_fuseConfigDlg;
	OWlDisplayCtrlDlg* m_displayCtrlDlg;
	SPxTablePopupDlgWin* m_netInputConfiglDlg;
	SPxTablePopupDlgWin* m_trackFusedDistributeConfiglDlg;
	OWlRadarPosConfigDlg* m_radarPosConfigDlg;

	SPxTablePopupDlgWin* m_radarSensorOverviewTableDlg;
	void initRadarSensorOverviewTableDlg();
	static void iterateInitOverview(OWlRadarSensor* sensor, void* userArg);
	void addRadarSensorOverviewDlg(OWlRadarSensor* sensor);
public:
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	afx_msg void OnMenuRadarPosConfig();
	afx_msg void OnMenuDisplayCtrl();
	afx_msg void OnMenuSaveConfig();
	afx_msg void OnMenuNetworkInputConfig();
	afx_msg void OnMenuFuseConfig();
	afx_msg void OnMenuTrackDistributeConfig();
	afx_msg void OnMenuSystemMesg();
	afx_msg void OnMenuRadarConfig();
 };

/*********************************************************************
*
* End of file
*
**********************************************************************/
