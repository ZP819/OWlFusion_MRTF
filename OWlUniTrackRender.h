#pragma once


class OWlUniTrackRender 
	:public SPxRendererWin
{
public:
	OWlUniTrackRender(
		SPxUniTrackDatabase* uniDb,
		SPxUniTrackRenderer* uniTrackRender);
	~OWlUniTrackRender();

	
	virtual SPxErrorCode renderAll(void);
	

private:
	SPxUniTrackRenderer* m_uniTrackRender;
	SPxUniTrackDatabase* m_uniDb;

private:
	
	void renderCaption(SPxUniTrack* track, int mouseX, int mouseY);

	string getRadarTrackCaptionTxt(SPxRadarTrack* track);
	string getAISTrackCaptionTxt(SPxAISTrack* track);
	string getFusedTrackCaptionTxt(SPxRadarTrack* track);
	string getRadarFusedTrackCaptionTxt(SPxRadarTrack* track);
};