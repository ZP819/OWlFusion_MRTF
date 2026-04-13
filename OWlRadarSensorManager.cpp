#include "stdafx.h"
#include "OWlRadarSensorManager.h"



OWlRadarSensorManager::OWlRadarSensorManager(SPxChannelDatabase* channelDB):m_channelDB(channelDB)
{
	//还是通过遍历channelDB比较合适，不使用回调，意义不大
}


OWlRadarSensorManager::~OWlRadarSensorManager()
{
	
}


int OWlRadarSensorManager::ChannelDBIterateForward(void *invokingObject,
	void *userObject,
	void *arg)
{
	SPxChannelDatabase* channelDB = (SPxChannelDatabase*)invokingObject;
	SPxChannelTracks * channelTrack = (SPxChannelTracks*)arg;
	OWlRadarSensorManager* mngr = (OWlRadarSensorManager*)userObject;
	//获取与航迹通道管理的回波通道
	UINT8 channelNetRadarID = channelTrack->GetLinkedChannelID();
	SPxChannelNetRadar* channelNetRadar = (SPxChannelNetRadar*)channelDB->GetChannelNoLock(channelNetRadarID);

	OWlRadarSensor* sensor = mngr->AddSensor(channelTrack, channelNetRadar);
	/*sensor->SetChannelNetRadar(channelNetRadar);
	sensor->SetChannelTracks(channelTrack);*/


	return 0;
}


void OWlRadarSensorManager::InitSensorsFromChannelDB()
{
	/*
	20240117
	对于通道配置文件的要求：
	回波和航迹通道成对出现，回波在前，航迹在后，且航迹link到回波
	如果后续需要添加通道，需按照此规则，否则无法识别
	*/
	m_channelDB->IterateForward(ChannelDBIterateForward, this,SPxChannel::TYPE_TRACKS);
}


OWlRadarSensor* OWlRadarSensorManager::AddSensor(SPxChannelTracks* channelTracks, SPxChannelNetRadar* channelNetRadar) {

	OWlRadarSensor* newSensor = new OWlRadarSensor(nextSensorId, channelTracks, channelNetRadar);

	sensors.push_back(newSensor);
	nextSensorId++;
	return newSensor;
}

void OWlRadarSensorManager::RemoveSensor(int sensorId) {  // 修正函数名的大小写
	for (auto it = sensors.begin(); it != sensors.end(); ++it) {
		if ((*it)->getSensorId() == sensorId) {
			delete *it;
			sensors.erase(it);
			//已删除
			return;
		}
	}

	//未找到 OWlRadarSensor #
}

OWlRadarSensor* OWlRadarSensorManager::GetSensor(int sensorId) { 
	for (auto sensor : sensors) {
		if (sensor->getSensorId() == sensorId) {
			return sensor;
		}
	}

	return nullptr; // 如果未找到
}

void OWlRadarSensorManager::IterateSensors(void(*func)(OWlRadarSensor*, void*), void* userArg)
{
	for (auto sensor : sensors) {
		func(sensor, userArg);  // 传递用户参数
	}
}

//// 传入的函数指针
//void PrintSensorId(OWlRadarSensor* sensor, void* userArg) {
//	std::cout << "处理 OWlRadarSensor #" << sensor->getSensorId();
//	if (userArg) {
//		std::cout << ", 用户参数: " << *(static_cast<int*>(userArg));
//	}
//	std::cout << std::endl;
//}