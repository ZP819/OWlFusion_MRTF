#include "stdafx.h"
#include "Fusion.h"

const char* AISTrackParamKey = "AISTrackParam";
const char* RadarTrackInternalParamKey = "RadarTrackInternalParam";

Fusion::Fusion(SPxUniTrackDatabase* uniTrackDb
	, SPxTrackDatabase* trackDb
	, SPxAISDecoder* aisDecoder)
{
	m_uniTrackDb = uniTrackDb;
	m_trackDb = trackDb;
	m_aisDecoder = aisDecoder;

	//初始化
	isSendNoFusedRadarTrack = true;//false;
	mut.Initialise();

	m_fuseUserParam.isFuseActive = true;
	m_fuseUserParam.isSpeedRuleActive = false;
	m_fuseUserParam.isCourseRuleActive = false;
	m_fuseUserParam.radarAziObsDiff = 0.2; //度
	m_fuseUserParam.radarDisObsDiff = 5;   //m
	m_fuseUserParam.GpsDisObsDiff = 3;     //m
	m_fuseUserParam.ThreshStart = TWO;
	m_fuseUserParam.ThreshMaintain = FOUR;
	/*m_fuseUserParam.startActiveRangeMetres = 50;
	m_fuseUserParam.StartAisExtrapTimeSecs = 60;
	m_fuseUserParam.MaintainAisExtrapTimeSecs = 180;*/
	m_fuseUserParam.minSpeedMps = 0.5;

	m_trackFusedSender = new SPxPacketSender();
	m_trackFusedSender->SetName("FusedTrackSender");
	//m_trackFusedSender->SetAddress("239.192.50.99", 5099);
	m_trackFusedSender->SetStateFromConfig();
	
	m_trackFusedSenderLocal = new SPxPacketSender();
	m_trackFusedSenderLocal->SetAddress("127.0.0.1", 7771);
	
	m_trackDb->GetSensorLatLong(&m_radarPos.latDegs, &m_radarPos.longDegs);
	
	/* 20230609 在UniTrackDB中捕获所有目标的状态 */
	//m_uniTrackDb->AddEventCallback(UniTrackDBEventFunc,this); //20240102 暂时delete

	/*m_aisDecoder->AddCallback(AISHandlerFunc, NULL);
	m_trackDb->AddCallback(TrackFuseAISHandlerFunc, this);*/
}

int Fusion::UniTrackDBEventFunc(void *invokingObject,
	void *userObject,
	void *arg)
{
	Fusion* fusion = (Fusion*)userObject;
	SPxUniTrackDatabase::Event_t* ev = (SPxUniTrackDatabase::Event_t*)arg;
	if (ev->type == SPxUniTrackDatabase::EVENT_TRACK_CREATE)
	{
		SPxUniTrack* track = ev->info.trackEvt.track;
		UINT32 trackType = track->GetTrackType();
		if (trackType == SPxUniTrack::TRACK_TYPE_RADAR)
		{
			RadarTrackInternalParam* param =
				(RadarTrackInternalParam*)track->GetUserData(RadarTrackInternalParamKey);
			if (!param)
			{
				param = new RadarTrackInternalParam();
				memset(param, 0, sizeof(RadarTrackInternalParam));
				track->SetUserData(param, RadarTrackInternalParamKey);
			}
			fusion->fuse((SPxRadarTrack*)track);//执行融合
		}
		if (trackType == SPxUniTrack::TRACK_TYPE_AIS)
		{
			AISTrackParam* param = (AISTrackParam*)track->GetUserData(AISTrackParamKey);
			if (!param)
			{
				param = new AISTrackParam();
				memset(param, 0, sizeof(AISTrackParam));
				track->SetUserData(param, AISTrackParamKey);
			}
		}
	}
	if (ev->type == SPxUniTrackDatabase::EVENT_TRACK_UPDATE)
	{
		SPxUniTrack* track = ev->info.trackEvt.track;
		UINT32 trackType = track->GetTrackType();
		if (trackType == SPxUniTrack::TRACK_TYPE_RADAR)
		{
			fusion->fuse((SPxRadarTrack*)track);//执行融合
		}
	}
	if (ev->type == SPxUniTrackDatabase::EVENT_TRACK_DELETE)
	{
		SPxUniTrack* track = ev->info.trackEvt.track;
		UINT32 trackType = track->GetTrackType();
		//直接在trackdatabase中仅能捕获航迹更新状态，删除状态不一定能捕获，SPxUniTrackDatabase应该可以捕获删除，待验证
		if (trackType == SPxUniTrack::TRACK_TYPE_RADAR)
		{
			fusion->fuse((SPxRadarTrack*)track);//执行融合
		}
		if (trackType == SPxUniTrack::TRACK_TYPE_AIS)
		{
			AISTrackParam* param = (AISTrackParam*)track->GetUserData(AISTrackParamKey);
			if (!param)
			{
				delete param;
			}
		}
	}

	return 0;
}

Fusion::~Fusion()
{
	//m_aisDecoder->RemoveCallback(AISHandlerFunc, NULL);
	m_uniTrackDb->RemoveEventCallback(UniTrackDBEventFunc, this);
}


