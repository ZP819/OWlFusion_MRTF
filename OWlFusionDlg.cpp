/*********************************************************************
*
* (c) Copyright 2016, 2018, XWH.
*
* Purpose:
*	OWlFusion program.
*
*	Main dialog class.
*
*
* Revision Control:
*   29/11/18 v1.5    AGC	High DPI improvements.
*
* Previous Changes:
*   18/05/16 1.4    AGC	Support course-up view.
*   17/03/16 1.3    REW	Support no-trails and heading-up views.
*   16/03/16 1.2    REW	Support option for single-colour mode.
*   15/03/16 1.1    REW	Initial Version.
**********************************************************************/

/*
 * Include files.
 */

/* "Standard" headers */
#include "stdafx.h"

/* Library headers. */

/* Our own headers. */
#include "OWlFusionDlg.h"
#include "OWlFusionPpi.h"

#include "SPxLibWin\SPxAlarmConfigCondDlgWin.h"

const int IDC_STATUSBAR_RADAR_TRACK_NUMS = 10000;
const int IDC_STATUSBAR_AIS_TRACK_NUMS = 10001;
const int IDC_STATUSBAR_FUSED_TRACK_NUMS = 10002;
/*
 * Macros.
 */


/*
 * Constants.
 */


/*
 * Types.
 */


/*
 * Private functions.
 */


/*
 * Global variables.
 */

/*********************************************************************
 *
 *	OWlFusionDlg class.
 *
 *********************************************************************/

/*====================================================================
 *
 * OWlFusionDlg::MESSAGE_MAP
 *	Message map for the dialog.
 *
 *===================================================================*/
void SQLQueryResultFunc(void *userArg,
	SPxSQLDatabase *db,
	int numColumns,
	const char **columnValues,
	const char **columnNames)
{
	/*每一行回调*/
	SPxPacketTrackMinimal min;
	min.id = atoi(columnValues[3]);
	/*ext.norm.min.speedMps = atoi(columnValues[10]);
	ext.norm.min.courseDegrees = atoi(columnValues[11]);*/

	int zp = 2;
}

BEGIN_MESSAGE_MAP(OWlFusionDlg, CDialog)
    ON_WM_SYSCOMMAND()
    ON_WM_QUERYDRAGICON()
    ON_WM_SIZE()
	ON_WM_TIMER()
	ON_COMMAND(ID_MENU_RADAR_POS_CONFIG, &OWlFusionDlg::OnMenuRadarPosConfig)
	ON_COMMAND(ID_MENU_DISPLAY_CTRL, &OWlFusionDlg::OnMenuDisplayCtrl)
	ON_COMMAND(ID_MENU_SAVE_CONFIG, &OWlFusionDlg::OnMenuSaveConfig)
	ON_COMMAND(ID_MENU_NETWORK_INPUT_CONFIG, &OWlFusionDlg::OnMenuNetworkInputConfig)
	ON_COMMAND(ID_MENU_FUSE_CONFIG, &OWlFusionDlg::OnMenuFuseConfig)
	ON_COMMAND(ID_MENU_TRACK_DISTRIBUTE_CONFIG, &OWlFusionDlg::OnMenuTrackDistributeConfig)
	ON_COMMAND(ID_MENU_SYSTEM_MESG, &OWlFusionDlg::OnMenuSystemMesg)
	ON_COMMAND(ID_MENU_RADAR_CONFIG, &OWlFusionDlg::OnMenuRadarConfig)
END_MESSAGE_MAP()


/*====================================================================
 *
 * OWlFusionDlg::OWlFusionDlg
 *	Constructor for the main dialog class.
 *
 * Params:
 *	parentWin		Parent window,
 *	dualColour		TRUE for dual-colour radar/trails,
 *				FALSE for single-colour.
 *
 * Returns:
 *	Nothing
 *
 * Notes
 *
 *===================================================================*/
OWlFusionDlg::OWlFusionDlg(CWnd* parentWin,
						    BOOL dualColour)
	: CDialog(OWlFusionDlg::IDD, parentWin)
{
    /* Set up the icon. */
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

    /*
     * Initialise things.
     */
    /* Options. */
    m_dualColour = dualColour;

    /* PPI window. */
    m_ppi = NULL;


} /* OWlFusionDlg::OWlFusionDlg */


