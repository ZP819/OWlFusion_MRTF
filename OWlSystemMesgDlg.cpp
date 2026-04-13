// OWlSystemMesgDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "OWlSystemMesgDlg.h"
#include "afxdialogex.h"


#define NORMAL_COLOR RGB(0, 0, 0)
#define WARNING_COLOR RGB(255,140,0)
#define ERROR_COLOR RGB(255, 0, 0)
// OWlSystemMesgDlg 对话框

IMPLEMENT_DYNAMIC(OWlSystemMesgDlg, CDialogEx)

OWlSystemMesgDlg::OWlSystemMesgDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(IDD_DIALOG_SYSTEM_MESG, pParent)
{

}

OWlSystemMesgDlg::~OWlSystemMesgDlg()
{
}

void OWlSystemMesgDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_RICHEDIT_SYSTEM_MESSAGE, m_mesgCtrl);
}


BEGIN_MESSAGE_MAP(OWlSystemMesgDlg, CDialogEx)
END_MESSAGE_MAP()


// OWlSystemMesgDlg 消息处理程序


void OWlSystemMesgDlg::OnOK()
{
	// TODO: 在此添加专用代码和/或调用基类

	//CDialogEx::OnOK();
}


BOOL OWlSystemMesgDlg::OnInitDialog()
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

	m_mesgCtrl.SetShowErrors(FALSE);
	m_mesgCtrl.SetShowDate(TRUE);
	m_mesgCtrl.SetAddLineEndings(TRUE);
	m_mesgCtrl.SetScrollToBottom(TRUE);
	m_mesgCtrl.SetMessageFont("楷体_GB2312", 12);

	//m_mesgCtrl.SetLogFilePrefix("SystemLog_");
	SPxSetErrorHandler(ErrHandle, &m_mesgCtrl);//由SPxErr()触发回调

	return TRUE;  // return TRUE unless you set the focus to a control
				  // 异常: OCX 属性页应返回 FALSE
}



void OWlSystemMesgDlg::ErrHandle(SPxErrorType errType,
	SPxErrorCode errorCode,
	int arg1, int arg2,
	const char *arg3, const char *arg4,
	void *userArg)
{
	/* 注意：只处理OWlError()发出错误 */
	if (errorCode != SPX_NO_ERROR)
	{
		return;
	}


	SPxWinMessageCtrl* mesgCtrl = (SPxWinMessageCtrl*)userArg;

	char buf[4096] = { 0 };
	COLORREF color = NORMAL_COLOR;
	switch (errType)
	{
	case SPX_ERR_SYSTEM:
		buf[1] = 0;
		return;
	case SPX_ERR_SYSTEM_CANT_RECOVER:
		break;
	case SPX_ERR_USER://USER在这里做为ERROR
		sprintf_s(buf, "错误, %s.", arg3);
		color = ERROR_COLOR;
		break;
	case SPX_ERR_WARNING:
		sprintf_s(buf, "警告, %s.", arg3);
		color = WARNING_COLOR;
		break;
	case SPX_ERR_INFO:
		sprintf_s(buf, "信息, %s.", arg3);
		color = NORMAL_COLOR;
		break;
	default:
		break;
	}

	mesgCtrl->ReportMsg(color, buf);

	/*if (errType == SPX_ERR_USER)
	{
	mesgCtrl->ReportMsg(RGB(255, 0, 0), arg3);
	}
	else
	{
	mesgCtrl->ReportError(errType, errorCode, arg1, arg2, arg3, arg4);
	}*/

}