int Fusion::AISHandlerFunc(void *invokingObject,
	void *userObject,
	void *arg)
{
	SPxAISDecoder* dec = (SPxAISDecoder*)invokingObject;
	SPxAISTrack * track = (SPxAISTrack*)arg;
	UINT32 flags = track->GetFlags();
	
	if (flags & SPX_AIS_TRACK_FLAGS_CREATED)
	{
		AISTrackParam* param = (AISTrackParam*)track->GetUserData(AISTrackParamKey);
		if (!param)
		{
			param = new AISTrackParam();
			memset(param, 0, sizeof(AISTrackParam));

			track->SetUserData(param, AISTrackParamKey);
		}
	}
	if (flags & SPX_AIS_TRACK_FLAGS_DELETED)
	{
		int isdel = track->GetIsDeleted();
		AISTrackParam* param = (AISTrackParam*)track->GetUserData(AISTrackParamKey);
		if (!param)
		{
			delete param;
		}
	}

	return 0;
}

void Fusion::fuse(SPxRadarTrack* radarTrack)
{
	SPxAutoLock alock(&mut);

	if (!m_fuseUserParam.isFuseActive)
		return;

	if (m_aisDecoder->GetNumTracks() <= 0)
		return;
	
	/* 1.确认内部参数有效 */
	checkInternalParam(radarTrack);
	RadarTrackInternalParam* param =
		(RadarTrackInternalParam*)radarTrack->GetUserData(RadarTrackInternalParamKey);

	/* 2.确认是否调试 */
	checkDebug(radarTrack);

	/* 3.融合 */
	if (!param->isFused)
	{
		startFuse(radarTrack);
	}
	else
	{
		maintainFuse(radarTrack);
	}
	
	/* 4.对外推送，包括删除包 */
	sendTrack(radarTrack);

	/* 5.重置内部参数 */
	checkDeleted(radarTrack);
}