/*====================================================================
 *
 * OWlFusionDlg::~OWlFusionDlg
 *	Destructor for the main dialog class.
 *
 * Params:
 *	None
 *
 * Returns:
 *	Nothing
 *
 * Notes
 *
 *===================================================================*/
OWlFusionDlg::~OWlFusionDlg(void)
{
	//EndDialog(0);//神器

    /* Delete all objects created by us. */
	delete m_ppi->GetFusion();

   SPxChannelDatabase* channelDb = m_ppi->GetChannelDb();
   if (channelDb)
   {
	   channelDb->DeleteAllChannels();
   }
 
   SPxObj::DeleteAllObjectsOfType("OWlRadarSensorRender", 0);
   
    SPxDeleteAll();

} /* OWlFusionDlg::~OWlFusionDlg */


/*====================================================================
 *
 * OWlFusionDlg::DoDataExchange
 *	Data exchange function for the main dialog (ensures GUI and
 *	member variables are consistent).
 *
 * Params:
 *	pDX		Standard data exchange pointer.
 *
 * Returns:
 *	Nothing
 *
 * Notes
 *
 *===================================================================*/
void OWlFusionDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
} /* OWlFusionDlg::DoDataExchange */

/*====================================================================
 *
 * OWlFusionDlg::OnInitDialog
 *	Function to initialise the dialog, basically creating all the
 *	objects we need and setting up the GUI.
 *
 * Params:
 *	None
 *
 * Returns:
 *	FALSE if we set the input focus to a control,
 *	TRUE if not.
 *
 * Notes
 *	Called by framework in response to WM_INITDIALOG message
 *	(probably during the DoModal() call to the object).
 *
 *===================================================================*/
