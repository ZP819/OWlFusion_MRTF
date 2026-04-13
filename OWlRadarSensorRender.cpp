#include "stdafx.h"
#include "OWlRadarSensorRender.h"


OWlRadarSensorRender::OWlRadarSensorRender(OWlRadarSensorManager* radarSensorMngr)
{
	SetClassName("OWlRadarSensorRender");

	m_radarSensorMngr = radarSensorMngr;



	m_radarPosSymbol.SetMode(0);
	m_radarPosSymbol.SetShape(0, SPX_SYMBOL_SHAPE_DIAG_CROSS);
	m_radarPosSymbol.SetLineWidth(0,m_radarPosSymbol.GetLineWidth(0)*2);
	//m_radarPosSymbol.SetSizePixels(0);
}


OWlRadarSensorRender::~OWlRadarSensorRender()
{
}


SPxErrorCode OWlRadarSensorRender::renderAll(void)
{
	SPxErrorCode err = SPX_NO_ERROR;

	renderAllRadarsPos();

	return err;
}


void OWlRadarSensorRender::renderRadarPos(OWlRadarSensor* sensor)
{
	double lat = 0;
	double lon = 0;
	if (SPX_NO_ERROR != 
		sensor->GetChannelNetRadar()->GetLatLong(&lat,&lon))
	{
		return;
	}
	double pixelX = 0;
	double pixelY = 0;
	int isInWindow = 0;
	if (SPX_NO_ERROR != 
		GetWinXYFromLatLong(lat, lon, &pixelX, &pixelY, &isInWindow))
	{
		return;
	}

	if (isInWindow)
	{ 
		//À×´ï·ûºÅ
		m_radarPosSymbol.Render(this, pixelX, pixelY);

		//Ãû³Æ
		double pad = SPxGetDpiScaledSize(5.0);
		SPxRendererXY_t pos = { int(pixelX + pad), int(pixelY + pad) };

		char buf[1024] = { 0 };
		if (SPX_NO_ERROR 
			== sensor->GetChannelNetRadar()->GetDisplayName(buf, sizeof(buf)))
		{
			string str = to_string(sensor->getSensorId());
			str += "_";
			str += string(buf);
			renderText(0xffffffff, &pos, str.c_str());
		}

		//endRange
		SPxRendererXY_t centre = { int(pixelX), int(pixelY) };
		double ppm = GetViewPixelsPerMetre();
		double endRangeMetres = sensor->GetChannelNetRadar()->GetPIM()->GetEndRange();
		if (endRangeMetres > EPS)
		{
			unsigned int raduisPixels = ppm * endRangeMetres;
			renderArc(0xffffffff, &centre, raduisPixels, raduisPixels, 0, 360, 0);
		}
		
	}
}

void OWlRadarSensorRender::renderAllRadarsPos()
{
	m_radarSensorMngr->IterateSensors(iterateRenderRadarPos, this);
}

void OWlRadarSensorRender::iterateRenderRadarPos(
	OWlRadarSensor* sensor, void* userArg)
{
	OWlRadarSensorRender* render = (OWlRadarSensorRender*)userArg;
	render->renderRadarPos(sensor);
}