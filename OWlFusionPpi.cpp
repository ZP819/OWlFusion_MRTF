

/*
 * Include files.
 */

/* "Standard" headers */
#include "stdafx.h"

/* Library headers. */

/* Our own headers. */
#include "OWlFusionDlg.h"
#include "OWlFusionPpi.h"

int OWlFusionPpi::IterateChannelToInitViewRes(void *invokingObject,
	void *userObject,
	void *arg)
{
	OWlFusionPpi* userObj = (OWlFusionPpi*)userObject;
	
	userObj->initChannelViewRes((SPxChannel*)arg);

	return 0;
}

	
SPxErrorCode OWlFusionPpi::TrackRendererPreRenderFunc(SPxRadarTrack *track,
	void *userArg,
	SPxSymbol *symbol,
	UINT32 *colourPtr)
{

	FusedTrackUserData* userData = (FusedTrackUserData*)track->GetUserData();
	if (userData)
	{
		size_t numSensor = userData->trackVec.size();
		UINT32 colour = 0XFFFF0000;
		if (numSensor == 1)
		{
			colour = 0XFFFF0000;
			//*colourPtr = 0XFFFF0000;
		}
		if (numSensor == 2)
		{
			colour = 0XFFFFFFFF;
			//*colourPtr = 0XFFFFFFFF;
		}
		if (numSensor == 3)
		{
			colour = 0XFF00FFFF;
			//*colourPtr = 0XFF00FFFF;
		}
		*colourPtr = colour;
		//symbol->SetColourAllModes(colour);
	}


	//if (track->GetExtRpt()->fusion.sensors == 3)//两部雷达做贡献
	//{
	//	*colourPtr = 0XFFFFFFFF;
	//}
	//else
	//{
	//	*colourPtr = 0XFFFF0000;
	//}

	return SPX_NO_ERROR;
}

int OWlFusionPpi::ChannelDatabaseEventFunc(void *invokingObject,
	void *userObject,
	void *arg)
{
	SPxChannelDatabase::Event_t* evt = (SPxChannelDatabase::Event_t*)arg;
	SPxChannelDatabase::EventType_t eventType = evt->type;

	OWlFusionPpi* userObj = (OWlFusionPpi*)userObject;

	switch (eventType)
	{
	case SPxChannelDatabase::EVENT_CHANNEL_ADD:
		userObj->onChannelAdd(evt->info.channelEvt.channel);
		break;
	case SPxChannelDatabase::EVENT_CHANNEL_REMOVE:
		break;
	default:
		break;
	}
	return 0;
}
/*
 * Macros.
 */


/*
 * Constants.
 */


/*
 * Types.
 */


/*
 * Private functions.
 */


/*
 * Global variables.
 */

/*********************************************************************
 *
 *	OWlFusionPpi class.
 *
 *********************************************************************/


/*====================================================================
 *
 * OWlFusionPpi::MESSAGE_MAP
 *	Message map for the dialog.
 *
 *===================================================================*/

BEGIN_MESSAGE_MAP(OWlFusionPpi, CDialog)
    ON_WM_ERASEBKGND()
    ON_WM_PAINT()
    ON_WM_TIMER()
END_MESSAGE_MAP()

/*====================================================================
 *
 * OWlFusionPpi::OWlFusionPpi
 *	Constructor for the PPI dialog class.
 *
 * Params:
 *	parentWin		Parent dialog class.
 *
 * Returns:
 *	Nothing
 *
 * Notes
 *
 *===================================================================*/
OWlFusionPpi::OWlFusionPpi(
					OWlFusionDlg *parentWin)
	: CDialog(OWlFusionPpi::IDD, parentWin)
{
    /*
     * Initialise things.
     */
    m_parent = parentWin;

    /* View control. */
    m_viewCtrl = NULL;
    m_mouseCtrl = NULL;
    m_followWin = NULL;

    /* Underlay graphics. */
    m_ulayBitmap = NULL;
    m_ulayGfx = NULL;
    m_ulayBgBrush = NULL;
    m_tiledMapRenderer = NULL;

    /* Radar layers. */
    m_newSc = NULL;
    m_newScProcess = NULL;
    m_newDispWin = NULL;
    m_trailsSc = NULL;
    m_trailsScProcess = NULL;
    m_trailsDispWin = NULL;

    /* Trail history objects. */
    m_historyProcess = NULL;
    m_historyPIM = NULL;

    /* Overlay graphics. */
    m_olayBitmap = NULL;
    m_olayGfx = NULL;
    m_olayBgBrush = NULL;
    m_olayWin = NULL;
    m_viewCtrlRenderer = NULL;
    m_gfxRenderer = NULL;


    /* Done. */
    return;
} /* OWlFusionPpi::OWlFusionPpi */


