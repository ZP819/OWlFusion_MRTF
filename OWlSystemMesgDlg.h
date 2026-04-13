#pragma once


// OWlSystemMesgDlg 对话框

class OWlSystemMesgDlg : public CDialogEx
{
	DECLARE_DYNAMIC(OWlSystemMesgDlg)

public:
	OWlSystemMesgDlg(CWnd* pParent = NULL);   // 标准构造函数
	virtual ~OWlSystemMesgDlg();

	// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_SYSTEM_MESG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()
	virtual void OnOK();

public:
	virtual BOOL OnInitDialog();
	SPxWinMessageCtrl m_mesgCtrl;

	static void ErrHandle(SPxErrorType errType,
		SPxErrorCode errorCode,
		int arg1, int arg2,
		const char *arg3, const char *arg4,
		void *userArg);

};