BOOL OWlFusionDlg::OnInitDialog()
{
    /* Initialise the base class. */
    CDialog::OnInitDialog();

    /* Set the icon for this dialog.  The framework does this automatically
     * when the application's main window is not a dialog.
     */
    SetIcon(m_hIcon, TRUE);		/* Set big icon. */
    SetIcon(m_hIcon, FALSE);		/* Set small icon. */

	SetWindowText(_T("雷达目标组网\t"));

	m_menu.LoadMenuW(IDR_MENU_MAIN);
	SetMenu(&m_menu);

	m_wndStatusBar.Create(this);
	// 设置状态栏面板
	UINT indicators[] = { 
		IDC_STATUSBAR_RADAR_TRACK_NUMS
		,IDC_STATUSBAR_AIS_TRACK_NUMS
		,IDC_STATUSBAR_FUSED_TRACK_NUMS };
	m_wndStatusBar.SetIndicators(indicators, sizeof(indicators) / sizeof(UINT));
	m_wndStatusBar.SetPaneInfo(0, indicators[0], SBPS_NORMAL, SPxGetDpiScaledSize(100));
	m_wndStatusBar.SetPaneText(0, CString(_T("雷达航迹 0")));
	m_wndStatusBar.SetPaneInfo(1, indicators[1], SBPS_NORMAL, SPxGetDpiScaledSize(100));
	m_wndStatusBar.SetPaneText(1, CString(_T("AIS 0")));
	m_wndStatusBar.SetPaneInfo(2, indicators[2], SBPS_NORMAL, SPxGetDpiScaledSize(100));
	m_wndStatusBar.SetPaneText(2, CString(_T("融合航迹 0")));

    /*
     * Create the window to show the radar data (AFTER creating other objs).
     */
	m_SystemMesgDlg = new OWlSystemMesgDlg();
	m_SystemMesgDlg->Create(IDD_DIALOG_SYSTEM_MESG);
	

	
	

	

    m_ppi = new OWlFusionPpi(this);
    m_ppi->Create(m_ppi->IDD);
    m_ppi->ShowWindow(SW_SHOW);

	
	//OWlEncryptFile ef;
	//ef.DeyptFile("lae.csv");
	//SPxLanguageDlgWin* ld = new SPxLanguageDlgWin(NULL, NULL);
	//if (SPxIsFile(OWL_LANG_FILE_NAME))
	//{
	//	SPxErrorCode er = ld->LoadLanguages(OWL_LANG_FILE_NAME, "Chinese", SPX_LANG_ENCODING_AUTO);
	//	ef.EnyptFile("la.csv");
	//}
	//const char* ssaa = SPxGetLangString("NetCtrlDlg5", "bbb");
	//SPxT8toT16  a(ssaa);
	//CString ssf = CString(a);

	

	
	/* 网络输入 */
	const unsigned int tableColIndex = 0;//即：共有1列
	unsigned int tableRowIndex = 0;
	unsigned int colSpan = 1;//一个控件的列宽
	m_netInputConfiglDlg = new SPxTablePopupDlgWin(this);
	m_netInputConfiglDlg->Create();
	CWnd* pParent = m_netInputConfiglDlg->GetParent();
	if (pParent)
	{
		HICON smallIco = pParent->GetIcon(0);
		HICON bigIco = pParent->GetIcon(1);
		m_netInputConfiglDlg->SetIcon(smallIco, 0);
		m_netInputConfiglDlg->SetIcon(bigIco, 1);
	}
	m_netInputConfiglDlg->SetWindowTextW(_T("AIS网络输入设置\n"));
	SPxNetCtrlDlgWin* AISNetCtrlDlg = new SPxNetCtrlDlgWin(m_netInputConfiglDlg, m_ppi->GetAISsDecoder()->GetSrcNetAddrWrapper());
	AISNetCtrlDlg->Create();
	m_netInputConfiglDlg->Add(tableRowIndex, tableColIndex, AISNetCtrlDlg, colSpan, "AIS网络\n", 1);
	tableRowIndex++;
	//SPxDialogPopupShow(m_netInputConfiglDlg);

	
	m_fuseConfigDlg = new OWlFuseConfigDlg(m_ppi->GetFusion());
	m_fuseConfigDlg->Create(IDD_DIALOG_FUSE_CONFIG);
	

	m_displayCtrlDlg = new OWlDisplayCtrlDlg(
		m_ppi->GetUniTrackRenderer(),
		m_ppi->GetTiledMapRenderer(),
		m_ppi->GetRadarSensorManager());
	m_displayCtrlDlg->Create(IDD_DIALOG_DISPLAY_CTRL);


	initRadarSensorOverviewTableDlg();
	


	
	

	/* 网络输出 */
	tableRowIndex = 0;
    m_trackFusedDistributeConfiglDlg = new SPxTablePopupDlgWin(this);
	m_trackFusedDistributeConfiglDlg->Create();
	pParent = m_trackFusedDistributeConfiglDlg->GetParent();
	if (pParent)
	{
		HICON smallIco = pParent->GetIcon(0);
		HICON bigIco = pParent->GetIcon(1);
		m_trackFusedDistributeConfiglDlg->SetIcon(smallIco, 0);
		m_trackFusedDistributeConfiglDlg->SetIcon(bigIco, 1);
	}
	m_trackFusedDistributeConfiglDlg->SetWindowTextW(_T("融合航迹输出"));
	SPxNetCtrlDlgWin* fusedTrackNetCtrlDlg = new SPxNetCtrlDlgWin(m_trackFusedDistributeConfiglDlg, m_ppi->GetFusion()->GetTrackFusedSender());
	fusedTrackNetCtrlDlg->Create();
	m_trackFusedDistributeConfiglDlg->Add(tableRowIndex, tableColIndex, fusedTrackNetCtrlDlg, colSpan, "网络设置\n", 1);
	m_trackFusedDistributeConfiglDlg->SyncFromObj();
	m_trackFusedDistributeConfiglDlg->Layout();


	m_radarPosConfigDlg = new OWlRadarPosConfigDlg(m_ppi->GetWorldMap());
	m_radarPosConfigDlg->Create(IDD_DIALOG_RADAR_POS);

    /* Resize to set initial component positions. */
    CRect rect;
    this->GetClientRect(&rect);
    resize(rect.Width(), rect.Height());

	SetTimer(1, 1000, NULL);

    /* Return TRUE, unless you set the focus to a control. */
    return TRUE;
} /* OWlFusionDlg::OnInitDialog */


void OWlFusionDlg::initRadarSensorOverviewTableDlg()
{
	/* 网络输入 */
	const unsigned int tableColIndex = 0;//即：共有1列
	unsigned int tableRowIndex = 0;
	unsigned int colSpan = 1;//一个控件的列宽
	m_radarSensorOverviewTableDlg = new SPxTablePopupDlgWin(this);
	m_radarSensorOverviewTableDlg->Create();
	CWnd* pParent = m_radarSensorOverviewTableDlg->GetParent();
	if (pParent)
	{
		HICON smallIco = pParent->GetIcon(0);
		HICON bigIco = pParent->GetIcon(1);
		m_radarSensorOverviewTableDlg->SetIcon(smallIco, 0);
		m_radarSensorOverviewTableDlg->SetIcon(bigIco, 1);
	}
	m_radarSensorOverviewTableDlg->SetWindowTextW(_T("雷达站点概览\n"));

	m_ppi->GetRadarSensorManager()->IterateSensors(iterateInitOverview, this);

	m_radarSensorOverviewTableDlg->SyncFromObj();
	m_radarSensorOverviewTableDlg->Layout();
	SPxDialogPopupShow(m_radarSensorOverviewTableDlg);
}