/*====================================================================
 *
 * OWlFusionPpi::~OWlFusionPpi
 *	Destructor for the main dialog class.
 *
 * Params:
 *	None
 *
 * Returns:
 *	Nothing
 *
 * Notes
 *
 *===================================================================*/
OWlFusionPpi::~OWlFusionPpi(void)
{
	//delete m_radarTrackFusion;
} /* OWlFusionPpi::~OWlFusionPpi */

/*====================================================================
 *
 * OWlFusionPpi::OnInitDialog
 *	Function to initialise the dialog, basically creating all the
 *	objects we need and setting up the GUI.
 *
 * Params:
 *	None
 *
 * Returns:
 *	FALSE if we set the input focus to a control,
 *	TRUE if not.
 *
 * Notes
 *	Called by framework in response to WM_INITDIALOG message
 *	(probably during the DoModal() call to the object).
 *
 *===================================================================*/
BOOL OWlFusionPpi::OnInitDialog()
{
	OWlVMProtect vmp("init");

    /* Initialise the base class. */
    CDialog::OnInitDialog();

	OWlEncryptFile ef;
	ef.DeyptFile("confige.ini");
	if (SPxIsFile(ConfigFileName))
	{
		SPxLoadConfig(ConfigFileName);

		ef.DelFile((char*)ConfigFileName);
	}
	else
	{
		OWlError(OWL_ERR_WARNING, "配置文件不存在");
	}

	ef.DeyptFile("lae.csv");
	SPxLanguageDlgWin* ld = new SPxLanguageDlgWin(NULL, NULL);
	if (SPxIsFile(OWL_LANG_FILE_NAME))
	{
		SPxErrorCode er = ld->LoadLanguages(OWL_LANG_FILE_NAME, "Chinese", SPX_LANG_ENCODING_AUTO);
		ef.EnyptFile("la.csv");
	}


	
	/*
	* Initialise our navigation data input.
	*/
	SPxErrorCode ertn;
	m_channelDb = new SPxChannelDatabase();//依赖nrp
	m_channelDb->SetName("ChannelDatabase");

	m_channelDb->AddEventCallback(ChannelDatabaseEventFunc, this);
	m_channelDb->Load("channel.ini");

	m_radarSensorMngr = new OWlRadarSensorManager(m_channelDb);
	m_radarSensorMngr->InitSensorsFromChannelDB();

	
	/* Create a tiled map database. */
	m_tiledMapDB = new SPxTiledMapDatabase();
	m_tiledMapDB->SetName("TiledMapDatabase");
	const char *customName = "GoogleSatelliteMap";
	m_tiledMapDB->SetCustomName(customName);
	const char *customPrefix = "http://mt3.google.com/vt/lyrs=s&";
	m_tiledMapDB->SetCustomURLPrefix(customPrefix);
	const char *customSuffix = "";
	m_tiledMapDB->SetCustomURLSuffix(customSuffix);
	m_tiledMapDB->SetCustomTileNameFormat(SPxTiledMapDatabase::TILE_NAME_FORMAT_QUERY_XYZ);
	m_tiledMapDB->SetCustomTileImageFormat(SPxTiledMapDatabase::TILE_IMAGE_FORMAT_PNG);
	m_tiledMapDB->SetServer(SPxTiledMapDatabase::SERVER_CUSTOM);
	m_tiledMapDB->SetStateFromConfig();

	/* Add a null process to act as a connection point for other proceses. */
	//m_baseProcess = new SPxRunProcess(SPxProNull, NULL, m_pim);

    /* See how big our screen is. */
   /* unsigned int screenWidth = 0, screenHeight = 0;*/
    SPxGetMaxScreenSize(&screenWidth, &screenHeight);

    /* Create a follow-win class to slave layers to our window. */
    m_followWin = new SPxScFollowWin(NULL, this);


    /* Choose the default graphics colour. */
    UINT32 graphicsColour = 0xFFFF8000;		/* ARGB */

	m_radarLatLong.latDegs = SPxGetReal("WorldMap.RadarLatDegs");
	m_radarLatLong.longDegs = SPxGetReal("WorldMap.RadarLongDegs");

    m_viewCtrl = new SPxViewControl();
    /* Set the initial view based on the initial nav data position. */
    SPxLatLong_t platformLL = m_radarLatLong;
	m_viewCtrl->SetPlatformLatLong(&platformLL);
    m_viewCtrl->SetViewCentreLatLong(&platformLL);
    m_viewCtrl->SetViewWidthDegs(0.5);

    /* Connect the view control to the follow-win object so it tracks
     * changes to the window size.
     */
    m_viewCtrl->SetWindowSizeAuto(m_followWin);
	//m_viewCtrl->SetWebMercatorMode(SPxViewControl::WEB_MERCATOR_MODE_ON);

    /* Set the state of the object from the configuration file. */
    m_viewCtrl->SetStateFromConfig();

    /*
     * Initialise the mouse control object for handling pan/zoom etc.
     */
    m_mouseCtrl = new SPxMouseControlWin( m_viewCtrl );
    m_mouseCtrl->SetWindow( this->GetSafeHwnd() );
    m_mouseCtrl->SetZoomOnDoubleClick(FALSE);
	m_mouseCtrl->SetContextMenuEnabled(1);


    /*************************************************************
     *
     * Initialise underlay graphics.
     *
     *************************************************************/
    /* Offscreen bitmap for rendering the underlay. */
    m_ulayBitmap = new SPxBitmapWin();
    m_ulayBitmap->CreateLocal(screenWidth, screenHeight);

    /* Use HALFTONE for improved scaling quality. */
    ::SetStretchBltMode(m_ulayBitmap->GetHDC(), HALFTONE);

    /* Create the underlay graphics GDI+ renderer. */
    m_ulayGfx = new Graphics(m_ulayBitmap->GetHDC());

    /* Configure anti-alias settings. */
    m_ulayGfx->SetTextRenderingHint(TextRenderingHintAntiAlias);
    m_ulayGfx->SetSmoothingMode(SmoothingModeAntiAlias);

    /* Create underlay background brush. */
    m_ulayBgBrush = new SolidBrush( Color((ARGB)0xFF000000) );

	m_worldMap = new SPxWorldMap();
	m_worldMap->SetName("WorldMap");
	m_worldMap->SetRadarLatLong(platformLL.latDegs, platformLL.longDegs);
	m_worldMap->SetStateFromConfig();

    /* Create tiled map renderer and link to view control. */
    m_tiledMapRenderer = new SPxTiledMapRendererWin(m_tiledMapDB);
    m_tiledMapRenderer->SetActive(TRUE);
    m_tiledMapRenderer->SetDestination(m_ulayBitmap->GetHDC());
    m_tiledMapRenderer->SetTextRenderingMode(SPX_TEXT_RENDER_ANTI_ALIAS);
    m_tiledMapRenderer->SetViewControl(m_viewCtrl);
	m_tiledMapRenderer->SetShowProgressIndicator(0);
    m_tiledMapRenderer->SetStateFromConfig();
  
    /*************************************************************
     *
     * Initialise the radar layer for new/live radar data.
     *
     *************************************************************/
	m_channelDb->IterateForward(IterateChannelToInitViewRes, this, SPxChannel::TYPE_RADAR);
    
    /*************************************************************
     *
     * Initialise overlay graphics.
     *
     *************************************************************/
    /* Offscreen bitmap for rendering the overlay. */
    m_olayBitmap = new SPxBitmapWin();
    m_olayBitmap->CreateLocal(screenWidth, screenHeight);

    /* Use HALFTONE for improved scaling quality. */
    ::SetStretchBltMode(m_olayBitmap->GetHDC(), HALFTONE);

    /* Create the overlay graphics GDI+ renderer.  Set compositing mode
     * to replace destination pixels rather than combine (i.e. overlay).
     */
    m_olayGfx = new Graphics(m_olayBitmap->GetHDC());
    m_olayGfx->SetCompositingMode(CompositingModeSourceCopy);

    /* Configure anti-alias settings. */
    m_olayGfx->SetTextRenderingHint(TextRenderingHintAntiAlias);
    m_olayGfx->SetSmoothingMode(SmoothingModeAntiAlias);

    /* Create overlay background brush, transparent (alpha = 0). */
    m_olayBgBrush = new SolidBrush(Color((ARGB)0x00000000));

    /* Create overlay window and make it a child of the underlay. */
    m_olayWin = new SPxGraphicsWindow();
    m_olayWin->Create( GetSafeHwnd() );

    /* Configure the window so that it is layered and will use 
     * per-pixel alpha values to control transparency of graphics.
     */
    m_olayWin->SetAlphaBlending(TRUE);

    /* Make the overlay window transparent to mouse events. */
    m_olayWin->SetTransparency(TRUE);

    /* Slave the window to the underlay window so that it
     * will track the size, position and stacking order of the
     * underlay window.
     */
    m_followWin->SetSlaveWindow(0, m_olayWin->GetCWnd());

    /* Create a view control renderer for scale marker, buttons etc. */
    m_viewCtrlRenderer = new SPxViewControlRendererWin();
    m_viewCtrlRenderer->Create();
    m_viewCtrlRenderer->SetViewControl(m_viewCtrl);
    m_viewCtrlRenderer->SetDestination(m_olayBitmap->GetHDC());
    m_viewCtrlRenderer->SetTextRenderingMode(SPX_TEXT_RENDER_ANTI_ALIAS);
    m_viewCtrlRenderer->SetRangeScaleEnabled(TRUE);
    m_viewCtrlRenderer->SetRangeScaleColour(graphicsColour);
    m_viewCtrlRenderer->SetCursorPosEnabled(TRUE);
    m_viewCtrlRenderer->SetCursorPosColour(graphicsColour);
    m_viewCtrlRenderer->SetCursorPosOptions(SPX_CURSOR_POS_OPT_RANGE_AZI | SPX_CURSOR_POS_OPT_LAT_LONG);
    m_viewCtrlRenderer->SetStateFromConfig();
	m_viewCtrlRenderer->SaveInitialView();
	m_viewCtrlRenderer->EnableMenu(0);
	m_viewCtrlRenderer->SetViewButtonsEnabled(0);
    m_mouseCtrl->AddInterface(m_viewCtrlRenderer);



    /* Create a graphics renderer for radar position, range rings etc. */
    m_gfxRenderer = new SPxGraphicsRendererWin();
    m_gfxRenderer->SetViewControl(m_viewCtrl);
    m_gfxRenderer->SetDestination(m_olayBitmap->GetHDC());
    m_gfxRenderer->SetRangeRingsEnabled(false);
    m_gfxRenderer->SetRangeRingsColour(graphicsColour);
    //m_gfxRenderer->SetRadarPositionEnabled(TRUE);	
    m_gfxRenderer->SetStateFromConfig();
	m_gfxRenderer->EnableMenu(0);
	m_mouseCtrl->AddInterface(m_gfxRenderer);

	m_uniTrackDb = new SPxUniTrackDatabase();
	m_uniTrackRender = new SPxUniTrackRenderer(m_uniTrackDb);

	m_aisDecoder = new SPxAISDecoder();
	m_aisDecoder->SetExtrapolatePeriodSecs(2.5);
	m_aisDecoder->SetSrcType(SPX_MESG_IO_TYPE_NETWORK_RAW); //SPX_MESG_IO_TYPE_NETWORK_RAW
	//m_aisDecoder->SetSrcNetAddr("127.0.0.1", 7771);
	m_aisDecoder->SetSrcNetAddr("239.192.14.45", 7109);
	m_aisRender = new SPxAISRendererWin(m_aisDecoder);
	m_aisRender->SetViewControl(m_viewCtrl);
	m_aisRender->SetDestination(m_olayBitmap->GetHDC());
	m_aisRender->SetLineWidth(SPxGetDpiScaledSize(1.6));
	m_aisRender->SetShowSelectedTrails(1);
	m_aisRender->SetHistorySeconds(360);
	m_aisRender->SetShowAllTrails(1);
	m_aisRender->SetShowInfoPanel(1);
	m_aisRender->SetShowExtrapolations(1);
	m_aisRender->SetActive(1);




	m_recvFusedTracks = new SPxRemoteServer();
	const char* addrRecvFusedTracks = "127.0.0.1";
	//m_recvFusedTracks->AddSocket(addrRecvFusedTracks, 7770, INADDR_ANY);//radar
	m_recvFusedTracks->AddSocket(addrRecvFusedTracks, 7770);//radar
	m_recvFusedTracks->StartReportHandlerThread();
	m_trackFusedDatabase = new SPxTrackDatabase();
	m_trackFusedDatabase->Create(m_recvFusedTracks);
	//m_trackFusedDatabase->SetName("FUSE");
	m_radarLatLong.latDegs = 22.928247;
	m_radarLatLong.longDegs = 116.387072;
	m_trackFusedDatabase->SetSensorUserLatLong(m_radarLatLong.latDegs, m_radarLatLong.longDegs);
	m_trackFusedRender = new SPxTrackRendererWin(m_trackFusedDatabase);
	m_trackFusedRender->SetViewControl(m_viewCtrl);
	m_trackFusedRender->SetDestination(m_olayBitmap->GetHDC());
	m_trackFusedRender->SetPointWidth(SPxGetDpiScaledSize(2.0));
	SPxSymbol* symbolfused = m_trackFusedRender->GetTrackSymbol();
	symbolfused->SetShapeAllModes(SPX_SYMBOL_SHAPE_CIRCLE);
	symbolfused->SetLineWidthAllModes(SPxGetDpiScaledSize(1.6));
	symbolfused->SetSizePixelsAllModes(SPxGetDpiScaledSize(18.0));//21.0
	symbolfused->SetColourAllModes(0xffff0000);
	//m_trackFusedRender->SetTrailLengthSecs(1200); //360/2
	m_trackFusedRender->SetTrailLengthSecs(0);
	m_trackFusedRender->SetLabelColour(0xffffffff);
	m_trackFusedRender->SetSelectedColour(0xff00ffff);
	//m_trackFusedRender->SetLabelOffsetPixels(10, -10);
	m_trackFusedRender->SetShowLabels(0);
	m_trackFusedRender->SetPreRenderCallback(TrackRendererPreRenderFunc, NULL);
	

	m_uniTrackDb->AddFusedTrackSource(m_trackFusedDatabase);
	m_uniTrackDb->AddAISTrackSource(m_aisDecoder);
	m_uniTrackRender = new SPxUniTrackRenderer(m_uniTrackDb);
	m_uniTrackRender->AddFusedTrackRenderer(m_trackFusedRender);
	m_uniTrackRender->AddAISTrackRenderer(m_aisRender);
	/*m_uniTrackRender->AddRadarTrackRenderer(m_trackRender);
	m_uniTrackRender->AddRadarTrackRenderer(m_trackRender1);
	m_uniTrackRender->AddAISTrackRenderer(m_aisRender);
	m_uniTrackRender->AddFusedTrackRenderer(m_trackFusedRender);*/
	m_uniTrackRender->SetShowTrackCaption(0);
	m_uniTrackRender->SetAllowDragDrop(0);
	m_mouseCtrl->AddInterface(m_uniTrackRender);

	m_owlUniTrackRender = new OWlUniTrackRender(m_uniTrackDb,m_uniTrackRender);
	m_owlUniTrackRender->SetViewControl(m_viewCtrl);
	m_owlUniTrackRender->SetDestination(m_olayBitmap->GetHDC());
	m_owlUniTrackRender->SetFont(
		m_owlUniTrackRender->GetFontName()
		, SPxGetDpiScaledSize(13.0)
		, SPX_RENDERER_FONT_STYLE_BOLD);
	m_mouseCtrl->AddInterface(m_owlUniTrackRender);
	
	//todo:注意销毁
	m_radarSensorRender = new OWlRadarSensorRender(m_radarSensorMngr);
	m_radarSensorRender->SetViewControl(m_viewCtrl);
	m_radarSensorRender->SetDestination(m_olayBitmap->GetHDC());
	m_radarSensorRender->SetFont(
		m_radarSensorRender->GetFontName()
		, SPxGetDpiScaledSize(13.0)
		, SPX_RENDERER_FONT_STYLE_BOLD);
	
	m_channelDb->IterateForward(IterateChannelToInitViewRes, this, SPxChannel::TYPE_TRACKS);
	//m_fusion = new Fusion(m_uniTrackDb, m_trackDatabase, m_aisDecoder);



	m_radarTrackFusion = new OWlRadarTracksFusion(m_uniTrackDb);

	//m_navData->SetActive(0);
	SPxObj::SetStateFromConfigAll();
	SetTimer(1, 50, NULL);

    /* Return TRUE, unless you set the focus to a control. */
    return TRUE;
} /* OWlFusionPpi::OnInitDialog */


