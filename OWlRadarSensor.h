#pragma once
/*
 单个雷达站点:包含 航迹和回波两路通道数据
*/
class OWlRadarSensor
{
public:
	OWlRadarSensor(int id
		, SPxChannelTracks* channelTracks
		, SPxChannelNetRadar* channelNetRadar);
	~OWlRadarSensor();

private:
	int  m_id;
	SPxChannelTracks* m_channelTracks;
	SPxChannelNetRadar* m_channelNetRadar;

	SPxLatLong_t m_pos;

	SPxTablePopupDlgWin* m_radarSensorConfigTableDlg;

private:
	void initConfigTableDlg();
	
public:
	//void SetChannelTracks(SPxChannelTracks* channelTracks) { 
	//	m_channelTracks = channelTracks; }
	//void SetChannelNetRadar(SPxChannelNetRadar* channelNetRadar) { 
	//	m_channelNetRadar = channelNetRadar; }

	SPxChannelTracks* GetChannelTracks() { 
		return m_channelTracks; }
	SPxChannelNetRadar* GetChannelNetRadar() { 
		return m_channelNetRadar; }

	int getSensorId() const {
		return m_id;
	}


	//显示控制相关
	void SetTrackRenderActive(int isActive);
	int GetTrackRenderActive();
	void SetVideoShowActive(int isActive);
	int GetVideoShowActive();

	void ViewRadarCentre();//是雷达位置为视图中心

	SPxTablePopupDlgWin* GetConfigTableDlg() { 
		return m_radarSensorConfigTableDlg; }
};

