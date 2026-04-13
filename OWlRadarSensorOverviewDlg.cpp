// OWlRadarSensorOverviewDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "OWlRadarSensorOverviewDlg.h"
#include "afxdialogex.h"


// OWlRadarSensorOverviewDlg 对话框

IMPLEMENT_DYNAMIC(OWlRadarSensorOverviewDlg, CDialogEx)

OWlRadarSensorOverviewDlg::OWlRadarSensorOverviewDlg(
	OWlRadarSensor* sensor,
	CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_DIALOG_RADAR_SENSOR_OVERVIEW, pParent)
{
	m_sensor = sensor;
}

OWlRadarSensorOverviewDlg::~OWlRadarSensorOverviewDlg()
{
}

void OWlRadarSensorOverviewDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_STATIC_TRACK_STATUS_LABEL, m_trackStatus_label);
	DDX_Control(pDX, IDC_STATIC_TRACK_INFO, m_trackInfo_label);
	DDX_Control(pDX, IDC_STATIC_VIDEO_STATUS_LABEL, m_videoStatus_label);
	DDX_Control(pDX, IDC_BUTTON_RADAR_SENSOR_CONFIG, m_config_btn);
	DDX_Control(pDX, IDC_STATIC_TRACK_STATUS, m_trackStatus_static);
	DDX_Control(pDX, IDC_STATIC_VIDEO_INFO, m_videoInfo_label);
	DDX_Control(pDX, IDC_STATIC_VIDEO_STATUS, m_videoStatus_static);
	DDX_Control(pDX, IDC_BUTTON_VIEW_RADAR_CENTRE, m_viewRadarCentre_btn);
}


BEGIN_MESSAGE_MAP(OWlRadarSensorOverviewDlg, CDialogEx)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_BUTTON_RADAR_SENSOR_CONFIG, &OWlRadarSensorOverviewDlg::OnBnClickedButtonRadarSensorConfig)
	ON_BN_CLICKED(IDC_BUTTON_VIEW_RADAR_CENTRE, &OWlRadarSensorOverviewDlg::OnBnClickedButtonViewRadarCentre)
END_MESSAGE_MAP()


// OWlRadarSensorOverviewDlg 消息处理程序


void OWlRadarSensorOverviewDlg::OnOK()
{
	// TODO: 在此添加专用代码和/或调用基类

	//CDialogEx::OnOK();
}


BOOL OWlRadarSensorOverviewDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_trackStatus_static.ModifyStyle(0xF, SS_ICON | SS_CENTERIMAGE);
	SPxSetStatusIcon(&m_trackStatus_static, SPX_STATUS_NEUTRAL);

	m_videoStatus_static.ModifyStyle(0xF, SS_ICON | SS_CENTERIMAGE);
	SPxSetStatusIcon(&m_videoStatus_static, SPX_STATUS_NEUTRAL);

	initLayout();

	SetTimer(1,1000,NULL);

	return TRUE;  // return TRUE unless you set the focus to a control
				  // 异常: OCX 属性页应返回 FALSE
}


void OWlRadarSensorOverviewDlg::initLayout()
{
	m_layout = new SPxTableLayoutWin();
	m_layout->SetDialog(this);
	m_layout->SetControl(this);

	unsigned int curRow = 0;
	m_layout->Add(curRow, 0, &m_trackStatus_label);
	m_layout->Add(curRow, 1, &m_trackInfo_label);
	m_layout->Add(curRow, 2, &m_trackStatus_static);
	curRow++;
	m_layout->Add(curRow, 0, &m_videoStatus_label);
	m_layout->Add(curRow, 1, &m_videoInfo_label);
	m_layout->Add(curRow, 2, &m_videoStatus_static);
	curRow++;
	m_layout->Add(curRow, 0, &m_config_btn);
	m_layout->Add(curRow, 1, &m_viewRadarCentre_btn);
	curRow++;


	m_layout->Layout();
	m_layout->SyncFromObj();
}

void OWlRadarSensorOverviewDlg::OnTimer(UINT_PTR nIDEvent)
{
	SPxChannelTracks* channelTracks = (SPxChannelTracks*)m_sensor->GetChannelTracks();
	SPxChannelNetRadar* channelRadar = (SPxChannelNetRadar*)m_sensor->GetChannelNetRadar();

	SPxSetStatusIcon(&m_trackStatus_static, channelTracks->GetStatus());
	SPxSetStatusIcon(&m_videoStatus_static, channelRadar->GetStatus());

	CString str;
	str.Format(_T("%d"), channelTracks->GetTrackDB()->GetNumTracks());
	m_trackInfo_label.SetWindowTextW(str);

	
	str.Format(_T("%0.1f km"), channelRadar->GetPIM()->GetEndRange()/1000.0);
	m_videoInfo_label.SetWindowTextW(str);

	CDialogEx::OnTimer(nIDEvent);
}


void OWlRadarSensorOverviewDlg::OnBnClickedButtonRadarSensorConfig()
{
	SPxDialogPopupShow(m_sensor->GetConfigTableDlg(), SPX_DLG_POS_CURSOR);
}


void OWlRadarSensorOverviewDlg::OnBnClickedButtonViewRadarCentre()
{
	
	m_sensor->ViewRadarCentre();
}
