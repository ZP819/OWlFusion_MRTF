/*********************************************************************
*
* (c) Copyright 2016, XWH.
*
* File: $RCSfile: OWlFusionPpi.h,v $
* ID: $Id: OWlFusionPpi.h,v 1.2 2016/03/16 16:13:14 rew Exp $
*
* Purpose:
*	OWlFusion program.
*
*	PPI dialog class.
*
*
* Revision Control:
*   16/03/16 v1.2    REW	Override OnOK()/OnCancel() to stop hiding window.
*				Add trail history retention. Rename objs.
*
* Previous Changes:
*   15/03/16 1.1    REW	Initial Version.
**********************************************************************/

#pragma once
#include "afxcmn.h"

#include "Fusion.h"
/*
 * Headers.
 */
/* Library headers. */
//#include "SPx.h"
//
///* Our headers. */
//#include "Resource.h"

/* Forward declare other classes we need. */
class OWlFusionDlg;

/*
 * OWlFusionPpi dialog.
 */
class OWlFusionPpi : public CDialog
{
public:
    /* Construction and destruction. */
    OWlFusionPpi(OWlFusionDlg *parentWin);
    virtual ~OWlFusionPpi(void);

    /* Dialog Data. */
    enum { IDD = IDD_PPI_DIALOG };

   

protected:
    /* Protected functions. */
    DECLARE_MESSAGE_MAP()
    virtual BOOL OnInitDialog();
    BOOL OnEraseBkgnd(CDC *pDC);
    void OnPaint(void);
    void OnTimer(UINT_PTR nIDEvent);

    /* Override OK/Cancel to prevent dialog disappearing. */
    virtual void OnOK(void){ /* Don't call the CDialog handler */ };
    virtual void OnCancel(void){ /* Don't call the CDialog handler */ };

private:
    /*
     * Private fields.
     */
    /* Parent dialog. */
    OWlFusionDlg *m_parent;	/* Parent dialog */

	/* Tiled map support. */
	SPxTiledMapDatabase *m_tiledMapDB;	/* Tiled map database */

	/* Radar objects. */
	SPxRIB *m_rib;			/* Radar input buffer */
	SPxPIM *m_pim;			/* Polar interface module */
	SPxNetworkReceiveAsterix *m_srcNet;	/* Network radar source */
	SPxRunProcess *m_baseProcess;	/* To connect others to */

	SPxLatLong_t m_radarLatLong;
	SPxRemoteServer* m_recvTracks;
	SPxTrackDatabase* m_trackDatabase;
	SPxTrackDatabase* m_trackDatabase1;
	SPxTrackRendererWin* m_trackRender;
	SPxTrackRendererWin* m_trackRender1;

	SPxRemoteServer* m_recvFusedTracks;
	SPxTrackDatabase* m_trackFusedDatabase;
	SPxTrackRendererWin* m_trackFusedRender;

	SPxChannelDatabase* m_channelDb;
	SPxChannelNetRadar* m_channelNetRadar;
	SPxRunProcess* m_gainOffsetProcess;
	SPxChannelTracks* m_channelTracks;
	SPxChannelTracks* m_channelTracks1;
	SPxChannelAIS* m_channelAIS;

	SPxAISDecoder* m_aisDecoder;
	SPxAISRendererWin* m_aisRender;
	Fusion* m_fusion;
	OWlRadarTracksFusion* m_radarTrackFusion;

	SPxUniTrackDatabase* m_uniTrackDb;
	SPxUniTrackRenderer* m_uniTrackRender;
	OWlUniTrackRender * m_owlUniTrackRender;

    /* View control. */
	SPxNavData*m_navData;
    SPxViewControl *m_viewCtrl;		/* View control object. */
    SPxMouseControlWin *m_mouseCtrl;	/* Mouse control object. */
    SPxScFollowWin *m_followWin;	/* Window following helper. */

