// OWlDisplayCtrlDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "OWlDisplayCtrlDlg.h"
#include "afxdialogex.h"


// OWlDisplayCtrlDlg 对话框

IMPLEMENT_DYNAMIC(OWlDisplayCtrlDlg, CDialogEx)

OWlDisplayCtrlDlg::OWlDisplayCtrlDlg(
	SPxUniTrackRenderer* uniTrackRender
	, SPxTiledMapRendererWin * tiledMapRenderer
	, OWlRadarSensorManager* radarSensorMngr
	,CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_DIALOG_DISPLAY_CTRL, pParent)
{
	m_uniTrackRender = uniTrackRender;
	m_tiledMapRenderer = tiledMapRenderer;
	m_radarSensorMngr = radarSensorMngr;
}

OWlDisplayCtrlDlg::~OWlDisplayCtrlDlg()
{
}

void OWlDisplayCtrlDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_CHECK_MAP, m_map_checkbox);
	DDX_Control(pDX, IDC_CHECK_RADAR_VIDEO, m_radarVideo_checkbox);
	DDX_Control(pDX, IDC_CHECK_RADAR_TRACK, m_radarTrack_checkbox);
	DDX_Control(pDX, IDC_CHECK_FUSED_TRACK, m_fusedTrack_checkbox);
	DDX_Control(pDX, IDC_CHECK_AIS, m_ais_checkbox);
	DDX_Control(pDX, IDC_STATIC_TRAIL_SECS_LABEL, m_trailSecs_label);
	DDX_Control(pDX, IDC_COMBO_TRAIL_SECS, m_trailSecs_combox);
	DDX_Control(pDX, IDC_TREE_RADAR_TRACKS, m_radarTracks_treeCtrl);
	DDX_Control(pDX, IDC_BUTTON_RADAR_SENSOR_VIEW_CONFIG, m_radarSensorViewConfig_btn);
}


BEGIN_MESSAGE_MAP(OWlDisplayCtrlDlg, CDialogEx)
	ON_BN_CLICKED(IDC_CHECK_MAP, &OWlDisplayCtrlDlg::OnBnClickedCheckMap)
	ON_BN_CLICKED(IDC_CHECK_RADAR_VIDEO, &OWlDisplayCtrlDlg::OnBnClickedCheckRadarVideo)
	ON_BN_CLICKED(IDC_CHECK_RADAR_TRACK, &OWlDisplayCtrlDlg::OnBnClickedCheckRadarTrack)
	ON_BN_CLICKED(IDC_CHECK_FUSED_TRACK, &OWlDisplayCtrlDlg::OnBnClickedCheckFusedTrack)
	ON_BN_CLICKED(IDC_CHECK_AIS, &OWlDisplayCtrlDlg::OnBnClickedCheckAis)
	//ON_NOTIFY(NM_CLICK, IDC_TREE_RADAR_TRACKS, &OWlDisplayCtrlDlg::OnNMClickTreeRadarTracks)

	ON_CBN_SELCHANGE(IDC_COMBO_TRAIL_SECS, &OWlDisplayCtrlDlg::OnCbnSelchangeComboTrailSecs)
	ON_BN_CLICKED(IDC_BUTTON_RADAR_SENSOR_VIEW_CONFIG, &OWlDisplayCtrlDlg::OnBnClickedButtonRadarSensorViewConfig)
END_MESSAGE_MAP()


// OWlDisplayCtrlDlg 消息处理程序


void OWlDisplayCtrlDlg::OnOK()
{
	// TODO: 在此添加专用代码和/或调用基类

	//CDialogEx::OnOK();
}


