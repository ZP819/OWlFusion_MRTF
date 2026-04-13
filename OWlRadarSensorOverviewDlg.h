#pragma once
#include "afxwin.h"


// OWlRadarSensorOverviewDlg 对话框

class OWlRadarSensorOverviewDlg : public CDialogEx
{
	DECLARE_DYNAMIC(OWlRadarSensorOverviewDlg)

public:
	OWlRadarSensorOverviewDlg(
		OWlRadarSensor* sensor,
		CWnd* pParent = NULL);   // 标准构造函数
	virtual ~OWlRadarSensorOverviewDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_RADAR_SENSOR_OVERVIEW };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
	virtual void OnOK();


private:
	SPxTableLayoutWin* m_layout;
	void initLayout();

	OWlRadarSensor* m_sensor;
public:
	virtual BOOL OnInitDialog();
	CStatic m_trackStatus_label;
	CStatic m_trackInfo_label;
	CStatic m_videoStatus_label;
	CButton m_config_btn;
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	CStatic m_trackStatus_static;
	CStatic m_videoInfo_label;
	CStatic m_videoStatus_static;
	CButton m_viewRadarCentre_btn;
	afx_msg void OnBnClickedButtonRadarSensorConfig();
	afx_msg void OnBnClickedButtonViewRadarCentre();
};
