// OWlRadarSensorViewConfigDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "OWlRadarSensorViewConfigDlg.h"
#include "afxdialogex.h"


// OWlRadarSensorViewConfigDlg 对话框

IMPLEMENT_DYNAMIC(OWlRadarSensorViewConfigDlg, CDialogEx)

OWlRadarSensorViewConfigDlg::OWlRadarSensorViewConfigDlg(
	OWlRadarSensor* sensor,
	CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_DIALOG_RADAR_SENSOR_VIEW_CONFIG, pParent)
{
	m_sensor = sensor;
}

OWlRadarSensorViewConfigDlg::~OWlRadarSensorViewConfigDlg()
{
}

void OWlRadarSensorViewConfigDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_CHECK_VIDEO_ACTIVE, m_videoActive_checkbox);
	DDX_Control(pDX, IDC_CHECK_TRACK_ACTIVE, m_trackActive_checkbox);
}


BEGIN_MESSAGE_MAP(OWlRadarSensorViewConfigDlg, CDialogEx)
	ON_BN_CLICKED(IDC_CHECK_VIDEO_ACTIVE, &OWlRadarSensorViewConfigDlg::OnBnClickedCheckVideoActive)
	ON_BN_CLICKED(IDC_CHECK_TRACK_ACTIVE, &OWlRadarSensorViewConfigDlg::OnBnClickedCheckTrackActive)
END_MESSAGE_MAP()


// OWlRadarSensorViewConfigDlg 消息处理程序


void OWlRadarSensorViewConfigDlg::OnOK()
{
	// TODO: 在此添加专用代码和/或调用基类

	//CDialogEx::OnOK();
}


BOOL OWlRadarSensorViewConfigDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	m_trackActive_checkbox.SetCheck(m_sensor->GetTrackRenderActive());
	m_videoActive_checkbox.SetCheck(1);

	initLayout();

	return TRUE;  // return TRUE unless you set the focus to a control
				  // 异常: OCX 属性页应返回 FALSE
}


void OWlRadarSensorViewConfigDlg::initLayout()
{
	m_layout = new SPxTableLayoutWin();
	m_layout->SetDialog(this);
	m_layout->SetControl(this);


	unsigned int curRow = 0;
	m_layout->Add(curRow, 0, &m_videoActive_checkbox);
	m_layout->Add(curRow, 1, &m_trackActive_checkbox,2);
	curRow++;

	m_layout->Layout();
	m_layout->SyncFromObj();
}

void OWlRadarSensorViewConfigDlg::OnBnClickedCheckVideoActive()
{
	m_sensor->SetVideoShowActive(m_videoActive_checkbox.GetCheck());
}


void OWlRadarSensorViewConfigDlg::OnBnClickedCheckTrackActive()
{
	m_sensor->SetTrackRenderActive(m_trackActive_checkbox.GetCheck());
}
