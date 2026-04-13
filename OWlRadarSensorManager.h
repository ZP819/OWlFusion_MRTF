#pragma once

#include "OWlRadarSensor.h"

class OWlRadarSensorManager
{
public:
	OWlRadarSensorManager(SPxChannelDatabase* channelDB);
	~OWlRadarSensorManager();

private:
	std::vector<OWlRadarSensor*> sensors;
	int nextSensorId = 1; // 下一个传感器的ID

	SPxChannelDatabase* m_channelDB;

private:
	

public:
	OWlRadarSensor* AddSensor(SPxChannelTracks* channelTracks, SPxChannelNetRadar* channelNetRadar);
	void RemoveSensor(int sensorId);
	OWlRadarSensor* GetSensor(int sensorId);


	void InitSensorsFromChannelDB();
	static int ChannelDBIterateForward(void *invokingObject,
		void *userObject,
		void *arg);

	void IterateSensors(void(*func)(OWlRadarSensor*, void*), void* userArg);
};