void Fusion::startFuse(SPxRadarTrack* radarTrack)
{
	RadarTrackInternalParam* param =
		(RadarTrackInternalParam*)radarTrack->GetUserData(RadarTrackInternalParamKey);

	/* 初始化参数 */


	/* 雷达航迹信息 */
	SPxLatLong_t radarTrackPos = { 0 };
	radarTrack->GetLatLong(&radarTrackPos.latDegs, &radarTrackPos.longDegs);
	const SPxTime_t* radarTimestamp = radarTrack->GetTimestamp();
	UINT32 radarTrackID = radarTrack->GetID();
	double radarTrackRangeMetres = radarTrack->GetRangeMetres();
	double radarTrackAziDegs = radarTrack->GetAzimuthDeg();

	/* 匹配AIS目标 */
	double disMetres = 0;
	SPxAISTrack* aisTrack = NULL;
	if (param->hitCount == 0) /* 首次执行hit,用FindNearestTrack匹配 */
	{
		aisTrack = m_aisDecoder->FindNearestTrack(
			radarTrackPos.latDegs, radarTrackPos.longDegs, &disMetres, radarTimestamp, 1);
	}
	else
	{
		aisTrack = m_aisDecoder->GetTrack(param->mmsi);
	}
	
	if (!aisTrack)
	{
		if (param->isDebug)
		{
			char buf[1024] = { 0 };
			sprintf_s(buf, "%d 未匹配到合适的AIS目标 ", radarTrackID);
			OWlError(OWL_ERR_INFO, buf);
		}
		param->hitCount = 0;//重新开始起始
		param->mmsi = 0;
		return;
	}

	UINT32 mmsi = aisTrack->GetMMSI();
	if (mmsi == 0)
	{
		//if (param->isDebug)
		{
			char buf[1024] = { 0 };
			sprintf_s(buf, "%d mmsi=%ld ", radarTrackID, mmsi);
			OWlError(OWL_ERR_INFO, buf);
		}
		param->hitCount = 0;//重新开始起始
		param->mmsi = 0;
		return;
	}

	AISTrackParam* aisParam = (AISTrackParam*)aisTrack->GetUserData(AISTrackParamKey);
	if (!aisParam)
	{
		//if (param->isDebug)
		{
			char buf[1024] = { 0 };
			sprintf_s(buf, "%d %ld内部参数无效 ", radarTrackID, mmsi);
			OWlError(OWL_ERR_INFO, buf);
		}
		aisParam = new AISTrackParam();
		memset(aisParam, 0, sizeof(AISTrackParam));
		aisTrack->SetUserData(aisParam, AISTrackParamKey);

		param->hitCount = 0;//重新开始起始
		param->mmsi = 0;
		return;
	}

	if (aisParam->isFused)
	{
		if (param->isDebug)
		{
			char buf[1024] = { 0 };
			sprintf_s(buf, "%d %ld已和%d融合 ", radarTrackID, mmsi, aisParam->radarID);
			OWlError(OWL_ERR_INFO, buf);
		}
		param->hitCount = 0;//重新开始起始
		param->mmsi = 0;
		return;
	}


	/* ais目标信息 */
	SPxLatLong_t aisTrackPos = { 0 };
	SPxTime_t aisTrackPosTimestamp = { 0 };
	double aisTrackAgeSecs = 0;//距离最近一次更新位置报文的时长
	SPxLatLong_t aisTrackExtraPos = { 0 };
	SPxTime_t aisTrackExtraPosTimestamp = { 0 };
	double aisTrackRangeMetres = 0;
	double aisTrackAziDegs = 0;
	if (SPX_NO_ERROR !=
		aisTrack->GetPosition(&aisTrackPos.latDegs, &aisTrackPos.longDegs, &aisTrackPosTimestamp, &aisTrackAgeSecs))
	{
		if (param->isDebug)
		{
			char buf[1024] = { 0 };
			sprintf_s(buf, "%d %ld:GetPosition() err ", radarTrackID, mmsi);
			OWlError(OWL_ERR_INFO, buf);
		}
		param->hitCount = 0;//重新开始起始
		param->mmsi = 0;
		return;
	}

	if (SPX_NO_ERROR != aisTrack->GetExtrapolatedPosition(
		&aisTrackExtraPos.latDegs, &aisTrackExtraPos.longDegs, &aisTrackExtraPosTimestamp))
	{
		if (param->isDebug)
		{
			char buf[1024] = { 0 };
			sprintf_s(buf, "%d %ld:GetExtrapolatedPosition() err ", radarTrackID, mmsi);
			OWlError(OWL_ERR_INFO, buf);
		}
		param->hitCount = 0;//重新开始起始
		param->mmsi = 0;
		return;
	}

	/* 融合判断 */
	CoordinateTransformation ct;
	ct.PolarCoordinates_Calculate(
		m_radarPos.longDegs, m_radarPos.latDegs,
		aisTrackExtraPos.longDegs, aisTrackExtraPos.latDegs,
		aisTrackAziDegs, aisTrackRangeMetres);
	double rangeDiffAbs = fabs(radarTrackRangeMetres - aisTrackRangeMetres);
	double aziDiffAbs = fabs(radarTrackAziDegs - aisTrackAziDegs);

	//方位的差值<方位尺寸A-Size/2  + 2*（雷达的方位观察误差+AIS位置误差_方位）
	//距离的差值<距离尺寸R - Size / 2 + 2 * （雷达的距离观察误差 + AIS位置误差_距离）
	//雷达观测误差
	double radarAziObsDiff = m_fuseUserParam.radarAziObsDiff;
	double radarDisObsDiff = m_fuseUserParam.radarDisObsDiff;
	//GPS定位误差
	double GpsDisObsDiff = m_fuseUserParam.GpsDisObsDiff;
	double GpsAziObsDiff = 0;
	//AIS外推误差
	double AisDisExtrapDiff = 0;
	double AisAziExtrapDiff = 0;
	//AIS总误差
	double AisDisTotalDiff = 0;
	double AisAziTotalDiff = 0;
	//初始为2倍融合门限
	double disAssocThresh = 0;
	double aziAssocThresh = 0;
	//航向航速融合门限
	double speedThresh = 9999;
	double courseThresh = 360;

	/* 根据当前AIS目标更新融合门限 */
	//GPS定位误差
	GpsDisObsDiff = GpsDisObsDiff * sqrt(2);
	GpsAziObsDiff = atan(GpsDisObsDiff / aisTrackRangeMetres) * 180 / PI;
	//AIS外推误差
	AisDisExtrapDiff = 0.2 * aisTrackAgeSecs * sqrt(2);
	AisAziExtrapDiff = atan(AisDisExtrapDiff / aisTrackRangeMetres) * 180 / PI;
	//AIS总误差
	AisDisTotalDiff = sqrt(AisDisExtrapDiff * AisDisExtrapDiff
		+ GpsDisObsDiff * GpsDisObsDiff);
	AisAziTotalDiff = sqrt(AisAziExtrapDiff * AisAziExtrapDiff
		+ GpsAziObsDiff * GpsAziObsDiff);
	//初始为2倍融合门限
	disAssocThresh = radarTrack->GetNormRpt()->measSizeMetres / 2
		+ (int)(m_fuseUserParam.ThreshStart) * (radarDisObsDiff + AisDisTotalDiff);
	aziAssocThresh = radarTrack->GetNormRpt()->measSizeDegrees / 2
		+ (int)(m_fuseUserParam.ThreshStart) * (radarAziObsDiff + AisAziTotalDiff);

	bool isRangeRuleHit = false;
	bool isAziRuleHit = false;
	bool isSpeedRuleHit = false;
	bool isCourseRuleHit = false;
	isSpeedRuleHit = true;
	isCourseRuleHit = true;

	if (disAssocThresh + EPS > rangeDiffAbs)
	{
		isRangeRuleHit = true;
	}
	else
	{
		if (param->isDebug)
		{
			char buf[1024] = { 0 };
			sprintf_s(buf, "%d %ld 距离未过门限 rd=%0.2f rt=%0.2f rr=%0.2f ra=%0.2f", radarTrackID, mmsi
				, rangeDiffAbs, disAssocThresh, radarTrackRangeMetres, aisTrackRangeMetres);
			OWlError(OWL_ERR_INFO, buf);
		}
	}

	if (aziAssocThresh + EPS > aziDiffAbs)
	{
		isAziRuleHit = true;
	}
	else
	{
		if (param->isDebug)
		{
			char buf[1024] = { 0 };
			sprintf_s(buf, "%d %ld 方位未过门限 ad=%0.2f at=%0.2f ar=%0.2f aa=%0.2f", radarTrackID, mmsi
				, aziDiffAbs, aziAssocThresh, radarTrackAziDegs, aisTrackAziDegs);
			OWlError(OWL_ERR_INFO, buf);

		}
	}

	if (m_fuseUserParam.isSpeedRuleActive || m_fuseUserParam.isCourseRuleActive)
	{
		double radarTrackSpeed = 0;
		double radarTrackCourse = 0;
		if (SPX_NO_ERROR != radarTrack->GetVelocity(&radarTrackSpeed, &radarTrackCourse))
		{
			if (param->isDebug)
			{
				char buf[1024] = { 0 };
				sprintf_s(buf, "%d %ld 雷达航迹无航向航速信息", radarTrackID, mmsi);
				OWlError(OWL_ERR_INFO, buf);
			}
		}
		else
		{
			double aisTrackSpeed = 0;
			double aisTrackCourse = 0;
			if (SPX_NO_ERROR != aisTrack->GetVelocity(&aisTrackSpeed, &aisTrackCourse))
			{
				if (param->isDebug)
				{
					char buf[1024] = { 0 };
					sprintf_s(buf, "%d %ld AIS无航向航速信息", radarTrackID, mmsi);
					OWlError(OWL_ERR_INFO, buf);
				}
			}
			else
			{

				/* 航速准则 */
				if (m_fuseUserParam.isSpeedRuleActive)
				{
					//航速符合融合要求
					double speedDiffAbs = fabs(radarTrackSpeed - aisTrackSpeed);
					speedThresh = radarTrack->GetExtRpt()->speedSD * 2 * m_fuseUserParam.ThreshStart + 0.5;//0.5余量
					if (speedThresh + EPS > speedDiffAbs)
					{
						isSpeedRuleHit = true;
					}
					else
					{
						if (param->isDebug)
						{
							char buf[1024] = { 0 };
							sprintf_s(buf, "%d %ld 航速未过门限 sd=%0.2f st=%0.2f rs=%0.2f as=%0.2f", radarTrackID, mmsi
								, speedDiffAbs, speedThresh, radarTrackSpeed, aisTrackSpeed);
							OWlError(OWL_ERR_INFO, buf);

						}
						isSpeedRuleHit = false;
					}
				}
				/* 航向准则 */
				if (m_fuseUserParam.isCourseRuleActive)
				{
					if (radarTrackSpeed < m_fuseUserParam.minSpeedMps
						|| aisTrackSpeed < m_fuseUserParam.minSpeedMps)
					{
						if (param->isDebug)
						{
							char buf[1024] = { 0 };
							sprintf_s(buf, "%d %ld 航速过低,不考虑航速准则 s=%0.2f as=%0.2f", radarTrackID, mmsi, radarTrackSpeed, aisTrackSpeed);
							OWlError(OWL_ERR_INFO, buf);
						}
					}
					else
					{
						double courseDiffAbs = fabs(radarTrackCourse - aisTrackCourse);
						courseThresh = radarTrack->GetExtRpt()->courseSD * 2 * m_fuseUserParam.ThreshStart + 5;//5余量
						if (courseThresh + EPS > courseDiffAbs)
						{
							isCourseRuleHit = true;
						}
						else
						{
							if (param->isDebug)
							{
								char buf[1024] = { 0 };
								sprintf_s(buf, "%d %ld 航向未过门限 cd=%0.2f ct=%0.2f rc=%0.2f ac=%0.2f", radarTrackID, mmsi
									, courseDiffAbs, courseThresh, radarTrackCourse, aisTrackCourse);
								OWlError(OWL_ERR_INFO, buf);

							}
							isCourseRuleHit = false;
						}
					}
				}
			}
		}
	}

	if (isRangeRuleHit && isAziRuleHit && isSpeedRuleHit && isCourseRuleHit)
	{
		//成功融合
		param->hitCount++;

		if (param->hitCount == 1)
		{
			param->mmsi = mmsi;//第一次hit成功 存储mmsi
		}

		if (param->hitCount >= FUSE_HIT_TIMES_THRESH)
		{
			param->isFused = 1;
			param->age = param->hitCount;
			//ais融合信息更新
			aisParam->isFused = 1;
			aisParam->radarID = radarTrackID;
		}

		if (param->isDebug)
		{
			char buf[1024] = { 0 };
			sprintf_s(buf, "hit ok! %d %ld rd=%0.2f rt= %0.2f ad=%0.2f at=%0.2f dis=%0.2f hitCount=%d", radarTrackID, mmsi
				, rangeDiffAbs, disAssocThresh, aziDiffAbs, aziAssocThresh, disMetres, param->hitCount);
			OWlError(OWL_ERR_INFO, buf);
		}
	}
	else
	{
		if (param->isDebug)
		{
			char buf[1024] = { 0 };
			sprintf_s(buf, "hit fail %d %ld rd=%0.2f rt= %0.2f ad=%0.2f at=%0.2f dis=%0.2f hitCount=%d", radarTrackID, mmsi
				, rangeDiffAbs, disAssocThresh, aziDiffAbs, aziAssocThresh, disMetres, param->hitCount);
			OWlError(OWL_ERR_INFO, buf);
		}
		param->hitCount = 0;
		param->mmsi = 0;
	}
}

