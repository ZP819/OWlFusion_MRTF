#include "stdafx.h"
#include "OWlRadarSensor.h"


OWlRadarSensor::OWlRadarSensor(int id
	, SPxChannelTracks* channelTracks
	, SPxChannelNetRadar* channelNetRadar)
	: m_id(id)
	, m_channelTracks(channelTracks)
	, m_channelNetRadar(channelNetRadar)
	
{
	/*m_channelTracks = NULL;
	m_channelNetRadar = NULL;*/

	initConfigTableDlg();

}


OWlRadarSensor::~OWlRadarSensor()
{

}

void OWlRadarSensor::initConfigTableDlg()
{
	OWlEncryptFile ef;
	ef.DeyptFile("lae.csv");
	SPxLanguageDlgWin* ld = new SPxLanguageDlgWin(NULL, NULL);
	if (SPxIsFile(OWL_LANG_FILE_NAME))
	{
		SPxErrorCode er = ld->LoadLanguages(OWL_LANG_FILE_NAME, "Chinese", SPX_LANG_ENCODING_AUTO);
		ef.EnyptFile("la.csv");
	}


	const unsigned int tableColIndex = 0;//即：共有1列
	unsigned int tableRowIndex = 0;
	unsigned int colSpan = 1;//一个控件的列宽
	m_radarSensorConfigTableDlg = new SPxTablePopupDlgWin(NULL);
	m_radarSensorConfigTableDlg->Create();
	CString str;
	str.Format(_T("雷达配置: 站点%d\n"), m_id);
	m_radarSensorConfigTableDlg->SetWindowTextW(str);

	SPxNetCtrlDlgWin* radarTrackInputNetCtrlDlg =
		new SPxNetCtrlDlgWin(m_radarSensorConfigTableDlg, m_channelTracks);
	radarTrackInputNetCtrlDlg->Create();
	m_radarSensorConfigTableDlg->Add(tableRowIndex, tableColIndex, radarTrackInputNetCtrlDlg, colSpan, "航迹输入网络\n", 1);
	tableRowIndex++;

	SPxNetCtrlDlgWin* radarTrackOutputNetCtrlDlg =
		new SPxNetCtrlDlgWin(m_radarSensorConfigTableDlg, m_channelTracks->GetOutputNetCtrlIf(0));
	radarTrackOutputNetCtrlDlg->Create();
	m_radarSensorConfigTableDlg->Add(tableRowIndex, tableColIndex, radarTrackOutputNetCtrlDlg, colSpan, "航迹输出网络\n", 1);
	tableRowIndex++;

	m_radarSensorConfigTableDlg->SyncFromObj();
	m_radarSensorConfigTableDlg->Layout();
	//SPxDialogPopupShow(m_radarSensorConfigTableDlg);
}

void OWlRadarSensor::SetTrackRenderActive(int isActive)
{
	ChannelRadarTracksUserData* userData =
		(ChannelRadarTracksUserData*)m_channelTracks->GetUserData(m_channelTracks);
	if (userData)
	{
		userData->trackRender->SetActive(isActive);
	}
}

int OWlRadarSensor::GetTrackRenderActive()
{
	int ret = 0;
	ChannelRadarTracksUserData* userData =
		(ChannelRadarTracksUserData*)m_channelTracks->GetUserData(m_channelTracks);
	if (userData)
	{
		ret = userData->trackRender->GetActive();
	}
	return ret;
}

void OWlRadarSensor::SetVideoShowActive(int isActive)
{
	ChannelNetRadarUserData* userData =
		(ChannelNetRadarUserData*)m_channelNetRadar->GetUserData(m_channelNetRadar);
	if (userData)
	{
		if (isActive)
		{
			userData->sc->ShowRadar(0, SPX_SC_STATE_RUN);
		}
		else
		{
			userData->sc->ShowRadar(0, SPX_SC_STATE_STOP_AND_CLEAR);
		}
	}
}

int OWlRadarSensor::GetVideoShowActive()
{
	//无法获取，默认有效
	int ret = 1;

	return ret;
}

void OWlRadarSensor::ViewRadarCentre()
{
	ChannelRadarTracksUserData* userData =
		(ChannelRadarTracksUserData*)m_channelTracks->GetUserData(m_channelTracks);
	if (userData)
	{
		SPxViewControl* viewCtrl = userData->trackRender->GetViewControl();
		SPxLatLong_t pos = {0};
		if (SPX_NO_ERROR == m_channelTracks->GetLatLong(&pos.latDegs, &pos.longDegs))
		{
			viewCtrl->SetViewCentreLatLong(&pos);
		}
	}
}