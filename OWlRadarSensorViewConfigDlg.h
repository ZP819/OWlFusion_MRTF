#pragma once
#include "afxwin.h"


// OWlRadarSensorViewConfigDlg �Ի���

class OWlRadarSensorViewConfigDlg : public CDialogEx
{
	DECLARE_DYNAMIC(OWlRadarSensorViewConfigDlg)

public:
	OWlRadarSensorViewConfigDlg(OWlRadarSensor* sensor, CWnd* pParent = NULL);   // ��׼���캯��
	virtual ~OWlRadarSensorViewConfigDlg();

// �Ի�������
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_RADAR_SENSOR_VIEW_CONFIG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

	DECLARE_MESSAGE_MAP()

private:
	SPxTableLayoutWin* m_layout;
	void initLayout();

	OWlRadarSensor* m_sensor;
public:
	CButton m_videoActive_checkbox;
	CButton m_trackActive_checkbox;
	virtual void OnOK();
	virtual BOOL OnInitDialog();
	afx_msg void OnBnClickedCheckVideoActive();
	afx_msg void OnBnClickedCheckTrackActive();
};
