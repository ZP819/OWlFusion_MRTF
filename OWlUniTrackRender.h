#pragma once

class OWlRadarTracksFusion;

class OWlUniTrackRender 
	:public SPxRendererWin
{
public:
	OWlUniTrackRender(
		SPxUniTrackDatabase* uniDb,
		SPxUniTrackRenderer* uniTrackRender);
	~OWlUniTrackRender();
	void SetFusion(OWlRadarTracksFusion* fusion);

	
	virtual SPxErrorCode renderAll(void);
	

private:
	SPxUniTrackRenderer* m_uniTrackRender;
	SPxUniTrackDatabase* m_uniDb;
	OWlRadarTracksFusion* m_radarTrackFusion;

private:
	
	void renderCaption(SPxUniTrack* track, int mouseX, int mouseY);

	string getRadarTrackCaptionTxt(SPxRadarTrack* track);
	string getAISTrackCaptionTxt(SPxAISTrack* track);
	string getFusedTrackCaptionTxt(SPxRadarTrack* track);
	string getRadarFusedTrackCaptionTxt(SPxRadarTrack* track);
};