void Fusion::maintainFuse(SPxRadarTrack* radarTrack)
{
	RadarTrackInternalParam* param =
		(RadarTrackInternalParam*)radarTrack->GetUserData(RadarTrackInternalParamKey);

	/* 雷达航迹信息 */
	SPxLatLong_t radarTrackPos = { 0 };
	radarTrack->GetLatLong(&radarTrackPos.latDegs, &radarTrackPos.longDegs);
	const SPxTime_t* radarTimestamp = radarTrack->GetTimestamp();
	UINT32 radarTrackID = radarTrack->GetID();
	double radarTrackRangeMetres = radarTrack->GetRangeMetres();
	double radarTrackAziDegs = radarTrack->GetAzimuthDeg();

	/* 雷达航迹有效性确认 */
	if (radarTrack->GetIsDeleted())
	{
		param->isDeleted = 1;//删除融合
		return;
	}


	/* AIS有效性确认 */
	double disMetres = 0;
	SPxAISTrack* aisTrack = NULL;
	aisTrack = m_aisDecoder->GetTrack(param->mmsi);
	if (!aisTrack)//这种情况理论上不会出现，防止AIS被删
	{
		//if (param->isDebug)
		{
			char buf[1024] = { 0 };
			sprintf_s(buf, "%d %ldAIS目标无效，融合失效 ", radarTrackID, param->mmsi);
			OWlError(OWL_ERR_INFO, buf);
		}
		param->isDeleted = 1;//删除融合
		return;
	}

	UINT32 mmsi = aisTrack->GetMMSI();
	if (mmsi == 0)
	{
		//if (param->isDebug)
		{
			char buf[1024] = { 0 };
			sprintf_s(buf, "%d 关联AIS%ld 此时mmsi为0，融合失效", radarTrackID, param->mmsi);
			OWlError(OWL_ERR_INFO, buf);
		}
		param->isDeleted = 1;//删除融合
		return;
	}

	AISTrackParam* aisParam = (AISTrackParam*)aisTrack->GetUserData(AISTrackParamKey);
	if (!aisParam)//这种情况理论上不会出现，防止AIS被删
	{
		//if (param->isDebug)
		{
			char buf[1024] = { 0 };
			sprintf_s(buf, "%d %ld内部参数无效，融合失效 ", radarTrackID, mmsi);
			OWlError(OWL_ERR_INFO, buf);
		}
		param->isDeleted = 1;//删除融合
		return;
	}

	/* ais目标信息 */
	SPxLatLong_t aisTrackPos = { 0 };
	SPxTime_t aisTrackPosTimestamp = { 0 };
	double aisTrackAgeSecs = 0;//距离最近一次更新位置报文的时长
	SPxLatLong_t aisTrackExtraPos = { 0 };
	SPxTime_t aisTrackExtraPosTimestamp = { 0 };
	double aisTrackRangeMetres = 0;
	double aisTrackAziDegs = 0;
	if (SPX_NO_ERROR !=
		aisTrack->GetPosition(&aisTrackPos.latDegs, &aisTrackPos.longDegs, &aisTrackPosTimestamp, &aisTrackAgeSecs))
	{
		if (param->isDebug)
		{
			char buf[1024] = { 0 };
			sprintf_s(buf, "%d %ld:GetPosition() err，融合失效", radarTrackID, mmsi);
			OWlError(OWL_ERR_INFO, buf);
		}
		param->isDeleted = 1;//删除融合
		return;
	}

	if (SPX_NO_ERROR != aisTrack->GetExtrapolatedPosition(
		&aisTrackExtraPos.latDegs, &aisTrackExtraPos.longDegs, &aisTrackExtraPosTimestamp))
	{
		if (param->isDebug)
		{
			char buf[1024] = { 0 };
			sprintf_s(buf, "%d %ld:GetExtrapolatedPosition() err，融合失效", radarTrackID, mmsi);
			OWlError(OWL_ERR_INFO, buf);
		}
		param->isDeleted = 1;//删除融合
		return;
	}

	/* 融合判断 */
	CoordinateTransformation ct;
	ct.PolarCoordinates_Calculate(
		m_radarPos.longDegs, m_radarPos.latDegs,
		aisTrackExtraPos.longDegs, aisTrackExtraPos.latDegs,
		aisTrackAziDegs, aisTrackRangeMetres);
	double rangeDiffAbs = fabs(radarTrackRangeMetres - aisTrackRangeMetres);
	double aziDiffAbs = fabs(radarTrackAziDegs - aisTrackAziDegs);

	//方位的差值<方位尺寸A-Size/2  + 2*（雷达的方位观察误差+AIS位置误差_方位）
	//距离的差值<距离尺寸R - Size / 2 + 2 * （雷达的距离观察误差 + AIS位置误差_距离）
	//雷达观测误差
	double radarAziObsDiff = m_fuseUserParam.radarAziObsDiff;
	double radarDisObsDiff = m_fuseUserParam.radarDisObsDiff;
	//GPS定位误差
	double GpsDisObsDiff = m_fuseUserParam.GpsDisObsDiff;
	double GpsAziObsDiff = 0;
	//AIS外推误差
	double AisDisExtrapDiff = 0;
	double AisAziExtrapDiff = 0;
	//AIS总误差
	double AisDisTotalDiff = 0;
	double AisAziTotalDiff = 0;
	//初始为2倍融合门限
	double disAssocThresh = 0;
	double aziAssocThresh = 0;
	//航向航速融合门限
	double speedThresh = 9999;
	double courseThresh = 360;

	/* 根据当前AIS目标更新融合门限 */
	//GPS定位误差
	GpsDisObsDiff = GpsDisObsDiff * sqrt(2);
	GpsAziObsDiff = atan(GpsDisObsDiff / aisTrackRangeMetres) * 180 / PI;
	//AIS外推误差
	AisDisExtrapDiff = 0.2 * aisTrackAgeSecs * sqrt(2);
	AisAziExtrapDiff = atan(AisDisExtrapDiff / aisTrackRangeMetres) * 180 / PI;
	//AIS总误差
	AisDisTotalDiff = sqrt(AisDisExtrapDiff * AisDisExtrapDiff
		+ GpsDisObsDiff * GpsDisObsDiff);
	AisAziTotalDiff = sqrt(AisAziExtrapDiff * AisAziExtrapDiff
		+ GpsAziObsDiff * GpsAziObsDiff);
	//初始为2倍融合门限
	disAssocThresh = radarTrack->GetNormRpt()->measSizeMetres / 2
		+ (int)(m_fuseUserParam.ThreshMaintain) * (radarDisObsDiff + AisDisTotalDiff);
	aziAssocThresh = radarTrack->GetNormRpt()->measSizeDegrees / 2
		+ (int)(m_fuseUserParam.ThreshMaintain) * (radarAziObsDiff + AisAziTotalDiff);

	bool isRangeRuleHit = false;
	bool isAziRuleHit = false;
	bool isSpeedRuleHit = false;
	bool isCourseRuleHit = false;
	//为了不影响基于位置的融合判断，此处设置为hit
	isSpeedRuleHit = true;
	isCourseRuleHit = true;

	if (disAssocThresh + EPS > rangeDiffAbs)
	{
		isRangeRuleHit = true;
	}
	else
	{
		if (param->isDebug)
		{
			char buf[1024] = { 0 };
			sprintf_s(buf, "%d %ld 距离未过门限 rd=%0.2f rt=%0.2f rr=%0.2f ra=%0.2f", radarTrackID, mmsi
				, rangeDiffAbs, disAssocThresh, radarTrackRangeMetres, aisTrackRangeMetres);
			OWlError(OWL_ERR_INFO, buf);
		}
	}

	if (aziAssocThresh + EPS > aziDiffAbs)
	{
		isAziRuleHit = true;
	}
	else
	{
		if (param->isDebug)
		{
			char buf[1024] = { 0 };
			sprintf_s(buf, "%d %ld 方位未过门限 ad=%0.2f at=%0.2f ar=%0.2f aa=%0.2f", radarTrackID, mmsi
				, aziDiffAbs, aziAssocThresh, radarTrackAziDegs, aisTrackAziDegs);
			OWlError(OWL_ERR_INFO, buf);

		}
	}

	if (m_fuseUserParam.isSpeedRuleActive || m_fuseUserParam.isCourseRuleActive)
	{
		double radarTrackSpeed = 0;
		double radarTrackCourse = 0;
		if (SPX_NO_ERROR != radarTrack->GetVelocity(&radarTrackSpeed, &radarTrackCourse))
		{
			if (param->isDebug)
			{
				char buf[1024] = { 0 };
				sprintf_s(buf, "%d %ld 雷达航迹无航向航速信息", radarTrackID, mmsi);
				OWlError(OWL_ERR_INFO, buf);
			}
		}
		else
		{
			double aisTrackSpeed = 0;
			double aisTrackCourse = 0;
			if (SPX_NO_ERROR != aisTrack->GetVelocity(&aisTrackSpeed, &aisTrackCourse))
			{
				if (param->isDebug)
				{
					char buf[1024] = { 0 };
					sprintf_s(buf, "%d %ld AIS无航向航速信息", radarTrackID, mmsi);
					OWlError(OWL_ERR_INFO, buf);
				}
			}
			else
			{

				/* 航速准则 */
				if (m_fuseUserParam.isSpeedRuleActive)
				{
					//航速符合融合要求
					double speedDiffAbs = fabs(radarTrackSpeed - aisTrackSpeed);
					speedThresh = radarTrack->GetExtRpt()->speedSD * 2 * m_fuseUserParam.ThreshMaintain + 0.5;//0.5余量
					if (speedThresh + EPS > speedDiffAbs)
					{
						isSpeedRuleHit = true;
					}
					else
					{
						if (param->isDebug)
						{
							char buf[1024] = { 0 };
							sprintf_s(buf, "%d %ld 航速未过门限 sd=%0.2f st=%0.2f rs=%0.2f as=%0.2f", radarTrackID, mmsi
								, speedDiffAbs, speedThresh, radarTrackSpeed, aisTrackSpeed);
							OWlError(OWL_ERR_INFO, buf);

						}
						isSpeedRuleHit = false;
					}
				}
				/* 航向准则 */
				if (m_fuseUserParam.isCourseRuleActive)
				{
					if (radarTrackSpeed < m_fuseUserParam.minSpeedMps
						|| aisTrackSpeed < m_fuseUserParam.minSpeedMps)
					{
						if (param->isDebug)
						{
							char buf[1024] = { 0 };
							sprintf_s(buf, "%d %ld 航速过低,不考虑航速准则 s=%0.2f as=%0.2f", radarTrackID, mmsi, radarTrackSpeed, aisTrackSpeed);
							OWlError(OWL_ERR_INFO, buf);
						}
					}
					else
					{
						double courseDiffAbs = fabs(radarTrackCourse - aisTrackCourse);
						courseThresh = radarTrack->GetExtRpt()->courseSD * 2 * m_fuseUserParam.ThreshMaintain + 5;//5余量
						if (courseThresh + EPS > courseDiffAbs)
						{
							isCourseRuleHit = true;
						}
						else
						{
							if (param->isDebug)
							{
								char buf[1024] = { 0 };
								sprintf_s(buf, "%d %ld 航向未过门限 cd=%0.2f ct=%0.2f rc=%0.2f ac=%0.2f", radarTrackID, mmsi
									, courseDiffAbs, courseThresh, radarTrackCourse, aisTrackCourse);
								OWlError(OWL_ERR_INFO, buf);

							}
							isCourseRuleHit = false;
						}
					}
				}
			}
		}	
	}

	if (isRangeRuleHit && isAziRuleHit && isSpeedRuleHit && isCourseRuleHit)
	{
		//成功融合
		param->age++;
		param->coastCount = 0;

		if (param->isDebug)
		{
			char buf[1024] = { 0 };
			sprintf_s(buf, "fuse ok! %d %ld rd=%0.2f rt= %0.2f ad=%0.2f at=%0.2f a=%d", radarTrackID, param->mmsi
				, rangeDiffAbs, disAssocThresh, aziDiffAbs, aziAssocThresh, param->age);
			OWlError(OWL_ERR_INFO, buf);
		}
	}
	else
	{
		param->coastCount++;
		if (param->coastCount >= COAST_MAX_NUMS)
		{
			param->isDeleted = 1;
		}
		if (param->isDebug)
		{
			char buf[1024] = { 0 };
			sprintf_s(buf, "co=%d %d %ld rd=%0.2f rt= %0.2f ad=%0.2f at=%0.2f a=%d", param->coastCount, radarTrackID, param->mmsi
				, rangeDiffAbs, disAssocThresh, aziDiffAbs, aziAssocThresh, param->age);
			OWlError(OWL_ERR_INFO, buf);
		}
	}
}