    /* Underlay graphics. */
    SPxBitmapWin *m_ulayBitmap;		/* Underlay offscreen bitmap */
    Graphics *m_ulayGfx;		/* Underlay graphics GDI+ */
    SolidBrush *m_ulayBgBrush;		/* Underlay background brush */
    SPxTiledMapRendererWin *m_tiledMapRenderer;	/* Renderer */
	SPxWorldMap* m_worldMap;

    /* Radar layer for new data (the most recent scan). */
    SPxScSourceLocal *m_newSc;		/* Scan converter */
    SPxRunProcess *m_newScProcess;	/* Process for scan converter */
    SPxScDestDisplayWin *m_newDispWin;	/* Destination for scan conversion */

    /* Radar layer for trails data. */
    SPxScSourceLocal *m_trailsSc;	/* Scan converter */
    SPxRunProcess *m_trailsScProcess;	/* Process for scan converter */
    SPxScDestDisplayWin *m_trailsDispWin; /* Destination for scan conversion */

    /* Objects for preserving the history of trails in some modes. */
    SPxRunProcess *m_historyProcess;	/* Process for keeping history */
    SPxPIM *m_historyPIM;		/* PIM for history data */

    /* Overlay graphics. */
    SPxBitmapWin *m_olayBitmap;		/* Overlay offscreen bitmap */
    Graphics *m_olayGfx;		/* Overlay graphics GDI+ */
    SolidBrush *m_olayBgBrush;		/* Overlay background brush */
    SPxGraphicsWindow *m_olayWin;	/* Layered overlay window */
    SPxViewControlRendererWin *m_viewCtrlRenderer;	/* Renderer */
    SPxGraphicsRendererWin *m_gfxRenderer;		/* Renderer */

	
    /*
     * Private functions.
     */
    void redrawUlayGraphics(void);
    void redrawOlayGraphics(void);

public:
	SPxChannelDatabase* GetChannelDb() { 
		return m_channelDb; 
	}

	SPxUniTrackDatabase* GetUniTrackDatabase() {
		return m_uniTrackDb;
	}
	static int ChannelDatabaseEventFunc(void *invokingObject,
		void *userObject,
		void *arg);

	//Fusion* GetFusion() { return m_fusion; }
	OWlRadarTracksFusion* GetFusion() { return m_radarTrackFusion; }
	SPxUniTrackRenderer* GetUniTrackRenderer() { return m_uniTrackRender; }
	SPxTiledMapRendererWin * GetTiledMapRenderer() { return m_tiledMapRenderer; }
	SPxScSourceLocal *GetScSourceLocalRaw() { return m_newSc; }

	SPxChannelNetRadar* GetChannelNetRadar() { return m_channelNetRadar; }
	SPxChannelTracks* GetChannelTracks() { return m_channelTracks; }
	SPxChannelTracks* GetChannelTracks1() { return m_channelTracks1; }
	SPxChannelAIS* GetChannelAIS() { return m_channelAIS; }
	SPxWorldMap* GetWorldMap() { return m_worldMap; }

	SPxAISDecoder* GetAISsDecoder() { return m_aisDecoder; }

	static SPxErrorCode TrackRendererPreRenderFunc(SPxRadarTrack *track,
		void *userArg,
		SPxSymbol *symbol,
		UINT32 *colourPtr);

	/* 适配图层叠加，防止雷达图层盖住航迹图层 */
	static int IterateChannelToInitViewRes(void *invokingObject,
		void *userObject,
		void *arg);

	OWlRadarSensorManager* GetRadarSensorManager() { return m_radarSensorMngr; }
private:
	void onChannelAdd(SPxChannel* channel);
	UINT32 m_radarNums = 0;
	unsigned int screenWidth = 0, screenHeight = 0;
	OWlRadarSensorManager* m_radarSensorMngr;
	OWlRadarSensorRender* m_radarSensorRender;

	void initChannelViewRes(SPxChannel* channel);//初始化通道显示相关的资源
};

/*********************************************************************
*
* End of file
*
**********************************************************************/