/*====================================================================
*
* OWlFusionPpi::OnEraseBkgnd
*	Handler for the erase background message.
*
* Params:
*	pDC		Drawing context.
*
* Returns:
*	TRUE to say we've erased it.
*
* Notes:
*	We override this function but do nothing (the background is cleared
*	in the OnPaint() function) so that the base class doesn't clear the
*	background because that can cause flicker on redraws.
*
*===================================================================*/
BOOL OWlFusionPpi::OnEraseBkgnd(CDC *pDC)
{
   return(TRUE);
} /* OWlFusionPpi::OnEraseBkgnd() */


/*====================================================================
*
* OWlFusionPpi::OnPaint
*	Handler to repaint the underlay window.
*
* Params:
*	None
*
* Returns:
*	Nothing
*
* Notes
*
*===================================================================*/
void OWlFusionPpi::OnPaint(void)
{
    /* Call the base class function. */
    CDialog::OnPaint();

    /* Check initialised. */
    if( m_ulayBitmap )
    {
	/* Get the window size. */
	CRect ulayRect;
	GetClientRect(&ulayRect);
	unsigned int width = ulayRect.Width();
	unsigned int height = ulayRect.Height();

	/* Update the underlay window from our bitmap. */
	CDC *dc = GetDC();
	BitBlt(dc->m_hDC, 0, 0, width, height,
		   m_ulayBitmap->GetHDC(), 0, 0, SRCCOPY);
	ReleaseDC(dc);
    }

    return;
} /* OWlFusionPpi::OnPaint() */


