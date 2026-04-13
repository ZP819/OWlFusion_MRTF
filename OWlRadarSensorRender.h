#pragma once

class OWlRadarSensorRender :
	public SPxRendererWin
{
public:
	OWlRadarSensorRender(OWlRadarSensorManager* radarSensorMngr);
	~OWlRadarSensorRender();

	virtual SPxErrorCode renderAll(void);

	

private:
	OWlRadarSensorManager* m_radarSensorMngr;
	SPxSymbol m_radarPosSymbol;

private:
	void renderAllRadarsPos();
	static void iterateRenderRadarPos(
		OWlRadarSensor* sensor, void* userArg);

	void renderRadarPos(OWlRadarSensor* sensor);
};

