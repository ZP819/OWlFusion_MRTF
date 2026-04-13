#pragma once
#include "afxwin.h"

#include "OWlRadarTracksFusion.h"

// OWlFuseConfigDlg 对话框

class OWlFuseConfigDlg : public CDialogEx
{
	DECLARE_DYNAMIC(OWlFuseConfigDlg)

public:
	OWlFuseConfigDlg(
		OWlRadarTracksFusion* fusion,
		CWnd* pParent = NULL);   // 标准构造函数
	virtual ~OWlFuseConfigDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_FUSE_CONFIG };
#endif

private:
	OWlRadarTracksFusion* m_fusion;
	SPxTableLayoutWin* m_layout;
	void initLayout();

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
	virtual void OnOK();
public:
	virtual BOOL OnInitDialog();
	LRESULT OnSpinChange(WPARAM wParam, LPARAM lParam);

	CButton m_speedRule_checkbox;
	CButton m_courseRule_checkbox;
	CStatic m_radarAziObsDiff_label;
	SPxWinSpinCtrl m_radarAziObsDiff_spin;
	CStatic m_radarRangeObsDiff_label;
	SPxWinSpinCtrl m_radarRangeObsDiff_spin;
	CStatic m_gpsRangeObsDiff_label;
	SPxWinSpinCtrl m_gpsRangeObsDiff_spin;
	CStatic m_startThresh_label;
	SPxWinSpinCtrl m_startThresh_spin;
	CStatic m_steadyThresh_label;
	SPxWinSpinCtrl m_steadyThresh_spin;
	afx_msg void OnBnClickedCheckSpeedRule();
	afx_msg void OnBnClickedCheckCourseRule();
};
