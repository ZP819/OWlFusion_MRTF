// OWlFuseConfigDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "OWlFuseConfigDlg.h"
#include "afxdialogex.h"


// OWlFuseConfigDlg 对话框

IMPLEMENT_DYNAMIC(OWlFuseConfigDlg, CDialogEx)

OWlFuseConfigDlg::OWlFuseConfigDlg(OWlRadarTracksFusion* fusion, CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_DIALOG_FUSE_CONFIG, pParent)
{
	m_fusion = fusion;
}

OWlFuseConfigDlg::~OWlFuseConfigDlg()
{
}

void OWlFuseConfigDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_CHECK_SPEED_RULE, m_speedRule_checkbox);
	DDX_Control(pDX, IDC_CHECK_COURSE_RULE, m_courseRule_checkbox);
	DDX_Control(pDX, IDC_STATIC_RADAR_AZI_OBS_DIFF, m_radarAziObsDiff_label);
	DDX_Control(pDX, IDC_CUSTOM_RADAR_AZI_OBS_DIFF, m_radarAziObsDiff_spin);
	DDX_Control(pDX, IDC_STATIC_RADAR_RANGE_OBS_DIFF, m_radarRangeObsDiff_label);
	DDX_Control(pDX, IDC_CUSTOM_RADAR_RANGE_OBS_DIFF, m_radarRangeObsDiff_spin);
	DDX_Control(pDX, IDC_STATIC_GPS_RANGE_OBS_DIFF, m_gpsRangeObsDiff_label);
	DDX_Control(pDX, IDC_CUSTOM_GPS_RANGE_OBS_DIFF, m_gpsRangeObsDiff_spin);
	DDX_Control(pDX, IDC_STATIC_START_FUSE_THRESH, m_startThresh_label);
	DDX_Control(pDX, IDC_CUSTOM_START_FUSE_THRESH, m_startThresh_spin);
	DDX_Control(pDX, IDC_STATIC_STEADY_FUSE_THRESH, m_steadyThresh_label);
	DDX_Control(pDX, IDC_CUSTOM_STEADY_FUSE_THRESH, m_steadyThresh_spin);
}


BEGIN_MESSAGE_MAP(OWlFuseConfigDlg, CDialogEx)
	ON_MESSAGE(WM_SPINCHANGE, &OWlFuseConfigDlg::OnSpinChange)
	ON_BN_CLICKED(IDC_CHECK_SPEED_RULE, &OWlFuseConfigDlg::OnBnClickedCheckSpeedRule)
	ON_BN_CLICKED(IDC_CHECK_COURSE_RULE, &OWlFuseConfigDlg::OnBnClickedCheckCourseRule)
END_MESSAGE_MAP()


// OWlFuseConfigDlg 消息处理程序


void OWlFuseConfigDlg::OnOK()
{
	// TODO: 在此添加专用代码和/或调用基类

	//CDialogEx::OnOK();
}


BOOL OWlFuseConfigDlg::OnInitDialog()
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

	m_radarAziObsDiff_spin.SetRange(0.0,360.0);
	m_radarAziObsDiff_spin.SetValue(m_fusion->GetRadarAziObsDiff());

	m_radarRangeObsDiff_spin.SetRange(0.0, 9999.0);
	m_radarRangeObsDiff_spin.SetValue(m_fusion->GetRadarDisObsDiff());

	/*m_gpsRangeObsDiff_spin.SetRange(0.0, 9999.0);
	m_gpsRangeObsDiff_spin.SetValue(m_fusion->GetGpsDisObsDiff());

	m_startThresh_spin.SetRange((int)ONE,(int)SIX);
	m_startThresh_spin.SetDecimalPlaces(0);
	m_startThresh_spin.SetValue(m_fusion->GetThreshStart());

	m_steadyThresh_spin.SetRange((int)ONE, (int)SIX);
	m_steadyThresh_spin.SetDecimalPlaces(0);
	m_steadyThresh_spin.SetValue(m_fusion->GetThreshMaintain());

	m_speedRule_checkbox.SetCheck(m_fusion->GetSpeedRuleActive());
	m_courseRule_checkbox.SetCheck(m_fusion->GetCourseRuleActive());*/

	initLayout();

	return TRUE;  // return TRUE unless you set the focus to a control
				  // 异常: OCX 属性页应返回 FALSE
}