/*====================================================================
 *
 * OWlFusionPpi::OnTimer
 *	This timer is used to flush changes to the view
 *	and redraw underlay and/or overlay graphics if
 *	the view has changed.
 *
 * Params:
 *	nIDEvent    Timer ID set when timer was initialised.
 *
 * Returns:
 *	Nothing.
 *
 * Notes
 *
 *===================================================================*/
void OWlFusionPpi::OnTimer(UINT_PTR nIDEvent)
{
    CDialog::OnTimer(nIDEvent);

    /* Check we have a view control object. */
    if( m_viewCtrl )
    {
	/* Flush any pending changes to the renderers etc. */
	m_viewCtrl->FlushChanges();

	/* Get and clear the change flags, to see what's changed. */
	UINT32 changeFlags = m_viewCtrl->GetChangeFlags(TRUE);
	if( changeFlags )
	{
	    /* View has changed, so redraw underlay graphics.  */
	    redrawUlayGraphics();
	}
    }

    /* Redraw overlay graphics every time (to update changing fields,
     * and provide an animated display).
     */
    redrawOlayGraphics();

    /* Done. */
    return;
} /* OWlFusionPpi::OnTimer() */



/*====================================================================
*
* OWlFusionPpi::redrawUlayGraphics
*	Redraw the underlay graphics.
*
* Params:
*	None
*
* Returns:
*	Nothing
*
* Notes
*
*===================================================================*/
void OWlFusionPpi::redrawUlayGraphics(void)
{
    /* Check we've been initialised. */
    if( (m_ulayGfx == NULL)
	|| (m_tiledMapRenderer == NULL) )
    {
	/* Don't do anything yet. */
	return;
    }

    /* Get the window size. */
    CRect ulayRect;
    GetClientRect(&ulayRect);
    unsigned int width = ulayRect.Width();
    unsigned int height = ulayRect.Height();

    /* Clear the underlay bitmap. The extra pixels ensure
     * that an anti-aliased gap is not visible at the edges.
     * The fill will be clipped to the window size.
     */
    m_ulayGfx->FillRectangle(m_ulayBgBrush, -1, -1, width + 2, height + 2);

    /* Render the tiled map if enabled. */
    if( m_tiledMapRenderer->GetActive() )
    {
	m_tiledMapRenderer->Render();
    }

    /* Invalidate the window to force a redraw via OnPaint(). */
    this->Invalidate();
	

    /* Done. */
    return;
} /* OWlFusionPpi::redrawUlayGraphics() */