BOOL OWlDisplayCtrlDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	CWnd* pParent = GetParent();
	if (pParent)
	{
		HICON smallIco = pParent->GetIcon(0);
		HICON bigIco = pParent->GetIcon(1);
		SetIcon(smallIco, 0);
		SetIcon(bigIco, 1);
	}

	m_map_checkbox.SetCheck(1);
	m_radarVideo_checkbox.SetCheck(1);
	m_radarTrack_checkbox.SetCheck(1);
	m_fusedTrack_checkbox.SetCheck(1);
	m_ais_checkbox.SetCheck(1);

	//HTREEITEM hRoot = m_radarTracks_treeCtrl.InsertCheckboxItem(_T("雷达航迹\n"));
	//m_radarSensorMngr->IterateSensors(iterateInitTracksTreeCtrl, &m_radarTracks_treeCtrl);
	///*m_radarTracks_treeCtrl.InsertCheckboxItem(_T("1站点\n"), hRoot);
	//m_radarTracks_treeCtrl.InsertCheckboxItem(_T("2站点\n"), hRoot);*/
	//m_radarTracks_treeCtrl.ExpandAll(true);
	///*HTREEITEM hRoot = m_radarTracks_treeCtrl.InsertItem(_T("雷达航迹\n"));
	//m_radarTracks_treeCtrl.SetItemState(hRoot, 0, TVIS_STATEIMAGEMASK);
	//m_radarTracks_treeCtrl.InsertItem(_T("1站点\n"), hRoot);
	//m_radarTracks_treeCtrl.InsertItem(_T("2站点\n"), hRoot);*/
	SPxComboBoxAppendEntry(&m_trailSecs_combox, "none", 0);
	SPxComboBoxAppendEntry(&m_trailSecs_combox, "1 min", 60);
	SPxComboBoxAppendEntry(&m_trailSecs_combox, "5 min", 60*5);
	SPxComboBoxAppendEntry(&m_trailSecs_combox, "10 min", 60*10);
	SPxComboBoxAppendEntry(&m_trailSecs_combox, "30 min", 60*30);
	SPxComboBoxSetFromVal(&m_trailSecs_combox, 60);
	updateTrailLengthSecs(60);

	//管理显示、配置等对话框
	m_radarSensorsTableDlg = new SPxTablePopupDlgWin(this);
	m_radarSensorsTableDlg->Create();
	pParent = m_radarSensorsTableDlg->GetParent();
	if (pParent)
	{
		HICON smallIco = pParent->GetIcon(0);
		HICON bigIco = pParent->GetIcon(1);
		m_radarSensorsTableDlg->SetIcon(smallIco, 0);
		m_radarSensorsTableDlg->SetIcon(bigIco, 1);
	}
	m_radarSensorsTableDlg->SetWindowTextW(_T("雷达显示设置\n"));
	//新增ViewConfigDlg
	m_radarSensorMngr->IterateSensors(iterateInitViewConfig, this);
	m_radarSensorsTableDlg->SyncFromObj();
	m_radarSensorsTableDlg->Layout();



	initLayout();

	return TRUE;  // return TRUE unless you set the focus to a control
				  // 异常: OCX 属性页应返回 FALSE
}

void OWlDisplayCtrlDlg::initLayout()
{
	m_layout = new SPxTableLayoutWin();
	m_layout->SetDialog(this);
	m_layout->SetControl(this);


	unsigned int curRow = 0;
	m_layout->Add(curRow, 0, &m_map_checkbox, 2);
	curRow++;
	m_layout->Add(curRow, 0, &m_ais_checkbox);
	curRow++;
	/*m_layout->Add(curRow, 0, &m_radarVideo_checkbox);
	curRow++;
	m_layout->Add(curRow, 0, &m_radarTrack_checkbox);
	curRow++;*/
	m_layout->Add(curRow, 0, &m_fusedTrack_checkbox);
	curRow++;
	m_layout->Add(curRow, 0, &m_radarSensorViewConfig_btn);
	curRow++;
	m_layout->Add(curRow, 0, &m_trailSecs_label);
	m_layout->Add(curRow, 1, &m_trailSecs_combox);
	/*curRow++;
	m_layout->Add(curRow, 0, &m_radarTracks_treeCtrl,2);*/

	m_layout->Layout();
	m_layout->SyncFromObj();
}

void OWlDisplayCtrlDlg::iterateInitTracksTreeCtrl(OWlRadarSensor* sensor, void* userArg)
{
	SPxWinCheckboxTreeCtrl* treeCtrl = (SPxWinCheckboxTreeCtrl*)userArg;

	int id = sensor->getSensorId();
	CString str;
	str.Format(_T("%d站点\n"), id);
	HTREEITEM hRoot = treeCtrl->GetRootItem();
	HTREEITEM item = treeCtrl->InsertCheckboxItem(str, hRoot);

	int isActive = sensor->GetTrackRenderActive();
	treeCtrl->SetTriStateCheck(item, isActive);

	treeCtrl->SetItemData(item, reinterpret_cast<DWORD_PTR>(sensor));
}

void OWlDisplayCtrlDlg::iterateInitViewConfig(OWlRadarSensor* sensor, void* userArg)
{
	OWlDisplayCtrlDlg* dlg = (OWlDisplayCtrlDlg*)userArg;
	dlg->addRadarSensorViewConfigDlg(sensor);
}

void OWlDisplayCtrlDlg::addRadarSensorViewConfigDlg(OWlRadarSensor* sensor)
{
	const unsigned int tableColIndex = 0;
	static unsigned int tableRowIndex = 0;
	unsigned int colSpan = 1;//一个控件的列宽

	OWlRadarSensorViewConfigDlg* viewConfigDlg = new OWlRadarSensorViewConfigDlg(sensor);
	viewConfigDlg->Create(IDD_DIALOG_RADAR_SENSOR_VIEW_CONFIG);
	char buf[1024] = { 0 };
	sprintf_s(buf,"站点%d", sensor->getSensorId());
	m_radarSensorsTableDlg->Add(tableRowIndex, tableColIndex, viewConfigDlg, colSpan, buf, 1);
	tableRowIndex++;
}

