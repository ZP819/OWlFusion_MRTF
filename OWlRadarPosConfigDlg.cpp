// OWlRadarPosConfigDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "OWlRadarPosConfigDlg.h"
#include "afxdialogex.h"


// OWlRadarPosConfigDlg 对话框

IMPLEMENT_DYNAMIC(OWlRadarPosConfigDlg, CDialogEx)

OWlRadarPosConfigDlg::OWlRadarPosConfigDlg(SPxWorldMap* worldMap, CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_DIALOG_RADAR_POS, pParent)
{
	m_worldMap = worldMap;
}

OWlRadarPosConfigDlg::~OWlRadarPosConfigDlg()
{
}

void OWlRadarPosConfigDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_STATIC_RADAR_LAT, m_lat_static);
	DDX_Control(pDX, IDC_STATIC_RADAR_LONG, m_longti_static);
	DDX_Control(pDX, IDC_CUSTOM_RADAR_LAT, m_lat_spin);
	DDX_Control(pDX, IDC_CUSTOM_RADAR_LONG, m_longti_spin);
}


BEGIN_MESSAGE_MAP(OWlRadarPosConfigDlg, CDialogEx)
	ON_MESSAGE(WM_SPINCHANGE, &OWlRadarPosConfigDlg::OnSpinChange)
END_MESSAGE_MAP()


// OWlRadarPosConfigDlg 消息处理程序


void OWlRadarPosConfigDlg::OnOK()
{
	// TODO: 在此添加专用代码和/或调用基类

	//CDialogEx::OnOK();
}


BOOL OWlRadarPosConfigDlg::OnInitDialog()
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
	SetWindowText(_T("融合中心位置\t"));

	m_lat_spin.SetRange(-90.0, 90.0);
	m_lat_spin.SetDecimalPlaces(6);
	m_lat_spin.SetValue(m_worldMap->GetRadarLatDegs());

	m_longti_spin.SetRange(-360.0, 360.0);
	m_longti_spin.SetDecimalPlaces(6);
	m_longti_spin.SetValue(m_worldMap->GetRadarLongDegs());

	initLayout();

	return TRUE;  // return TRUE unless you set the focus to a control
				  // 异常: OCX 属性页应返回 FALSE
}


void OWlRadarPosConfigDlg::initLayout()
{
	m_layout = new SPxTableLayoutWin();
	m_layout->SetDialog(this);
	m_layout->SetControl(this);

	unsigned int curRow = 0;
	m_layout->Add(curRow, 0, &m_lat_static);
	m_layout->Add(curRow, 1, &m_lat_spin, 3);
	curRow++;
	m_layout->Add(curRow, 0, &m_longti_static);
	m_layout->Add(curRow, 1, &m_longti_spin, 3);
	
	m_layout->Layout();
	m_layout->SyncFromObj();
}

LRESULT OWlRadarPosConfigDlg::OnSpinChange(WPARAM wParam, LPARAM lParam)
{
	switch (wParam)
	{
	case IDC_CUSTOM_RADAR_LAT:
		 m_worldMap->SetRadarLatLong(
			m_lat_spin.GetValueAsDouble()
			, m_longti_spin.GetValueAsDouble());
		break;
	case IDC_CUSTOM_RADAR_LONG:
		m_worldMap->SetRadarLatLong(
			m_lat_spin.GetValueAsDouble()
			, m_longti_spin.GetValueAsDouble());
		break;
	}
	return 0L;
}