void Fusion::checkInternalParam(SPxRadarTrack* radarTrack)
{
	RadarTrackInternalParam* param =
		(RadarTrackInternalParam*)radarTrack->GetUserData(RadarTrackInternalParamKey);
	if (!param)
	{
		param = new RadarTrackInternalParam();
		memset(param, 0, sizeof(RadarTrackInternalParam));
		radarTrack->SetUserData(param, RadarTrackInternalParamKey);
		
		/*char buf[1024] = { 0 };
		sprintf_s(buf, "新建内部 ID=%d", radarTrack->GetID());
		OWlError(OWL_ERR_INFO, buf);*/
	}
}

void Fusion::checkDebug(SPxRadarTrack* radarTrack)
{
	RadarTrackInternalParam* param =
		(RadarTrackInternalParam*)radarTrack->GetUserData(RadarTrackInternalParamKey);
	SPxUniTrack* selectedTrack = m_uniTrackDb->GetSelectedTrack();
	if (selectedTrack)
	{
		if (selectedTrack->GetID() == radarTrack->GetID())
		{
			param->isDebug = 1;
		}
		else
		{
			param->isDebug = 0;
		}
	}
	else
	{
		param->isDebug = 0;
	}
}

bool Fusion::isInActiveRange(SPxRadarTrack* track)
{
	bool ret = false;

	return ret;
}