void OWlDisplayCtrlDlg::OnBnClickedCheckMap()
{
	m_tiledMapRenderer->SetActive(m_map_checkbox.GetCheck());
}

int RadarVideoDispCtrlFunc(void *invokingObject,
	void *userObject,
	void *arg)
{
	SPxChannelNetRadar* channelRadar = (SPxChannelNetRadar*)arg;
	int isShow = *((int*)userObject);
	ChannelNetRadarUserData* userDate =
		(ChannelNetRadarUserData*)channelRadar->GetUserData(channelRadar);
	if (userDate)
	{
		if (isShow)
		{
			userDate->sc->ShowRadar(0, SPX_SC_STATE_RUN);
		}
		else
		{
			userDate->sc->ShowRadar(0, SPX_SC_STATE_STOP_AND_CLEAR);
		}
	}

	return 0;
}

void OWlDisplayCtrlDlg::OnBnClickedCheckRadarVideo()
{
	int ischeck = m_radarVideo_checkbox.GetCheck();
	/*if (ischeck)
	{
		m_sc->ShowRadar(0, SPX_SC_STATE_RUN);
	}
	else
	{
		m_sc->ShowRadar(0, SPX_SC_STATE_STOP_AND_CLEAR);
	}*/
	//m_channelDB->IterateForward(RadarVideoDispCtrlFunc,&ischeck, SPxChannel::TYPE_RADAR);
}


void OWlDisplayCtrlDlg::OnBnClickedCheckRadarTrack()
{
	int nums = m_uniTrackRender->GetNumTrackRenderers();
	for (int i = 1; i < nums; i++)
	{
		m_uniTrackRender->GetTrackRenderer(i)->SetActive(m_radarTrack_checkbox.GetCheck());
	}
}


void OWlDisplayCtrlDlg::OnBnClickedCheckFusedTrack()
{
	m_uniTrackRender->GetTrackRenderer(0)->SetActive(m_fusedTrack_checkbox.GetCheck());
}


void OWlDisplayCtrlDlg::OnBnClickedCheckAis()
{
	m_uniTrackRender->GetAISTrackRenderer(0)->SetActive(m_ais_checkbox.GetCheck());
}


//void OWlDisplayCtrlDlg::OnNMClickTreeRadarTracks(NMHDR *pNMHDR, LRESULT *pResult)
//{
//	// 转换消息
//	NM_TREEVIEW* pNMTreeView = (NM_TREEVIEW*)pNMHDR;
//
//	// 获取点击的位置
//	CPoint point;
//	GetCursorPos(&point);
//	m_radarTracks_treeCtrl.ScreenToClient(&point);
//
//	// 获取点击的节点
//	TVHITTESTINFO hitTestInfo;
//	hitTestInfo.pt = point;
//	m_radarTracks_treeCtrl.HitTest(&hitTestInfo);
//
//	// 检查点击的位置是否在复选框的区域内
//	if (hitTestInfo.flags & TVHT_ONITEMSTATEICON)
//	{
//		// 获取点击的节点
//		HTREEITEM hClickedItem = hitTestInfo.hItem;
//
//		// 在这里执行相应的操作，根据复选框的状态进行处理
//		int previousState = m_radarTracks_treeCtrl.GetTriStateCheck(hClickedItem);
//		int state = !previousState;
//
//		OWlRadarSensor*sensor = 
//			reinterpret_cast<OWlRadarSensor*>(m_radarTracks_treeCtrl.GetItemData(hClickedItem));
//		sensor->SetTrackRenderActive(state);
//	}
//
//	*pResult = 0;
//}




void OWlDisplayCtrlDlg::OnCbnSelchangeComboTrailSecs()
{
	int secs = SPxComboBoxGetVal(&m_trailSecs_combox);
	updateTrailLengthSecs(secs);
}

void OWlDisplayCtrlDlg::updateTrailLengthSecs(int secs)
{
	int numTrackRenders = m_uniTrackRender->GetNumTrackRenderers();
	int numAisRenders = m_uniTrackRender->GetNumAISTrackRenderers();

	for (int i = 0; i < numTrackRenders; i++)
	{
		m_uniTrackRender->GetTrackRenderer(i)->SetTrailLengthSecs(secs);
	}
	for (int i = 0; i < numAisRenders; i++)
	{
		m_uniTrackRender->GetAISTrackRenderer(i)->SetHistorySeconds(secs);
	}
}

void OWlDisplayCtrlDlg::OnBnClickedButtonRadarSensorViewConfig()
{
	SPxDialogPopupShow(m_radarSensorsTableDlg, SPX_DLG_POS_EDGE_PARENT);
}