/*====================================================================
*
* OWlFusionPpi::redrawOlayGraphics
*	Redraw the overlay graphics.
*
* Params:
*	None
*
* Returns:
*	Nothing
*
* Notes
*
*===================================================================*/
void OWlFusionPpi::redrawOlayGraphics(void)
{
    /* Check we're initialised. */
    if( (m_olayGfx == NULL)
	|| (m_olayBitmap == NULL)
	|| (m_olayWin == NULL) )
    {
	return;
    }

    /* Get the window size. */
    CRect olayRect;
    m_olayWin->GetCWnd()->GetClientRect(&olayRect);
    unsigned int width = olayRect.Width();
    unsigned int height = olayRect.Height();

    /* Clear the overlay bitmap. */
    m_olayGfx->FillRectangle(m_olayBgBrush, 0, 0, width, height);

    /* Draw view control graphics. */
    if( m_viewCtrlRenderer )
    {
	m_viewCtrlRenderer->Render();
    }

    /* Draw overlay graphics. */
    if( m_gfxRenderer )
    {
	m_gfxRenderer->Render();
    }
	
	if (m_uniTrackRender)
	{
		m_uniTrackRender->Render();
	}

	if (m_owlUniTrackRender)
	{
		m_owlUniTrackRender->Render();
	}
	

	if (m_radarSensorRender)
	{
		m_radarSensorRender->Render();
	}
	

    /* Update the layered overlay window from the bitmap. */
    m_olayWin->PaintWindow(m_olayBitmap->GetHDC(), 0, 0, width, height);

    /* Done. */
    return;
} /* OWlFusionPpi::redrawOlayGraphics() */