void Fusion::sendTrack(SPxRadarTrack* radarTrack)
{
	RadarTrackInternalParam* param =
		(RadarTrackInternalParam*)radarTrack->GetUserData(RadarTrackInternalParamKey);
	if (param->isFused)
	{
		SPxPacketTrackExtended fusedPacket = *radarTrack->GetExtRpt();
		if (radarTrack->GetIsDeleted() || param->isDeleted)
		{
			fusedPacket.norm.min.status = SPX_PACKET_TRACK_STATUS_DELETED;
		}

		fusedPacket.extMask = 0xffffffff;//发送掩码，默认值不发送fusion
		fusedPacket.fusion.sensorTypes = SPX_PACKET_TRACK_SENSOR_AIS;  //SPX_PACKET_TRACK_SENSOR_AIS 显示格式取决于这个，1是雷达，2是AIS
		fusedPacket.fusion.sensors = 2;      //sensor0=0 雷达  sensor1 = 1 AIS
		fusedPacket.fusion.flags = 0;//SPX_PACKET_TRACK_FUSE_FLAG_ALL_COASTED
		fusedPacket.secondary.sensorType = SPX_PACKET_TRACK_SENSOR_AIS;
		fusedPacket.secondary.uniqueID = param->mmsi;
		//memcpy(fusedPacket.secondary.name, aisTrack->GetName().c_str(), aisTrack->GetName().size());

		const int bufLen = sizeof(SPxPacketTrackExtended);
		unsigned char buf[bufLen] = { 0 };
		SPxRemoteServer::TrackPackNet(&fusedPacket.norm.min, &fusedPacket.norm, &fusedPacket, buf, bufLen);
		SPxTime_t now;
		SPxTimeGetEpoch(&now);

		if (m_trackFusedSender->SendPacketB(SPX_PACKET_TYPEB_TRACK_EXT, now, &buf, bufLen) == SPX_NO_ERROR)
		{
			m_trackFusedSenderLocal->SendPacketB(SPX_PACKET_TYPEB_TRACK_EXT, now, &buf, bufLen);
			if (param->isDebug)
			{
				char buf[1024] = { 0 };
				sprintf_s(buf, "%d %ld 发送", radarTrack->GetID(), param->mmsi);
				//OWlError(OWL_ERR_INFO, buf);
			}
		}
		else
		{
			//发送失败
		}
	}

	if (!param->isFused && isSendNoFusedRadarTrack)
	{
		SPxPacketTrackExtended fusedPacket = *radarTrack->GetExtRpt();

		fusedPacket.extMask = 0xffffffff;//发送掩码，默认值不发送fusion
		fusedPacket.fusion.sensorTypes = SPX_PACKET_TRACK_SENSOR_PRIMARY;  //SPX_PACKET_TRACK_SENSOR_AIS 显示格式取决于这个，1是雷达，2是AIS
		fusedPacket.fusion.sensors = 1;      //sensor0=0 雷达  sensor1 = 1 AIS
		fusedPacket.fusion.flags = 0;//SPX_PACKET_TRACK_FUSE_FLAG_ALL_COASTED
		fusedPacket.secondary.sensorType = SPX_PACKET_TRACK_SENSOR_PRIMARY;
		const int bufLen = sizeof(SPxPacketTrackExtended);
		unsigned char buf[bufLen] = { 0 };
		SPxRemoteServer::TrackPackNet(&fusedPacket.norm.min, &fusedPacket.norm, &fusedPacket, buf, bufLen);
		SPxTime_t now;
		SPxTimeGetEpoch(&now);
		m_trackFusedSender->SendPacketB(SPX_PACKET_TYPEB_TRACK_EXT, now, &buf, bufLen);
	}
}

