#pragma once
#include "afxwin.h"
#include "afxcmn.h"


// OWlDisplayCtrlDlg 对话框

class OWlDisplayCtrlDlg : public CDialogEx
{
	DECLARE_DYNAMIC(OWlDisplayCtrlDlg)

public:
	OWlDisplayCtrlDlg(
		SPxUniTrackRenderer* uniTrackRender
		, SPxTiledMapRendererWin * tiledMapRenderer
		, OWlRadarSensorManager* radarSensorMngr
		, CWnd* pParent = NULL);   // 标准构造函数
	virtual ~OWlDisplayCtrlDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_DISPLAY_CTRL };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
	virtual void OnOK();

private:
	SPxTableLayoutWin* m_layout;
	void initLayout();

	SPxUniTrackRenderer* m_uniTrackRender;
	SPxTiledMapRendererWin * m_tiledMapRenderer;
	OWlRadarSensorManager* m_radarSensorMngr;
	SPxTablePopupDlgWin* m_radarSensorsTableDlg;

	void updateTrailLengthSecs(int secs);

	void addRadarSensorViewConfigDlg(OWlRadarSensor* sensor);
public:
	static void iterateInitTracksTreeCtrl(OWlRadarSensor* sensor, void* userArg);
	static void iterateInitViewConfig(OWlRadarSensor* sensor, void* userArg);
public:
	virtual BOOL OnInitDialog();
	CButton m_map_checkbox;
	CButton m_radarVideo_checkbox;
	CButton m_radarTrack_checkbox;
	CButton m_fusedTrack_checkbox;
	CButton m_ais_checkbox;


	afx_msg void OnBnClickedCheckMap();
	afx_msg void OnBnClickedCheckRadarVideo();
	afx_msg void OnBnClickedCheckRadarTrack();
	afx_msg void OnBnClickedCheckFusedTrack();
	afx_msg void OnBnClickedCheckAis();
	CStatic m_trailSecs_label;
	CComboBox m_trailSecs_combox;
	SPxWinCheckboxTreeCtrl m_radarTracks_treeCtrl;
	//CTreeCtrl m_radarTracks_treeCtrl;
	//afx_msg void OnNMClickTreeRadarTracks(NMHDR *pNMHDR, LRESULT *pResult);

	afx_msg void OnCbnSelchangeComboTrailSecs();
	afx_msg void OnBnClickedButtonRadarSensorViewConfig();
	CButton m_radarSensorViewConfig_btn;
};