/*********************************************************************
*
* End of file
*
**********************************************************************/

UINT32 TRACK_COLOR_ARRAY[OWL_MAX_SENSOR_NUMS] = {
	0xFFFFFF00,
	0xFF0000FF,
	0xFF00FF00,
	0xFFFFC1C1,
	0xFF00A5FF,
	0xFFBC8F8F,
	0xFFFFFAFA,
	0xFF00BFFF,
	0xFFDAA520,
	0xFF2E8B57,
	/* todo:add */
};
UINT32 RADAR_COLOR_ARRAY[OWL_MAX_SENSOR_NUMS] = {
	0xFF00FF00,
	0xFF00A5FF,
	0xFF0000FF,
	0xFFFF0000,
	0xFFFFC1C1,
	0xFF2E8B57,
	0xFF00BFFF,
	0xFFDAA520,
	0xFFFFFAFA,
	0xFFBC8F8F,
	/* todo:add */
};
void OWlFusionPpi::onChannelAdd(SPxChannel* channel)
{
	if (channel->GetType() == SPxChannel::TYPE_TRACKS)
	{
		/* new userdata */
		SPxChannelTracks* channelTracks = (SPxChannelTracks*)channel;
		channelTracks->SetStateFromConfig();

		ChannelRadarTracksUserData* userDate =
			(ChannelRadarTracksUserData*)channelTracks->GetUserData(channelTracks);
		if (!userDate)
		{
			userDate = new ChannelRadarTracksUserData();
			channelTracks->SetUserData(channelTracks, userDate);
		}

		//if (userDate)
		//{
		//	userDate->trackRender = new SPxTrackRendererWin(channelTracks->GetTrackDB());
		//	userDate->trackRender->SetViewControl(m_viewCtrl);
		//	userDate->trackRender->SetDestination(m_olayBitmap->GetHDC());
		//	userDate->trackRender->SetLabelMoveEnabled(TRUE);
		//	userDate->trackRender->SetPointWidth(SPxGetDpiScaledSize(2.0));
		//	SPxSymbol* symbol = userDate->trackRender->GetTrackSymbol();
		//	symbol->SetShapeAllModes(SPX_SYMBOL_SHAPE_CIRCLE);
		//	symbol->SetColourAllModes(TRACK_COLOR_ARRAY[channelTracks->GetID()-1]);
		//	char labelBuf[10] = { 0 };
		//	sprintf_s(labelBuf, "T%d-%%i", channelTracks->GetID());
		//	userDate->trackRender->SetLabel(labelBuf);
		//	userDate->trackRender->SetLabelColour(0xffffffff);//white
		//	userDate->trackRender->SetShowLabels(0);
		//	userDate->trackRender->SetTrailLengthSecs(360/2);

		//	m_uniTrackDb->AddRadarTrackSource(channelTracks->GetTrackDB());
		//	m_uniTrackRender->AddRadarTrackRenderer(userDate->trackRender);
		//	
		//}
	}


	if (channel->GetType() == SPxChannel::TYPE_RADAR)
	{
		SPxChannelNetRadar* channelRadar = (SPxChannelNetRadar*)channel;
		//注意：要从配置中更新参数，load内部不会执行，否则经纬度参数等不会更新

		//添加扩展参数，从配置文件读取
		//channelRadar->AddExternalParameter("RGB","0x00FF00");

		channelRadar->SetStateFromConfig();

		/* new userdata */
		ChannelNetRadarUserData* userDate =
			(ChannelNetRadarUserData*)channelRadar->GetUserData(channelRadar);
		if (!userDate)
		{
			userDate = new ChannelNetRadarUserData();
			channelRadar->SetUserData(channelRadar, userDate);
		}

		//if (userDate)
		//{
		//	int radarIndex = m_radarNums;
		//	m_radarNums++;

		//	/* Create the window to show the radar data.  */
		//	userDate->dispWin = new SPxScDestDisplayWin();
		//	userDate->dispWin->Create(0, screenWidth, screenHeight, this);
		//	/* Set the update interval in msecs. */
		//	userDate->dispWin->SetUpdateInterval(30);

		//	/* Create the scan converter, telling it which display window to use. */
		//	userDate->sc = new SPxScSourceLocal(userDate->dispWin);
		//	userDate->sc->SetFade(SPX_RADAR_FADE_REPLACE, 0);
		//	//userDate->sc->SetRadarColour(0, 0, 255, 0);	/* Green */  /* 0, 255, 165, 0 Orange */

		//	UINT32 color = RADAR_COLOR_ARRAY[radarIndex];
		//	userDate->sc->SetRadarColour(0, GetRValue(color), GetGValue(color), GetBValue(color));
		//	userDate->sc->SetRadarBright(0, 150);

		//	m_followWin->AddSC(userDate->sc);
		//	userDate->sc->SetViewControl(m_viewCtrl, radarIndex, 0);

		//	userDate->scProcess = new SPxRunProcess(SPxProScanConv, NULL,
		//		channelRadar->GetPIM(), userDate->sc);

		//	SPxLatLong_t pos;
		//	channelRadar->GetLatLong(&pos.latDegs, &pos.longDegs);
		//	double xMetres = 0;
		//	double yMetres = 0;
		//	m_viewCtrl->LatLongToMetres(&pos, &xMetres, &yMetres);
		//	
		//	SPxLatLong_t platformLL = { 31.349948, 121.904502 };
		//	SPxLatLongDiffMetres(platformLL.latDegs, platformLL.longDegs, pos.latDegs, pos.longDegs, &xMetres, &yMetres);

		//	m_viewCtrl->SetDeviceOffsetMetres(xMetres, yMetres, radarIndex, 0);
		//
		//	//注意：只能通过nav将雷达位置信息传递到viewCtrl
		//	//m_navData->SetRadarOffsetMetres(xMetres, yMetres, radarIndex);

		//	userDate->sc->ShowRadar(0, SPX_SC_STATE_STOP_AND_CLEAR);
		//}
	}
}