void OWlFusionDlg::iterateInitOverview(OWlRadarSensor* sensor, void* userArg)
{
	OWlFusionDlg* dlg = (OWlFusionDlg*)userArg;
	dlg->addRadarSensorOverviewDlg(sensor);
}

void OWlFusionDlg::addRadarSensorOverviewDlg(OWlRadarSensor* sensor)
{
	const unsigned int tableColIndex = 0;
	static unsigned int tableRowIndex = 0;
	unsigned int colSpan = 1;//一个控件的列宽

	
	OWlRadarSensorOverviewDlg * radarSensorOverviewDlg = new OWlRadarSensorOverviewDlg(sensor);
	radarSensorOverviewDlg->Create(IDD_DIALOG_RADAR_SENSOR_OVERVIEW);
	char buf[1024] = { 0 };
	char name[1024] = { 0 };
	sensor->GetChannelNetRadar()->GetDisplayName(name, sizeof(name));
	sprintf_s(buf, "站点%d: %s", sensor->getSensorId(), name);
	m_radarSensorOverviewTableDlg->Add(tableRowIndex, tableColIndex, radarSensorOverviewDlg, colSpan, buf, 1);
	tableRowIndex++;
}

/*====================================================================
*
* OWlFusionDlg::OnSysCommand
*	Function to handle user actions via the control menu.
*
* Params:
*	nID		Type of system command requested (although bottom
*			four bits are reserved, so mask them out),
*	lParam		Cursor corrdinates or other parameter.
*
* Returns:
*	Nothing
*
* Notes
*
*===================================================================*/
void OWlFusionDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
    /* See if the ID is one we are supposed to handle ourselves. */
    if( (nID & 0xFFF0) == SC_CLOSE )
    {
	/* The user wants to close the application, probably by pressing
	 * the X in the top right of the window.  Normally, this would seem
	 * to be handled automatically and invoke OnCancel() to close the
	 * dialog, but since we've overridden that to avoid closing the
	 * dialog when Escape is pressed (which also invokes OnCancel()),
	 * we need to manually call the base class OnCancel() rather than
	 * our own, so that the dialog exits.
	 */
	CDialog::OnCancel();
    }
    else
    {
	/* Pass it to the base class. */
	CDialog::OnSysCommand(nID, lParam);
    }
    return;
} /* OWlFusionDlg::OnSysCommand() */


/*====================================================================
 *
 * OWlFusionDlg::OnQueryDragIcon
 *       The system calls this function to obtain the cursor to 
 *       display while the user drags the minimized window.
 *
 * Params:
 *	None
 *
 * Returns:
 *	Cursor.
 *
 * Notes
 *
 *===================================================================*/
HCURSOR OWlFusionDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
} /* OWlFusionDlg::OnQueryDragIcon() */


/*====================================================================
 *
 * OWlFusionDlg::OnSize
 *	Function to handle the WM_SIZE message from the framework.
 *
 * Params:
 *	 nType       Type of resizing requested.
 *       cWidth      New client width.
 *       cHeight     New client height.
 *
 * Returns:
 *	Nothing
 *
 * Notes
 *
 *===================================================================*/
void OWlFusionDlg::OnSize(UINT nType, int cWidth, int cHeight)
{
    /* Call the base class function. */
    CDialog::OnSize(nType, cWidth, cHeight);

    /* Resize the dialog. */
    resize(cWidth, cHeight);
} /* OWlFusionDlg::OnSize() */


/*====================================================================
 *
 * OWlFusionDlg::resize
 *	Function to resize the dialog.
 *
 * Params:
 *       cWidth      New client width.
 *       cHeight     New client height.
 *
 * Returns:
 *	Nothing
 *
 * Notes
 *
 *===================================================================*/