void OWlFuseConfigDlg::initLayout()
{
	m_layout = new SPxTableLayoutWin();
	m_layout->SetDialog(this);
	m_layout->SetControl(this);

	unsigned int curRow = 0;
	/*m_layout->Add(curRow, 0, &m_speedRule_checkbox);
	m_layout->Add(curRow, 1, &m_courseRule_checkbox);
	curRow++;*/
	m_layout->Add(curRow, 0, &m_radarAziObsDiff_label);
	m_layout->Add(curRow, 1, &m_radarAziObsDiff_spin);
	curRow++;
	m_layout->Add(curRow, 0, &m_radarRangeObsDiff_label);
	m_layout->Add(curRow, 1, &m_radarRangeObsDiff_spin);
	curRow++;
	//m_layout->Add(curRow, 0, &m_gpsRangeObsDiff_label);
	//m_layout->Add(curRow, 1, &m_gpsRangeObsDiff_spin);
	//curRow++;
	//m_layout->Add(curRow, 0, &m_startThresh_label);
	//m_layout->Add(curRow, 1, &m_startThresh_spin);
	//curRow++;
	//m_layout->Add(curRow, 0, &m_steadyThresh_label);
	//m_layout->Add(curRow, 1, &m_steadyThresh_spin);
	//curRow++;

	m_speedRule_checkbox.ShowWindow(SW_HIDE);
	m_courseRule_checkbox.ShowWindow(SW_HIDE);
	m_gpsRangeObsDiff_label.ShowWindow(SW_HIDE);
	m_gpsRangeObsDiff_spin.ShowWindow(SW_HIDE);
	m_startThresh_label.ShowWindow(SW_HIDE);
	m_startThresh_spin.ShowWindow(SW_HIDE);
	m_steadyThresh_label.ShowWindow(SW_HIDE);
	m_steadyThresh_spin.ShowWindow(SW_HIDE);


	m_layout->Layout();
	m_layout->SyncFromObj();
}



void OWlFuseConfigDlg::OnBnClickedCheckSpeedRule()
{
	// TODO: 在此添加控件通知处理程序代码
}


void OWlFuseConfigDlg::OnBnClickedCheckCourseRule()
{
	// TODO: 在此添加控件通知处理程序代码
}

LRESULT OWlFuseConfigDlg::OnSpinChange(WPARAM wParam, LPARAM lParam)
{
	switch (wParam)
	{
	case IDC_CUSTOM_RADAR_AZI_OBS_DIFF:
		m_fusion->SetRadarAziObsDiff(m_radarAziObsDiff_spin.GetValueAsDouble());
		break;
	case IDC_CUSTOM_RADAR_RANGE_OBS_DIFF:
		m_fusion->SetRadarDisObsDiff(m_radarRangeObsDiff_spin.GetValueAsDouble());
		break;
	/*case IDC_CUSTOM_GPS_RANGE_OBS_DIFF:
		m_fusion->SetGpsDisObsDiff(m_gpsRangeObsDiff_spin.GetValueAsDouble());
		break;
	case IDC_CUSTOM_START_FUSE_THRESH:
		m_fusion->SetThreshStart((FusionThreshold)m_startThresh_spin.GetValueAsUint());
		break;
	case IDC_CUSTOM_STEADY_FUSE_THRESH:
		m_fusion->SetThreshMaintain((FusionThreshold)m_steadyThresh_spin.GetValueAsUint());
		break;*/
	}
	return 0L;
}