void Fusion::checkDeleted(SPxRadarTrack* radarTrack)
{
	RadarTrackInternalParam* param =
		(RadarTrackInternalParam*)radarTrack->GetUserData(RadarTrackInternalParamKey);
	if (param->isDeleted)
	{
		if (param->isDebug)
		{
			char buf[1024] = { 0 };
			sprintf_s(buf, "%d %ld 融合失败，删除", radarTrack->GetID(), param->mmsi);
			OWlError(OWL_ERR_INFO, buf);
		}
		SPxAISTrack* aisTrack = NULL;
		aisTrack = m_aisDecoder->GetTrack(param->mmsi);
		if (aisTrack)
		{
			AISTrackParam* aisParam = (AISTrackParam*)aisTrack->GetUserData(AISTrackParamKey);
			if (aisParam)
			{
				aisParam->isFused = 0;
				aisParam->radarID = 0;
			}
		}
		memset(param, 0, sizeof(RadarTrackInternalParam));
	}

	if (radarTrack->GetIsDeleted())
	{
		RadarTrackInternalParam* param =
			(RadarTrackInternalParam*)radarTrack->GetUserData(RadarTrackInternalParamKey);
		if (param)
		{
			delete param;
		}
	}
}

int Fusion::TrackFuseAISHandlerFunc(void *invokingObject,
	void *userObject,
	void *arg)
{
	Fusion* fusion = (Fusion*)userObject;
	SPxRadarTrack * radarTrack = (SPxRadarTrack*)arg;
	fusion->fuse(radarTrack);

	return 0;
}