void OWlFusionPpi::initChannelViewRes(SPxChannel* channel)
{
	static int trackIndex = 0;
	if (channel->GetType() == SPxChannel::TYPE_TRACKS)
	{
		SPxChannelTracks* channelTracks = (SPxChannelTracks*)channel;

		ChannelRadarTracksUserData* userDate =
			(ChannelRadarTracksUserData*)channelTracks->GetUserData(channelTracks);
		if (userDate)
		{
			userDate->trackRender = new SPxTrackRendererWin(channelTracks->GetTrackDB());
			userDate->trackRender->SetViewControl(m_viewCtrl);
			userDate->trackRender->SetDestination(m_olayBitmap->GetHDC());
			userDate->trackRender->SetLabelMoveEnabled(TRUE);
			userDate->trackRender->SetPointWidth(SPxGetDpiScaledSize(2.0));
			SPxSymbol* symbol = userDate->trackRender->GetTrackSymbol();
			symbol->SetShapeAllModes(SPX_SYMBOL_SHAPE_CIRCLE);

			symbol->SetColourAllModes(TRACK_COLOR_ARRAY[trackIndex++]);
			char labelBuf[10] = { 0 };
			sprintf_s(labelBuf, "T%d-%%i", channelTracks->GetID());
			userDate->trackRender->SetLabel(labelBuf);
			userDate->trackRender->SetLabelColour(0xffffffff);//white
			userDate->trackRender->SetShowLabels(0);
			//userDate->trackRender->SetTrailLengthSecs(360 / 2);
			userDate->trackRender->SetTrailLengthSecs(0);

			m_uniTrackDb->AddRadarTrackSource(channelTracks->GetTrackDB());
			m_uniTrackRender->AddRadarTrackRenderer(userDate->trackRender);

		}
	}


	if (channel->GetType() == SPxChannel::TYPE_RADAR)
	{
		SPxChannelNetRadar* channelRadar = (SPxChannelNetRadar*)channel;

		ChannelNetRadarUserData* userDate =
			(ChannelNetRadarUserData*)channelRadar->GetUserData(channelRadar);
		if (userDate)
		{
			int radarIndex = m_radarNums;
			m_radarNums++;

			/* Create the window to show the radar data.  */
			userDate->dispWin = new SPxScDestDisplayWin();
			userDate->dispWin->Create(0, screenWidth, screenHeight, this);
			/* Set the update interval in msecs. */
			userDate->dispWin->SetUpdateInterval(30);

			/* Create the scan converter, telling it which display window to use. */
			userDate->sc = new SPxScSourceLocal(userDate->dispWin);
			userDate->sc->SetFade(SPX_RADAR_FADE_REPLACE, 0);
			//userDate->sc->SetRadarColour(0, 0, 255, 0);	/* Green */  /* 0, 255, 165, 0 Orange */

			UINT32 color = RADAR_COLOR_ARRAY[radarIndex];
			userDate->sc->SetRadarColour(0, GetRValue(color), GetGValue(color), GetBValue(color));
			userDate->sc->SetRadarBright(0, 150);

			m_followWin->AddSC(userDate->sc);
			//userDate->sc->SetViewControl(m_viewCtrl, radarIndex, 0);
			userDate->sc->SetViewControl(m_viewCtrl, radarIndex, radarIndex);

			userDate->scProcess = new SPxRunProcess(SPxProScanConv, NULL,
				channelRadar->GetPIM(), userDate->sc);

			SPxLatLong_t pos;
			channelRadar->GetLatLong(&pos.latDegs, &pos.longDegs);
			double xMetres = 0;
			double yMetres = 0;
			m_viewCtrl->LatLongToMetres(&pos, &xMetres, &yMetres);

			//SPxLatLong_t platformLL = { 31.349948, 121.904502 };
			/*SPxLatLong_t platformLL = m_radarLatLong;
			SPxLatLongDiffMetres(platformLL.latDegs, platformLL.longDegs
				, pos.latDegs, pos.longDegs, &xMetres, &yMetres, SPX_LATLONG_CONV_MODE_UTM);*/ //计算有误差
			
			/*CoordinateTransformation tf;
			double r = 0;
			double a = 0;
			tf.PolarCoordinates_Calculate(platformLL.longDegs, platformLL.latDegs, pos.longDegs, pos.latDegs, a, r);
		    m_viewCtrl->r*/


			m_viewCtrl->SetPlatformLatLong(&pos, radarIndex);
			
			//m_viewCtrl->SetDeviceOffsetMetres(xMetres, yMetres, radarIndex, 0);
			
		

			//注意：只能通过nav将雷达位置信息传递到viewCtrl
			//m_navData->SetRadarOffsetMetres(xMetres, yMetres, radarIndex);

			//userDate->sc->ShowRadar(0, SPX_SC_STATE_STOP_AND_CLEAR);
		}
	}
}



