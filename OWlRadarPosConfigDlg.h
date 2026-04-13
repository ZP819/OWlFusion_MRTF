#pragma once
#include "afxwin.h"


// OWlRadarPosConfigDlg 对话框

class OWlRadarPosConfigDlg : public CDialogEx
{
	DECLARE_DYNAMIC(OWlRadarPosConfigDlg)

public:
	OWlRadarPosConfigDlg(SPxWorldMap* worldMap,CWnd* pParent = NULL);   // 标准构造函数
	virtual ~OWlRadarPosConfigDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_RADAR_POS };
#endif

private:
	SPxWorldMap* m_worldMap;
	SPxTableLayoutWin* m_layout;
	void initLayout();
protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
public:
	CStatic m_lat_static;
	CStatic m_longti_static;
	SPxWinSpinCtrl m_lat_spin;
	SPxWinSpinCtrl m_longti_spin;

	virtual void OnOK();
	virtual BOOL OnInitDialog();

	LRESULT OnSpinChange(WPARAM wParam, LPARAM lParam);
};