void OWlFusionDlg::resize(int cWidth, int cHeight)
{
    /* Sanity check. */
    if( m_ppi == NULL )
    {
        /* Nothing to do. */
        return;
    }

    /* Padding between components. */
    unsigned int pad = SPxGetDpiScaledSize(5U);

   // /* Get the size of the control area. */
   // CRect ctrlRect;
   // GetDlgItem(IDC_MOTION_GROUPBOX)->GetWindowRect(&ctrlRect);
   //                       
   // /* Position the underlay window below the controls and 
   //  * resize to fill the remaining space in the dialog.
   //  */
   // m_ppi->MoveWindow(pad, 
			//ctrlRect.Height() + (2 * pad),
			//cWidth - (2 * pad),
			//cHeight - ctrlRect.Height() - (3 * pad),
			//TRUE);
			/* Get the size of the control area. */
	

	/* Position the underlay window below the controls and
	* resize to fill the remaining space in the dialog.
	*/
	m_ppi->MoveWindow(pad,
		(1 * pad),
		cWidth - (2 * pad),
		cHeight - (5 * pad),
		TRUE);

	RepositionBars(AFX_IDW_CONTROLBAR_FIRST, AFX_IDW_CONTROLBAR_LAST, IDC_STATUSBAR_RADAR_TRACK_NUMS);

    /* Done. */
    return;
} /* OWlFusionDlg::resize() */





void OWlFusionDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	SPxUniTrackDatabase* uniTrackDb = m_ppi->GetUniTrackDatabase();
	
	unsigned int numRadar,numAIS,fusedTrackNums;
	int ret = uniTrackDb->GetNumTracks(&numRadar,&numAIS, NULL,&fusedTrackNums);


	CString str;
	str.Format(_T("雷达航迹 %d"), numRadar);
	m_wndStatusBar.SetPaneText(0, str);
	str.Format(_T("组网航迹 %d"), fusedTrackNums);
	m_wndStatusBar.SetPaneText(1, str);
	str.Format(_T("AIS %d"), numAIS);
	m_wndStatusBar.SetPaneText(2, str);

	EncryptHasp en;
	if (!en.CheckHaspIsExist())
	{
		OWlError(OWL_ERR_INFO, "未检测到加密狗，软件即将退出!");
		KillTimer(nIDEvent);
		if (AfxGetMainWnd())
			AfxGetMainWnd()->SendMessageW(WM_CLOSE);
	}

	CDialog::OnTimer(nIDEvent);
}


void OWlFusionDlg::OnMenuRadarPosConfig()
{
	SPxDialogPopupShow(m_radarPosConfigDlg);
}


void OWlFusionDlg::OnMenuDisplayCtrl()
{
	SPxDialogPopupShow(m_displayCtrlDlg);
}


void OWlFusionDlg::OnMenuSaveConfig()
{
	OWlEncryptFile ef;
	ef.DeyptFile("confige.ini");

	if (!SPxIsFile(ConfigFileName))
	{
		CString FilePathName;
		char path[MAX_PATH];
		GetCurrentDirectoryA(MAX_PATH, path);
		char fullPath[MAX_PATH];
		sprintf_s(fullPath, "%s\\%s", path, ConfigFileName);
		CStdioFile file;
		file.Open(CString(fullPath), CFile::modeCreate | CFile::modeReadWrite);
		file.Close();

		SPxLoadConfig(ConfigFileName);
	}

	SPxObj::SetConfigFromStateAll();
	int ret = SPxWriteConfig();
	if (ret == SPX_NO_ERROR)
	{
		OWlError(OWL_ERR_INFO, "保存配置文件成功");
	}
	else
	{
		OWlError(OWL_ERR_ERROR, "保存配置文件失败");
	}
	SPxRemoveFileHistory(ConfigFileName);

	ef.EnyptFile((char*)ConfigFileName);
}


void OWlFusionDlg::OnMenuNetworkInputConfig()
{
	SPxDialogPopupShow(m_netInputConfiglDlg);
}

void OWlFusionDlg::OnMenuRadarConfig()
{
	SPxDialogPopupShow(m_radarSensorOverviewTableDlg);
}


void OWlFusionDlg::OnMenuFuseConfig()
{
	SPxDialogPopupShow(m_fuseConfigDlg);
}


void OWlFusionDlg::OnMenuTrackDistributeConfig()
{
	SPxDialogPopupShow(m_trackFusedDistributeConfiglDlg);
}


void OWlFusionDlg::OnMenuSystemMesg()
{
	SPxDialogPopupShow(m_SystemMesgDlg);
}


