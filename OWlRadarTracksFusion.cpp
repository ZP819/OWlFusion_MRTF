
/******************************************************************************************************************************************************************/


#include "stdafx.h"
#include "OWlRadarTracksFusion.h"

#include <queue>

#define RADAR_SENSORS_MAX_NUM 10
#define FUSION_HIT_COUNT_THRESH 3 //连续命中几次正式并入融合
#define FUSION_FIAL_COUNT_THRESH 5 //连续失败几次正式确认为独立融合

static const char* g_owlRadarTracksFusionReadParams[] = {
	"radarAziObsDiff",
	"radarDisObsDiff",
	NULL
};

static const char* g_owlRadarTracksFusionWriteParams[] = {
	"radarAziObsDiff",
	"radarDisObsDiff",
	NULL
};

static const double OWL_ASSOCIATION_DISTANCE_METRES = 1000.0;
static const UINT32 OWL_FUSED_TRACK_SENDER_ID = 99;

static void RemoveUnifiedTrackFromVec(vector<UINT32>& trackVec, UINT32 trackUnifiedID)
{
	for (auto it = trackVec.begin(); it != trackVec.end(); )
	{
		if (trackUnifiedID == (*it))
		{
			it = trackVec.erase(it);
		}
		else
		{
			++it;
		}
	}
}

static bool IsTrackFromDifferentSensor(
	SPxUniTrackDatabase* uniTrackDb,
	SPxRadarTrack* radarTrack,
	FusedTrackUserData* fud)
{
	if (!uniTrackDb || !radarTrack || !fud)
	{
		return false;
	}

	int sourceIndex = radarTrack->GetSourceIndex();
	if (sourceIndex == -1)
	{
		return false;
	}

	for (int i = 0; i < fud->trackVec.size(); i++)
	{
		UINT32 uniqueID = fud->trackVec[i];
		SPxRadarTrack* associatedTrack = (SPxRadarTrack*)uniTrackDb->GetTrack(uniqueID);
		if (associatedTrack)
		{
			int idx = associatedTrack->GetSourceIndex();
			if (sourceIndex == idx)
			{
				return false;
			}
		}
	}

	return true;
}

static bool IsFusedCandidateValid(
	SPxUniTrackDatabase* uniTrackDb,
	SPxRadarTrack* radarTrack,
	SPxRadarTrack* fusedTrack,
	double disMetresRtn)
{
	if (!fusedTrack)
	{
		return false;
	}

	FusedTrackUserData* fud = (FusedTrackUserData*)fusedTrack->GetUserData();
	if (!fud)
	{
		return false;
	}

	if (!IsTrackFromDifferentSensor(uniTrackDb, radarTrack, fud))
	{
		return false;
	}

	return disMetresRtn < OWL_ASSOCIATION_DISTANCE_METRES + EPS;
}

class IDPool {
private:
	std::queue<int> availableIDs;
	int currentID;

public:
	IDPool(int maxIDs) : currentID(1) {
		for (int i = 1; i <= maxIDs; ++i) {
			availableIDs.push(i);
		}
	}

	int getID() {
		if (availableIDs.empty()) {
			return -1; // 或者抛出异常表示没有可用的ID
		}
		else {
			int id = availableIDs.front();
			availableIDs.pop();
			return id;
		}
	}

	void releaseID(int id) {
		availableIDs.push(id);
	}
};



IDPool m_idPool(3000);



OWlRadarTracksFusion::OWlRadarTracksFusion(SPxUniTrackDatabase* uniTrackDb)
{
	m_uniTrackDb = uniTrackDb;

	SetClassName("OWlRadarTracksFusion");
	if (!IsNameSet())
	{
		SetName("OWlRadarTracksFusion");
	}
	SetParameterLists(g_owlRadarTracksFusionReadParams, g_owlRadarTracksFusionWriteParams);

	//初始化
	m_lock.Initialise();
	m_tracksRtn = new SPxUniTrackDatabase::TrackDist_t*[RADAR_SENSORS_MAX_NUM];
	fusedTrackDB = m_uniTrackDb->GetFusedTrackSource(0);

	fusedTrackDB->GetSensorLatLong(&m_sensorFusedRadarLat,&m_sensorFusedRadarLong);

	m_trackFusedSenderLocal = new SPxPacketSender();
	m_trackFusedSenderLocal->SetAddress("127.0.0.1", 7770);

	m_trackFusedSender = new SPxPacketSender(); 
	m_trackFusedSender->SetName("UnifiedTrackSender");
	//m_trackFusedSender->SetAddress("239.192.88.99", 8899);
	m_trackFusedSender->SetStateFromConfig();

	m_fuseParam.radarAziObserveDifferenceVal = radarAziObserveDiffDegs;
	m_fuseParam.radarDisObserveDifferenceVal = radarDisObserveDiffMetres;

	m_uniTrackDb->AddEventCallback(UniTrackDBEventFunc, this);
}

OWlRadarTracksFusion::~OWlRadarTracksFusion()
{
	m_uniTrackDb->RemoveEventCallback(UniTrackDBEventFunc, this);

	delete[] m_tracksRtn;
}

int OWlRadarTracksFusion::UniTrackDBEventFunc(
	void *invokingObject, 
	void *userObject,
	void *arg)
{
	OWlRadarTracksFusion* fusion = (OWlRadarTracksFusion*)userObject;

	SPxUniTrackDatabase::Event_t* ev = (SPxUniTrackDatabase::Event_t*)arg;
	if (ev->type == SPxUniTrackDatabase::EVENT_TRACK_CREATE)
	{
		SPxUniTrack* track = ev->info.trackEvt.track;
		if (track->GetTrackType() == SPxUniTrack::TRACK_TYPE_RADAR)
		{
			//fusion->onRadarTrackCreate((SPxRadarTrack*)track);
		}
		if (track->GetTrackType() == SPxUniTrack::TRACK_TYPE_FUSED)
		{
			//fusion->onFusedTrackCreate((SPxRadarTrack*)track);
		}
	}

	if (ev->type == SPxUniTrackDatabase::EVENT_TRACK_UPDATE)
	{
		SPxUniTrack* track = ev->info.trackEvt.track;

		if (track->GetTrackType() == SPxUniTrack::TRACK_TYPE_RADAR)
		{
			if (((SPxRadarTrack*)track)->GetMinRpt()->status == SPX_PACKET_TRACK_STATUS_DELETED)
			{
				int zp = 1;
			}

			fusion->onRadarTrackUpdate((SPxRadarTrack*)track);
		}
		if (track->GetTrackType() == SPxUniTrack::TRACK_TYPE_FUSED)
		{
		
			if (((SPxRadarTrack*)track)->GetExtRpt()->age > 1)
			{
				int zp = 1;
			}
			fusion->onFusedTrackUpdate((SPxRadarTrack*)track);
		}
	}

	if (ev->type == SPxUniTrackDatabase::EVENT_TRACK_DELETE)
	{
		SPxUniTrack* track = ev->info.trackEvt.track;
		if (track->GetTrackType() == SPxUniTrack::TRACK_TYPE_RADAR)
		{
			fusion->onRadarTrackDelete((SPxRadarTrack*)track);
		}
		if (track->GetTrackType() == SPxUniTrack::TRACK_TYPE_FUSED)
		{
			fusion->onFusedTrackDelete((SPxRadarTrack*)track);
		}
	}

	if (ev->type == SPxUniTrackDatabase::EVENT_TRACK_SELECT)
	{
		/*SPxRadarTrack* track = (SPxRadarTrack*)ev->info.trackEvt.track;
		if (track->GetIsSelected())
		{
			char buf[1024] = { 0 };
			sprintf_s(buf, "fusedTrack assoaiate=%d,%d"
				, track->GetExtRpt()->fusion.trackID[0]
				, track->GetExtRpt()->fusion.trackID[1]);
			OWlError(OWL_ERR_INFO, buf);
		}*/

	}
	
	return 0;
}

void  OWlRadarTracksFusion::onRadarTrackCreate(SPxRadarTrack* track)
{
	
}

void OWlRadarTracksFusion::onFusedTrackUpdate(SPxRadarTrack* track)
{
	//check userData
	FusedTrackUserData* fud = (FusedTrackUserData*)track->GetUserData();
	if (!fud)
	{
		fud = new FusedTrackUserData();
		fud->trackVec.clear();
		/*memset(fud, 0, sizeof(FusedTrackUserData));
		fud->trackVec.clear();*/

		const SPxPacketTrackExtended* extRpt = track->GetExtRpt();
		fud->trackVec.push_back(extRpt->fusion.trackID[0]);
		track->SetUserData(fud);
	}

	
}

void OWlRadarTracksFusion::onRadarTrackDelete(SPxRadarTrack* track)
{
	RadarTrackUserData* rud = (RadarTrackUserData*)track->GetUserData();
	if (rud)
	{
		SPxRadarTrack* fusedTrack = (SPxRadarTrack*)fusedTrackDB->GetTrack(rud->fusedID);
		if (fusedTrack)
		{
			//如果融合航迹仅有该航迹单个贡献，则直接删除融合航迹

			SPxPacketTrackExtended* extRpt = (SPxPacketTrackExtended*)fusedTrack->GetExtRpt();

		
			
			FusedTrackUserData* fud = (FusedTrackUserData*)fusedTrack->GetUserData();
			if (fud)
			{
				vector<UINT32> vec = fud->trackVec;
				RemoveUnifiedTrackFromVec(vec, track->GetUnifiedID());
				fud->trackVec = vec;

				int numSensors = fud->trackVec.size();
				if (numSensors == 0)
				{
					SPxPacketTrackExtended ext = { 0 };
					ext.norm.min.id = fusedTrack->GetID();
					ext.norm.min.status = SPX_PACKET_TRACK_STATUS_DELETED;

					//注意：通过网络发送的形式，进行航迹的创建和删除，Update进行更新
					const int bufLen = sizeof(SPxPacketTrackExtended);
					unsigned char buf[bufLen] = { 0 };
					SPxRemoteServer::TrackPackNet(&(ext.norm.min), &(ext.norm), &ext, buf, bufLen);
					SPxTime_t now;
					SPxTimeGetEpoch(&now);
					m_trackFusedSenderLocal->SendPacketB(SPX_PACKET_TYPEB_TRACK_EXT, now, &buf, bufLen);
					m_trackFusedSender->SendPacketB(SPX_PACKET_TYPEB_TRACK_EXT, now, &buf, bufLen);
				}
				else
				{
					SPxTime_t now;
					SPxTimeGetEpoch(&now);
					fusedTrack->Update(&now, &(extRpt->norm.min), &(extRpt->norm), extRpt);

					//注意：需将距离方位转换为相对于融合雷达站,以便与AIS融合
					double azi = 0;
					double range = 0;
					CoordinateTransformation ct;
					ct.PolarCoordinates_Calculate(m_sensorFusedRadarLong, m_sensorFusedRadarLat
						, extRpt->longDegs, extRpt->latDegs
						, azi, range);
					extRpt->norm.min.azimuthDegrees = azi;
					extRpt->norm.min.rangeMetres = range;

					const SPxPacketTrackExtended* ext = fusedTrack->GetExtRpt();
					const int bufLen = sizeof(SPxPacketTrackExtended);
					unsigned char buf[bufLen] = { 0 };
					SPxRemoteServer::TrackPackNet(&(ext->norm.min), &(ext->norm), ext, buf, bufLen);
					SPxTimeGetEpoch(&now);
					m_trackFusedSender->SendPacketB(SPX_PACKET_TYPEB_TRACK_EXT, now, &buf, bufLen);
				}
			}



		/*	SPxTime_t now;
			SPxTimeGetEpoch(&now);
			fusedTrack->Update(&now, &(extRpt->norm.min), &(extRpt->norm), extRpt);*/
		}
		else
		{
			/* do nothing */
			int zp = 10;
		}

		delete rud;
	}
}

void OWlRadarTracksFusion::onFusedTrackDelete(SPxRadarTrack* track)
{
	const SPxPacketTrackExtended* extRpt = track->GetExtRpt();
	
	FusedTrackUserData* fud = (FusedTrackUserData*)track->GetUserData();
	if (fud)
	{
		for (int i = 0; i < fud->trackVec.size(); i++)
		{
			UINT32 uniqueID = (fud->trackVec[i]);
			SPxRadarTrack* track = (SPxRadarTrack*)m_uniTrackDb->GetTrack(uniqueID);
			if (track)
			{
				RadarTrackUserData* rud = (RadarTrackUserData*)track->GetUserData();
				if (rud)
				{
					if (rud->fusedID != 0)
					{
						rud->fusedID = 0;
					}
				}
			}
		}

		delete fud;
	}
	
	

	UINT32 id = track->GetID();
	m_idPool.releaseID(id);
}




void OWlRadarTracksFusion::onRadarTrackUpdate(SPxRadarTrack* track)
{
	SPxAutoLock lock(m_lock);

	//check userData
	RadarTrackUserData* rud = (RadarTrackUserData*)track->GetUserData();
	if (!rud)
	{
		rud = new RadarTrackUserData();
		memset(rud, 0, sizeof(RadarTrackUserData));
		track->SetUserData(rud);
	}


	rud->count++;
	if (rud->count < 2)//延迟一圈，否则来不及存入内存数据库
	{
		return;
	}


	/**********************************************************************/
	if (track->GetIsSelected())
	{
		char buf[1024] = { 0 };
		sprintf_s(buf, "track fusedId=%d  perfusedId=%d   hitCount=%d", rud->fusedID, rud->preFusedID, rud->hitCount);
		OWlError(OWL_ERR_INFO, buf);
	}

	const SPxPacketTrackExtended* extRpt = track->GetExtRpt();
	if (rud->fusedID == 0)//该目标还未与融合航迹关联
	{
		handleUnAssiocatedRadarTrack(track);
	}
	else//该航迹已和融合航迹关联
	{
		handleAassiocatedRadarTrack(track);
	}
}


bool OWlRadarTracksFusion::fuseEngine(SPxRadarTrack* track, SPxRadarTrack* fusedTrack, UINT8 threshMulti)
{
	bool ret = false;


	/* 获取内部航迹，理论上只有一个 */

	SPxRadarTrack* interTrack = NULL;
	
	//20240222 注意：融合航迹本身就是一个完整意义上的航迹,直接用融合航迹本身进行融合判断

	interTrack = fusedTrack;
	
	if (interTrack)
	{
		//雷达观测误差（由外部配置）
		const double radarAziObserveDiffDegsLocal =
			(this->radarAziObserveDiffDegs > EPS) ? this->radarAziObserveDiffDegs : EPS;
		const double radarDisObserveDiffMetresLocal =
			(this->radarDisObserveDiffMetres > EPS) ? this->radarDisObserveDiffMetres : EPS;
		double radarAziObserveDiffRad = radarAziObserveDiffDegsLocal / 180.0 * PI;

		const SPxPacketTrackExtended* interTrackExtRpt = interTrack->GetExtRpt();
		double interTrackRangeMetres = interTrackExtRpt->norm.min.rangeMetres;
		double interTrackAziRad = interTrackExtRpt->norm.min.azimuthDegrees / 180.0 * PI;
		double interTrackLatDegs = interTrackExtRpt->latDegs;
		double interTrackLonDegs = interTrackExtRpt->longDegs;

		const SPxPacketTrackExtended* trackExtRpt = track->GetExtRpt();
		double trackRangeMetres = trackExtRpt->norm.min.rangeMetres;
		double trackAziRad = trackExtRpt->norm.min.azimuthDegrees / 180.0 * PI;
		double trackLatDegs = trackExtRpt->latDegs;
		double trackLonDegs = trackExtRpt->longDegs;

		/* 转换到直角坐标 */
		double interTrackMeasErrXMetres = 
			radarDisObserveDiffMetresLocal * sin(interTrackAziRad)
			+ interTrackRangeMetres * radarAziObserveDiffRad * cos(interTrackAziRad);
		double interTrackMeasErrYMetres = radarDisObserveDiffMetresLocal * cos(interTrackAziRad)
			+ interTrackRangeMetres * radarAziObserveDiffRad * sin(interTrackAziRad);

		double trackMeasErrXMetres =
			radarDisObserveDiffMetresLocal * sin(trackAziRad)
			+ trackRangeMetres * radarAziObserveDiffRad * cos(trackAziRad);
		double trackMeasErrYMetres = radarDisObserveDiffMetresLocal * cos(trackAziRad)
			+ trackRangeMetres * radarAziObserveDiffRad * sin(trackAziRad);

		double threshXMetres = threshMulti * sqrt((interTrackMeasErrXMetres*interTrackMeasErrXMetres + trackMeasErrXMetres*trackMeasErrXMetres));
		double dxMetres = 0;
		double dyMetres = 0;
		SPxLatLongDiffMetres(interTrackLatDegs, interTrackLonDegs,
			trackLatDegs, trackLonDegs, &dxMetres, &dyMetres);
		double diffXMetres = fabs(dxMetres);
		bool isXHit = diffXMetres < threshXMetres + EPS;

		double threshYMetres = threshMulti * sqrt((interTrackMeasErrYMetres*interTrackMeasErrYMetres + trackMeasErrYMetres*trackMeasErrYMetres));
		double diffYMetres = fabs(dyMetres);
		bool isYHit = diffYMetres < threshYMetres;
			

		if (isXHit && isYHit)
		{
			ret = true;
		}

		if (track->GetIsSelected())
		{
			char buf[1024] = { 0 };
			sprintf_s(buf, "track=%d  iner=%d, diffXMetres=%0.2f threshXMetres=%0.2f diffYMetres=%0.2f threshYMetres=%0.2f"
				, track->GetID(),interTrack->GetID()
				, diffXMetres, threshXMetres, diffYMetres, threshYMetres);
			OWlError(OWL_ERR_INFO, buf);
		}
	}

	return ret;
}

void OWlRadarTracksFusion::preFuse(SPxRadarTrack* track)
{
	RadarTrackUserData* rud = (RadarTrackUserData*)track->GetUserData();
	if (!rud)
	{
		return;
	}

	const SPxPacketTrackExtended* extRpt = track->GetExtRpt();
	/*SPxRadarTrack* fusedTrack = fusedTrackDB->GetNearestTrackLatLong(
		extRpt->latDegs, extRpt->longDegs);*/
	SPxRadarTrack* fusedTrack = fusedTrackDB->GetTrack(rud->preFusedID);
	if (fusedTrack)
	{
		bool isFused = fuseEngine(track, fusedTrack);
		if (isFused)
		{
			rud->hitCount++;
		}
		else
		{
			/* clear perpare state */
			rud->preFusedID = 0;
			rud->hitCount = 0;
		}
	}
	else
	{
		/* clear perpare state */
		rud->preFusedID = 0;
		rud->hitCount = 0;
	}

	/* 正式与融合航迹关联 */
	if (rud->hitCount >= FUSION_HIT_COUNT_THRESH)
	{
		if (rud->fusedID != 0)
		{
			char buf[1024] = { 0 };
			sprintf_s(buf, "radar track=%d  change fused track from %d to %d"
				, track->GetID(), rud->fusedID, fusedTrack->GetID());
			OWlError(OWL_ERR_WARNING, buf);

			//解除融合关系
			SPxRadarTrack* oldFusedTrack = fusedTrackDB->GetTrack(rud->fusedID);
			if (oldFusedTrack)
			{

				FusedTrackUserData* fud = (FusedTrackUserData*)oldFusedTrack->GetUserData();
				if (fud)
				{
					vector<UINT32> vec = fud->trackVec;
					RemoveUnifiedTrackFromVec(vec, track->GetUnifiedID());
					fud->trackVec = vec;


					int numSensors = fud->trackVec.size();
					if (numSensors == 0)
					{
						SPxPacketTrackExtended ext = { 0 };
						ext.norm.min.id = oldFusedTrack->GetID();
						ext.norm.min.status = SPX_PACKET_TRACK_STATUS_DELETED;

						//注意：通过网络发送的形式，进行航迹的创建和删除，Update进行更新
						const int bufLen = sizeof(SPxPacketTrackExtended);
						unsigned char buf[bufLen] = { 0 };
						SPxRemoteServer::TrackPackNet(&(ext.norm.min), &(ext.norm), &ext, buf, bufLen);
						SPxTime_t now;
						SPxTimeGetEpoch(&now);
						m_trackFusedSenderLocal->SendPacketB(SPX_PACKET_TYPEB_TRACK_EXT, now, &buf, bufLen);
					m_trackFusedSender->SendPacketB(SPX_PACKET_TYPEB_TRACK_EXT, now, &buf, bufLen);
					}
					//else
					//{
					//	SPxTime_t now;
					//	SPxTimeGetEpoch(&now);
					//	fusedTrack->Update(&now, &(extRpt->norm.min), &(extRpt->norm), extRpt);

					//	//注意：需将距离方位转换为相对于融合雷达站,以便与AIS融合
					//	double azi = 0;
					//	double range = 0;
					//	CoordinateTransformation ct;
					//	ct.PolarCoordinates_Calculate(m_sensorFusedRadarLong, m_sensorFusedRadarLat
					//		, extRpt->longDegs, extRpt->latDegs
					//		, azi, range);
					//	extRpt->norm.min.azimuthDegrees = azi;
					//	extRpt->norm.min.rangeMetres = range;

					//	const SPxPacketTrackExtended* ext = fusedTrack->GetExtRpt();
					//	const int bufLen = sizeof(SPxPacketTrackExtended);
					//	unsigned char buf[bufLen] = { 0 };
					//	SPxRemoteServer::TrackPackNet(&(ext->norm.min), &(ext->norm), ext, buf, bufLen);
					//	SPxTimeGetEpoch(&now);
					//	m_trackFusedSender->SendPacketB(SPX_PACKET_TYPEB_TRACK_EXT, now, &buf, bufLen);
					//}

				}
			}
			
		}

		rud->fusedID = fusedTrack->GetID();
		rud->preFusedID = 0;
		rud->failCount = 0;
		rud->hitCount = 0;

		SPxPacketTrackExtended* fusedTrackExtRpt = (SPxPacketTrackExtended*)fusedTrack->GetExtRpt();
		
	
		FusedTrackUserData* fud = (FusedTrackUserData*)fusedTrack->GetUserData();
		if (fud)
		{
			fud->trackVec.push_back(track->GetUnifiedID());
		}


		//更新经纬度位置、航向航速
		SPxLatLong_t pos = { 0 };
		double speed = 0;
		double course = 0;
		computeFusedTrackPosSpeedCourse(fusedTrack, &pos, &speed, &course);

		fusedTrackExtRpt->latDegs = pos.latDegs;
		fusedTrackExtRpt->longDegs = pos.longDegs;
		fusedTrackExtRpt->norm.min.speedMps = speed;
		fusedTrackExtRpt->norm.min.courseDegrees = course;
		fusedTrackExtRpt->fusion.sensors = SENSOR_0_1_FLAG;
		fusedTrackExtRpt->age++;

		SPxTime_t now;
		SPxTimeGetEpoch(&now);
		fusedTrack->Update(&now, &(fusedTrackExtRpt->norm.min), &(fusedTrackExtRpt->norm), fusedTrackExtRpt);


		//注意：需将距离方位转换为相对于融合雷达站,以便与AIS融合
		double azi = 0;
		double range = 0;
		CoordinateTransformation ct;
		ct.PolarCoordinates_Calculate(m_sensorFusedRadarLong, m_sensorFusedRadarLat
			, fusedTrackExtRpt->longDegs, fusedTrackExtRpt->latDegs
			, azi, range);
		fusedTrackExtRpt->norm.min.azimuthDegrees = azi;
		fusedTrackExtRpt->norm.min.rangeMetres = range;
		//注意：需将直角坐标转换为相对于融合雷达站,以便光电跟踪
		double xM, yM = 0;
		SPxLatLongDiffMetres(m_sensorFusedRadarLat, m_sensorFusedRadarLong
			, fusedTrackExtRpt->latDegs, fusedTrackExtRpt->longDegs
			, &xM, &yM);
		fusedTrackExtRpt->norm.xMetres = xM;
		fusedTrackExtRpt->norm.yMetres = yM;



		const SPxPacketTrackExtended* ext = fusedTrack->GetExtRpt();
		const int bufLen = sizeof(SPxPacketTrackExtended);
		unsigned char buf[bufLen] = { 0 };
		SPxRemoteServer::TrackPackNet(&(fusedTrackExtRpt->norm.min), &(ext->norm), ext, buf, bufLen);
		SPxTimeGetEpoch(&now);
		m_trackFusedSender->SendPacketB(SPX_PACKET_TYPEB_TRACK_EXT, now, &buf, bufLen);
	}
}

void OWlRadarTracksFusion::computeFusedTrackPosSpeedCourse(SPxRadarTrack* fusedTrack
	, SPxLatLong_t* posRet, double* speedRet, double* courseRet)
{
	SPxLatLong_t fusedPos = { 0 };
	const SPxPacketTrackExtended* extRpt = fusedTrack->GetExtRpt();

	FusedTrackUserData* fud = (FusedTrackUserData*)fusedTrack->GetUserData();
	if (fud)
	{
		int numSensors = fud->trackVec.size();
		double all_rangeMeteres = 0;
		double m_radian = 0;
		//for (auto it = userData->trackVec.begin(); it != userData->trackVec.end(); ++it)
		for (int i = 0; i < fud->trackVec.size(); i++)
		{
			UINT32 uniqueID = (fud->trackVec[i]);
			//UINT32 uniqueID = *it;
			SPxRadarTrack* track = (SPxRadarTrack*)m_uniTrackDb->GetTrack(uniqueID);
			if (track)
			{
				double rangeMetres = track->GetRangeMetres();
				if (rangeMetres < EPS)
				{
					all_rangeMeteres += 1 / 100;
				}
				else
				{
					all_rangeMeteres += 1 / rangeMetres;
				}
			}
		}

		double x = 0, y = 0;
		double m_weight = 0;
		double trackx = 0, tracky = 0;
		for (int i = 0; i < fud->trackVec.size(); i++)
		{
			UINT32 uniqueID = (fud->trackVec[i]);
			//UINT32 uniqueID = *it;
			SPxRadarTrack* track = (SPxRadarTrack*)m_uniTrackDb->GetTrack(uniqueID);
			if (track)
			{
				double Lat = 0;
				double Lon = 0;
				track->GetLatLong(&Lat, &Lon);

				double s = 0;
				double c = 0;
				track->GetVelocity(&s,&c);

				m_radian = c * PI / 180;
				//double a = sin(m_radian);
				x = sin(m_radian)*s;
				y = cos(m_radian)*s;

				double rangeMetres = track->GetRangeMetres();
				if (rangeMetres == 0)
				{
					m_weight = (1 / 100) / all_rangeMeteres;
				}
				else
				{
					m_weight = (1 / rangeMetres) / all_rangeMeteres;
				}
				fusedPos.latDegs += (m_weight*Lat);
				fusedPos.longDegs += (m_weight*Lon);
				trackx += m_weight * x;
				tracky += m_weight * y;
			}
		}

		//把x,y坐标转换为航向航速
		double course = 0;
		double speed = 0;
		if (tracky == 0)
		{
			course = 0;
		}
		else
		{
			course = atan2(trackx, tracky) * 180 / PI;
			if (course < 0)
			{
				course += 360;
			}
		}

		double aa = pow(trackx, 2);
		double bb = pow(tracky, 2);
		speed = sqrt(aa + bb);

		posRet->latDegs = fusedPos.latDegs;
		posRet->longDegs = fusedPos.longDegs;

		*courseRet = course;
		*speedRet = speed;

	}


}

void OWlRadarTracksFusion::onFusedTrackCreate(SPxRadarTrack* track)
{
	const SPxPacketTrackExtended* extRpt = track->GetExtRpt();
	int trackUnifiedID = extRpt->fusion.trackID[0];
	SPxRadarTrack* radarTrack = (SPxRadarTrack*)m_uniTrackDb->GetTrack(trackUnifiedID);
	if (radarTrack)
	{
		RadarTrackUserData* rud = (RadarTrackUserData*)radarTrack->GetUserData();
		if (rud)
		{
			//确定融合航迹已在数据库可访问，然后再将融合ID赋值给userData->fusedID
			rud->fusedID = track->GetID();
		}
	}
	else
	{
		//不会出现
		int zp = 1;
	}
	
}



void OWlRadarTracksFusion::handleUnAssiocatedRadarTrack(SPxRadarTrack* track)
{
	//是否处于预融合状态
	RadarTrackUserData* rud = (RadarTrackUserData*)track->GetUserData();
	const SPxPacketTrackExtended* extRpt = track->GetExtRpt();
	if (rud->preFusedID != 0)
	{
		preFuse(track);
	}
	else
	{
		/**********************************************************************/
		double disMetresRtn = 0;
		// 初筛距离阈值在 IsFusedCandidateValid() 中统一处理
		SPxRadarTrack* fusedTrack = fusedTrackDB->GetNearestTrackLatLong(
			extRpt->latDegs, extRpt->longDegs, 1, &disMetresRtn);

		bool isNearestFusedTrackVaild =
			IsFusedCandidateValid(m_uniTrackDb, track, fusedTrack, disMetresRtn);

		bool isFused = false;
		if (isNearestFusedTrackVaild)
		{
			isFused = fuseEngine(track, fusedTrack);
			if (isFused)
			{
				//进入预融合状态
				rud->preFusedID = fusedTrack->GetID();
				rud->hitCount++;
				rud->failCount = 0;
			}
			else
			{
				rud->failCount++;
			}
		}


		//不符合关联,则直接新建单独融合航迹
		//if (!isNearestFusedTrackVaild || !isFused)
		if (!isNearestFusedTrackVaild || rud->failCount > FUSION_FIAL_COUNT_THRESH)
		{
			

			SPxPacketTrackExtended ext = { 0 };
			memcpy(&ext, extRpt, sizeof(SPxPacketTrackExtended));

			ext.norm.min.uniTrackType = SPxUniTrack::TRACK_TYPE_FUSED;
			ext.norm.min.senderID = OWL_FUSED_TRACK_SENDER_ID;
			ext.norm.min.id = m_idPool.getID();//分配ID
			ext.extMask = 0xFFFFFFFF;
			ext.fusion.sensors = SENSOR_0_FLAG;
			ext.fusion.trackID[0] = track->GetUnifiedID();
			ext.age = 1;
			ext.norm.min.sizeDegrees = track->GetMinRpt()->sizeDegrees;
			ext.norm.min.sizeMetres = track->GetMinRpt()->sizeMetres;

			//注意：需将距离方位转换为相对于融合雷达站,以便与AIS融合
			double azi = 0;
			double range = 0;
			CoordinateTransformation ct;
			ct.PolarCoordinates_Calculate(m_sensorFusedRadarLong, m_sensorFusedRadarLat
				, extRpt->longDegs, extRpt->latDegs
				, azi, range);
			ext.norm.min.azimuthDegrees = azi;
			ext.norm.min.rangeMetres = range;
			//注意：需将直角坐标转换为相对于融合雷达站,以便光电跟踪
			double xM, yM = 0;
			SPxLatLongDiffMetres(m_sensorFusedRadarLat, m_sensorFusedRadarLong
				, extRpt->latDegs, extRpt->longDegs
				, &xM, &yM);
			ext.norm.xMetres = xM;
			ext.norm.yMetres = yM;

			const int bufLen = sizeof(SPxPacketTrackExtended);
			unsigned char buf[bufLen] = { 0 };
			SPxRemoteServer::TrackPackNet(&(ext.norm.min), &(ext.norm), &ext, buf, bufLen);
			SPxTime_t now;
			SPxTimeGetEpoch(&now);
			SPxErrorCode errLocal = m_trackFusedSenderLocal->SendPacketB(SPX_PACKET_TYPEB_TRACK_EXT, now, &buf, bufLen);
			SPxErrorCode errExt = m_trackFusedSender->SendPacketB(SPX_PACKET_TYPEB_TRACK_EXT, now, &buf, bufLen);
			if (errLocal != SPX_NO_ERROR || errExt != SPX_NO_ERROR)
			{
				int zp = 1;
			}

			//单个航迹正式融合，后续等待其他航迹发起融合
			rud->fusedID = ext.norm.min.id;

			rud->failCount = 0;
			rud->hitCount = 0;
			rud->preFusedID = 0;

		}
	}
}

void OWlRadarTracksFusion::handleAassiocatedRadarTrack(SPxRadarTrack* track)
{
	//更新与之关联的融合目标
	RadarTrackUserData* rud = (RadarTrackUserData*)track->GetUserData();
	SPxRadarTrack* fusedTrack = fusedTrackDB->GetTrack(rud->fusedID);

	if (fusedTrack)
	{
		SPxPacketTrackExtended* fusedTrackExtRpt = (SPxPacketTrackExtended*)fusedTrack->GetExtRpt();

		/* 更新航向、航速、经纬度位置、声明周期 */
		SPxLatLong_t pos = { 0 };
		double speed = 0;
		double course = 0;

		FusedTrackUserData* fud = (FusedTrackUserData*)fusedTrack->GetUserData();
		if (fud)
		{
			int numSensors = fud->trackVec.size();
			if (numSensors == 1)
			{
				handleAassiocatedRadarTrackFusedSingle(track, fusedTrack);
			}
			if (numSensors > 1)
			{
				//继续进行融合判断
				if (!fuseEngine(track, fusedTrack,4))
				{
					rud->failCount++;

					char buf[1024] = { 0 };
					sprintf_s(buf, "fusedTrack=%d  radar track=%d  fuse fail,failCount=%d", fusedTrack->GetID(), track->GetID(), rud->failCount);
					OWlError(OWL_ERR_INFO, buf);

				}
				else
				{
					rud->failCount = 0;

					/*char buf[1024] = { 0 };
					sprintf_s(buf, "fusedTrack=%d  radar track=%d  fuse hit", fusedTrack->GetID(), track->GetID());
					OWlError(OWL_ERR_INFO, buf);*/
				}
				if (rud->failCount > FUSION_FIAL_COUNT_THRESH)//从融合航迹中剔除
				{
					char buf[1024] = { 0 };
					sprintf_s(buf, "fusedTrack=%d remove radar track=%d", fusedTrack->GetID(), track->GetID());
					OWlError(OWL_ERR_INFO, buf);

					resetRadarTrackUserData(rud);

					//从融合航迹中剔除
					vector<UINT32> vec = fud->trackVec;
					RemoveUnifiedTrackFromVec(vec, track->GetUnifiedID());
					fud->trackVec = vec;
				}
			}
		}



		if (fud)
		{
			int numSensors = fud->trackVec.size();
			if (numSensors == 1)
			{
				track->GetLatLong(&pos.latDegs, &pos.longDegs);
				track->GetVelocity(&speed, &course);
			}
			if (numSensors > 1)
			{
				computeFusedTrackPosSpeedCourse(fusedTrack, &pos, &speed, &course);
			}
		}

		fusedTrackExtRpt->latDegs = pos.latDegs;
		fusedTrackExtRpt->longDegs = pos.longDegs;
		fusedTrackExtRpt->norm.min.speedMps = speed;
		fusedTrackExtRpt->norm.min.courseDegrees = course;
		fusedTrackExtRpt->age++;
		fusedTrackExtRpt->norm.min.sizeDegrees = track->GetMinRpt()->sizeDegrees;
		fusedTrackExtRpt->norm.min.sizeMetres = track->GetMinRpt()->sizeMetres;

		//通过update更新，而不是发送网络包
		SPxTime_t now;
		SPxTimeGetEpoch(&now);
		fusedTrack->Update(&now, &(fusedTrackExtRpt->norm.min), &(fusedTrackExtRpt->norm), fusedTrackExtRpt);
		/*
		注意!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
		Update不会触发回调函数，仅仅是更新了内存中航迹信息
		*/
		//注意：需将距离方位转换为相对于融合雷达站,以便与AIS融合
		double azi = 0;
		double range = 0;
		CoordinateTransformation ct;
		ct.PolarCoordinates_Calculate(m_sensorFusedRadarLong, m_sensorFusedRadarLat
			, fusedTrackExtRpt->longDegs, fusedTrackExtRpt->latDegs
			, azi, range);
		fusedTrackExtRpt->norm.min.azimuthDegrees = azi;
		fusedTrackExtRpt->norm.min.rangeMetres = range;
		//注意：需将直角坐标转换为相对于融合雷达站,以便光电跟踪
		double xM, yM = 0;
		SPxLatLongDiffMetres(m_sensorFusedRadarLat, m_sensorFusedRadarLong
			, fusedTrackExtRpt->latDegs, fusedTrackExtRpt->longDegs
			, &xM, &yM);
		fusedTrackExtRpt->norm.xMetres = xM;
		fusedTrackExtRpt->norm.yMetres = yM;

		const SPxPacketTrackExtended* ext = fusedTrack->GetExtRpt();
		const int bufLen = sizeof(SPxPacketTrackExtended);
		unsigned char buf[bufLen] = { 0 };
		SPxRemoteServer::TrackPackNet(&(ext->norm.min), &(ext->norm), ext, buf, bufLen);
		SPxTimeGetEpoch(&now);
		m_trackFusedSender->SendPacketB(SPX_PACKET_TYPEB_TRACK_EXT, now, &buf, bufLen);
	}
	else
	{
		char buf[1024] = { 0 };
		sprintf_s(buf, "fusedTrack=%d no exist", rud->fusedID);
		OWlError(OWL_ERR_WARNING, buf);

		rud->fusedID = 0;
	}
}

void OWlRadarTracksFusion::handleAassiocatedRadarTrackFusedSingle(SPxRadarTrack* track, SPxRadarTrack* fusedTrack)
{
	//是否处于预融合状态
	RadarTrackUserData* rud = (RadarTrackUserData*)track->GetUserData();
	const SPxPacketTrackExtended* extRpt = track->GetExtRpt();
	if (rud->preFusedID != 0)
	{
		preFuse(track);
	}
	else
	{
		/**********************************************************************/
		double disMetresRtn = 0;
		// 初筛距离阈值在 IsFusedCandidateValid() 中统一处理
		SPxRadarTrack* fusedTrackTemp = fusedTrackDB->GetNearestTrackLatLong(
			extRpt->latDegs, extRpt->longDegs, 1, &disMetresRtn);
		if (!fusedTrackTemp)
		{
			return;
		}

		if (fusedTrack->GetID() == fusedTrackTemp->GetID())//距离优先，不能比现有融合航迹近
		{
			return;
		}


		bool isNearestFusedTrackVaild =
			IsFusedCandidateValid(m_uniTrackDb, track, fusedTrackTemp, disMetresRtn);

		bool isFused = false;
		if (isNearestFusedTrackVaild)
		{
			isFused = fuseEngine(track, fusedTrackTemp);
			if (isFused)
			{
				//进入预融合状态
				rud->preFusedID = fusedTrackTemp->GetID();
				rud->hitCount++;
				rud->failCount = 0;
			}
			else
			{
				rud->failCount++;
			}
		}


		//不符合关联,则恢复
		//if (!isNearestFusedTrackVaild || !isFused)
		if (!isNearestFusedTrackVaild || rud->failCount > FUSION_FIAL_COUNT_THRESH)
		{
			rud->preFusedID = 0;
			rud->hitCount = 0;
			rud->failCount = 0;
		}
	}
}

void OWlRadarTracksFusion::resetRadarTrackUserData(RadarTrackUserData* rud)
{
	rud->failCount = 0;
	rud->hitCount = 0;
	rud->preFusedID = 0;
	rud->fusedID = 0;
	//rud->count = 0;
}


int OWlRadarTracksFusion::SetParameter(char* parameterName, char* parameterValue)
{
	if (!parameterName || !parameterValue)
	{
		return SPX_ERR_BAD_PARAMETER_FOR_COMMAND;
	}

	if (strcmp(parameterName, "radarAziObsDiff") == 0)
	{
		radarAziObserveDiffDegs = atof(parameterValue);
		m_fuseParam.radarAziObserveDifferenceVal = radarAziObserveDiffDegs;
		return SPX_NO_ERROR;
	}

	if (strcmp(parameterName, "radarDisObsDiff") == 0)
	{
		radarDisObserveDiffMetres = atof(parameterValue);
		m_fuseParam.radarDisObserveDifferenceVal = radarDisObserveDiffMetres;
		return SPX_NO_ERROR;
	}

	return SPX_ERR_BAD_PARAMETER_FOR_COMMAND;
}

int OWlRadarTracksFusion::GetParameter(char* parameterName, char* valueBuf, int bufLen)
{
	if (!parameterName || !valueBuf || bufLen <= 0)
	{
		return SPX_ERR_BAD_PARAMETER_FOR_COMMAND;
	}

	if (strcmp(parameterName, "radarAziObsDiff") == 0)
	{
		_snprintf_s(valueBuf, bufLen, _TRUNCATE, "%.3f", radarAziObserveDiffDegs);
		return SPX_NO_ERROR;
	}

	if (strcmp(parameterName, "radarDisObsDiff") == 0)
	{
		_snprintf_s(valueBuf, bufLen, _TRUNCATE, "%.3f", radarDisObserveDiffMetres);
		return SPX_NO_ERROR;
	}

	return SPX_ERR_BAD_PARAMETER_FOR_COMMAND;
}





//#include "stdafx.h"
//#include "OWlRadarTracksFusion.h"
//
//#include <queue>
//#include <cmath>
//#include <algorithm>
//
//#ifndef EPS
//#define EPS 1e-6
//#endif
//
//// 支持的最大雷达源数（用于 m_tracksRtn 分配）
//#define RADAR_SENSORS_MAX_NUM         10
//
//// 预融合阶段：连续命中次数达到该值，才确认真正融合
//#define FUSION_HIT_COUNT_THRESH       3
//
//// 预融合 / 维护阶段：连续失败次数超过该值，认为当前融合关系不可靠
//#define FUSION_FIAL_COUNT_THRESH      5
//
//// 搜索最近融合航迹时的粗过滤距离（单位：米）
//#define OWL_FUSED_DISTANCE_THRESH_METRES   1000.0
//
//// 本模块创建的融合航迹 senderID（可按项目需要调整）
//#define OWL_FUSED_SENDER_ID           99
//
////=======================
//// 简单 ID 池：为融合航迹分配 ID
////=======================
//class IDPool
//{
//public:
//    explicit IDPool(int maxIDs)
//    {
//        for (int i = 1; i <= maxIDs; ++i)
//        {
//            availableIDs.push(i);
//        }
//    }
//
//    int getID()
//    {
//        if (availableIDs.empty())
//        {
//            return -1;
//        }
//        int id = availableIDs.front();
//        availableIDs.pop();
//        return id;
//    }
//
//    void releaseID(int id)
//    {
//        if (id > 0)
//        {
//            availableIDs.push(id);
//        }
//    }
//
//private:
//    std::queue<int> availableIDs;
//};
//
//// 全局 ID 池（当前进程只用一个融合对象时是安全的）
//static IDPool g_fusedIdPool(3000);
//
////============================================================
//// 构造 / 析构
////============================================================
//OWlRadarTracksFusion::OWlRadarTracksFusion(SPxUniTrackDatabase* uniTrackDb)
//    : m_uniTrackDb(uniTrackDb)
//    , m_tracksRtn(nullptr)
//    , fusedTrackDB(nullptr)
//    , m_trackFusedSenderLocal(nullptr)
//    , m_trackFusedSender(nullptr)
//    , m_sensorFusedRadarLat(0.0)
//    , m_sensorFusedRadarLong(0.0)
//    , radarAziObserveDiffDegs(0.2)
//    , radarDisObserveDiffMetres(5.0)
//{
//    // 初始化锁
//    m_lock.Initialise();
//
//    if (!m_uniTrackDb)
//    {
//        OWlError(OWL_ERR_ERROR, "OWlRadarTracksFusion: uniTrackDb is null, fusion disabled.");
//        return;
//    }
//
//    // 获取融合航迹数据库（index=0 为融合源）
//    fusedTrackDB = m_uniTrackDb->GetFusedTrackSource(0);
//    if (!fusedTrackDB)
//    {
//        OWlError(OWL_ERR_ERROR, "OWlRadarTracksFusion: GetFusedTrackSource(0) failed, fusion disabled.");
//        return;
//    }
//
//    // 获取“融合雷达”的经纬度，用作极坐标 / 笛卡尔坐标转换基准
//    fusedTrackDB->GetSensorLatLong(&m_sensorFusedRadarLat, &m_sensorFusedRadarLong);
//
//    // 为统一航迹库的距离查询接口准备返回数组（当前未直接使用，保留兼容）
//    m_tracksRtn = new SPxUniTrackDatabase::TrackDist_t * [RADAR_SENSORS_MAX_NUM];
//    memset(m_tracksRtn, 0, sizeof(SPxUniTrackDatabase::TrackDist_t*) * RADAR_SENSORS_MAX_NUM);
//
//    // 本地 loopback 发送器（127.0.0.1）：把新建/删除 fused 航迹回灌到 SPx
//    m_trackFusedSenderLocal = new SPxPacketSender();
//    m_trackFusedSenderLocal->SetAddress("127.0.0.1", 7770);
//
//    // 对外组网发送 fused 航迹
//    m_trackFusedSender = new SPxPacketSender();
//    m_trackFusedSender->SetName("UnifiedTrackSender");
//    m_trackFusedSender->SetStateFromConfig();
//
//    // 利用 m_fuseParam 初始化默认观测误差（可由外部参数调整）
//    radarAziObserveDiffDegs = m_fuseParam.radarAziObserveDifferenceVal;
//    radarDisObserveDiffMetres = m_fuseParam.radarDisObserveDifferenceVal;
//
//    // 注册统一航迹库事件回调
//    m_uniTrackDb->AddEventCallback(UniTrackDBEventFunc, this);
//
//    char buf[256] = { 0 };
//    sprintf_s(buf,
//        "OWlRadarTracksFusion: init ok (aziDiff=%.3f deg, disDiff=%.1f m, sensorLat=%.5f, sensorLon=%.5f)",
//        radarAziObserveDiffDegs,
//        radarDisObserveDiffMetres,
//        m_sensorFusedRadarLat,
//        m_sensorFusedRadarLong);
//    OWlError(OWL_ERR_INFO, buf);
//}
//
//OWlRadarTracksFusion::~OWlRadarTracksFusion()
//{
//    if (m_uniTrackDb)
//    {
//        m_uniTrackDb->RemoveEventCallback(UniTrackDBEventFunc, this);
//    }
//
//    if (m_tracksRtn)
//    {
//        delete[] m_tracksRtn;
//        m_tracksRtn = nullptr;
//    }
//
//    // 是否在此处 delete 发送器，取决于外部是否还在持有指针。
//    // 如果确认外部不再使用，可取消注释。
//    /*
//    delete m_trackFusedSenderLocal;
//    m_trackFusedSenderLocal = nullptr;
//
//    delete m_trackFusedSender;
//    m_trackFusedSender = nullptr;
//    */
//}
//
////============================================================
//// 统一航迹库事件回调（静态）
////============================================================
//int OWlRadarTracksFusion::UniTrackDBEventFunc(
//    void* /*invokingObject*/,
//    void* userObject,
//    void* arg)
//{
//    OWlRadarTracksFusion* fusion = (OWlRadarTracksFusion*)userObject;
//    if (!fusion || !arg)
//    {
//        return 0;
//    }
//
//    SPxUniTrackDatabase::Event_t* ev = (SPxUniTrackDatabase::Event_t*)arg;
//
//    if (ev->type == SPxUniTrackDatabase::EVENT_TRACK_CREATE)
//    {
//        SPxUniTrack* track = ev->info.trackEvt.track;
//        if (!track)
//            return 0;
//
//        if (track->GetTrackType() == SPxUniTrack::TRACK_TYPE_RADAR)
//        {
//            fusion->onRadarTrackCreate((SPxRadarTrack*)track);
//        }
//        else if (track->GetTrackType() == SPxUniTrack::TRACK_TYPE_FUSED)
//        {
//            fusion->onFusedTrackCreate((SPxRadarTrack*)track);
//        }
//    }
//    else if (ev->type == SPxUniTrackDatabase::EVENT_TRACK_UPDATE)
//    {
//        SPxUniTrack* track = ev->info.trackEvt.track;
//        if (!track)
//            return 0;
//
//        if (track->GetTrackType() == SPxUniTrack::TRACK_TYPE_RADAR)
//        {
//            fusion->onRadarTrackUpdate((SPxRadarTrack*)track);
//        }
//        else if (track->GetTrackType() == SPxUniTrack::TRACK_TYPE_FUSED)
//        {
//            fusion->onFusedTrackUpdate((SPxRadarTrack*)track);
//        }
//    }
//    else if (ev->type == SPxUniTrackDatabase::EVENT_TRACK_DELETE)
//    {
//        SPxUniTrack* track = ev->info.trackEvt.track;
//        if (!track)
//            return 0;
//
//        if (track->GetTrackType() == SPxUniTrack::TRACK_TYPE_RADAR)
//        {
//            fusion->onRadarTrackDelete((SPxRadarTrack*)track);
//        }
//        else if (track->GetTrackType() == SPxUniTrack::TRACK_TYPE_FUSED)
//        {
//            fusion->onFusedTrackDelete((SPxRadarTrack*)track);
//        }
//    }
//    else if (ev->type == SPxUniTrackDatabase::EVENT_TRACK_SELECT)
//    {
//        // 可在此处补充选中航迹的调试信息
//    }
//
//    return 0;
//}
//
////============================================================
//// 各类轨迹的事件处理实现
////============================================================
//void OWlRadarTracksFusion::checkFusedTrackUserData(SPxRadarTrack* fusedTrack)
//{
//    if (!fusedTrack)
//        return;
//
//    FusedTrackUserData* fud = (FusedTrackUserData*)fusedTrack->GetUserData();
//    if (!fud)
//    {
//        fud = new FusedTrackUserData();
//        fud->trackVec.clear();
//        fusedTrack->SetUserData(fud);
//    }
//}
//void OWlRadarTracksFusion::checkRadarTrackUserData(SPxRadarTrack* radarTrack)
//{
//    if (!radarTrack)
//        return;
//
//    RadarTrackUserData* rud = (RadarTrackUserData*)radarTrack->GetUserData();
//    if (!rud)
//    {
//        rud = new RadarTrackUserData();
//        memset(rud, 0, sizeof(RadarTrackUserData));
//        radarTrack->SetUserData(rud);
//    }
//}
//
//// 雷达航迹创建：仅做 userData 初始化
//void OWlRadarTracksFusion::onRadarTrackCreate(SPxRadarTrack* track)
//{
//    checkRadarTrackUserData(track);
//}
//
//// 融合航迹更新：确保 FusedTrackUserData 存在
//void OWlRadarTracksFusion::onFusedTrackUpdate(SPxRadarTrack* fusedTrack)
//{
//    checkFusedTrackUserData(fusedTrack);
//
//    FusedTrackUserData* fud = (FusedTrackUserData*)fusedTrack->GetUserData();
//    if (!fud)
//    {
//        const SPxPacketTrackExtended* extRpt = fusedTrack->GetExtRpt();
//        if (extRpt && extRpt->fusion.trackID[0] != 0)
//        {
//            fud->trackVec.push_back(extRpt->fusion.trackID[0]);
//        }
//    }
//}
//
//// 雷达航迹删除：从对应 fused 中移除该雷达，并在必要时触发 fused 删除
//void OWlRadarTracksFusion::onRadarTrackDelete(SPxRadarTrack* track)
//{
//    if (!track)
//        return;
//
//    RadarTrackUserData* rud = (RadarTrackUserData*)track->GetUserData();
//    if (!rud)
//        return;
//
//    SPxRadarTrack* fusedTrack =
//        (SPxRadarTrack*)fusedTrackDB->GetTrack(rud->fusedID);
//    if (!fusedTrack)
//    {
//        delete rud;
//        return;
//    }
//
//    FusedTrackUserData* fud =
//        (FusedTrackUserData*)fusedTrack->GetUserData();
//    if (!fud)
//    {
//        delete rud;
//        return;
//    }
//
//    const SPxPacketTrackExtended* fusedExtRptConst =
//        fusedTrack->GetExtRpt();
//    if (!fusedExtRptConst)
//    {
//        delete rud;
//        return;
//    }
//    SPxPacketTrackExtended* fusedExtRpt =
//        (SPxPacketTrackExtended*)fusedExtRptConst;
//
//    // 从融合航迹列表中移除本雷达 unifiedID
//    std::vector<UINT32>& vec = fud->trackVec;
//    vec.erase(
//        std::remove(vec.begin(), vec.end(), track->GetUnifiedID()),
//        vec.end());
//
//    SPxTime_t now;
//    SPxTimeGetEpoch(&now);
//
//    int numSensors = (int)vec.size();
//    if (numSensors == 0)
//    {
//        // 没有任何雷达参与该 fused -> 发送删除包
//        sendFusedTrack(fusedTrack, FUSED_TRACK_STATUS_DELETE);
//    }
//    else
//    {
//        // 至少还有一个雷达源：重新计算 fused 轨迹的综合位置和速度
//        updateFusedTrackInfo(fusedTrack);
//        sendFusedTrack(fusedTrack, FUSED_TRACK_STATUS_UPDATE);
//
//    }
//
//    delete rud;
//    track->SetUserData(nullptr);
//}
//
//// 融合航迹删除：把所有参与雷达的 fusedID 清空，并释放 ID
//void OWlRadarTracksFusion::onFusedTrackDelete(SPxRadarTrack* fusedTrack)
//{
//    if (!fusedTrack)
//        return;
//
//    FusedTrackUserData* fud = (FusedTrackUserData*)fusedTrack->GetUserData();
//    if (fud)
//    {
//        for (size_t i = 0; i < fud->trackVec.size(); ++i)
//        {
//            UINT32 uniqueID = fud->trackVec[i];
//            SPxRadarTrack* radarTrack =
//                (SPxRadarTrack*)m_uniTrackDb->GetTrack(uniqueID);
//            if (!radarTrack)
//                continue;
//            //重置内部关联雷达航迹对应的rud
//            RadarTrackUserData* rud =
//                (RadarTrackUserData*)radarTrack->GetUserData();
//            if (rud && rud->fusedID == fusedTrack->GetID())
//            {
//                resetRadarTrackUserData(rud);
//            }
//        }
//
//        delete fud;
//        fusedTrack->SetUserData(nullptr);
//    }
//
//    // 释放 ID 回池
//    g_fusedIdPool.releaseID(fusedTrack->GetID());
//}
//
//// 雷达航迹更新：驱动整个融合逻辑
//void OWlRadarTracksFusion::onRadarTrackUpdate(SPxRadarTrack* track)
//{
//    if (!track)
//        return;
//
//    SPxAutoLock lock(m_lock);
//
//    // 确保 userData 存在
//    RadarTrackUserData* rud =
//        (RadarTrackUserData*)track->GetUserData();
//    if (!rud)
//    {
//        rud = new RadarTrackUserData();
//        memset(rud, 0, sizeof(RadarTrackUserData));
//        track->SetUserData(rud);
//    }
//
//    // 刚入库的前几次更新不参与融合（避免刚出现时 jitter 影响）
//    ++rud->count;
//    if (rud->count < 2)
//    {
//        return;
//    }
//
//    // 调试：选中航迹时输出其融合状态
//    if (track->GetIsSelected())
//    {
//        char buf[256] = { 0 };
//        sprintf_s(buf,
//            "radar track=%d fusedId=%u preFusedId=%u hit=%u fail=%u",
//            track->GetID(),
//            rud->fusedID,
//            rud->preFusedID,
//            rud->hitCount,
//            rud->failCount);
//        OWlError(OWL_ERR_INFO, buf);
//    }
//
//    if (rud->fusedID == 0)
//    {
//        // 还没有融合航迹 -> 走“未关联”流程
//        handleUnAssiocatedRadarTrack(track);
//    }
//    else
//    {
//        // 已经挂在某个融合航迹下 -> 走“已关联”流程
//        handleAassiocatedRadarTrack(track);
//    }
//}
//
////============================================================
//// 融合核心：fuseEngine + 预融合 / 维护逻辑
////============================================================
//
//// 椭圆门限判定：track 是否在 fusedTrack 的“空间门限”内
//bool OWlRadarTracksFusion::fuseEngine(
//    SPxRadarTrack* track,
//    SPxRadarTrack* fusedTrack,
//    UINT8          threshMulti /*=3*/)
//{
//    if (!track || !fusedTrack)
//        return false;
//
//    const SPxPacketTrackExtended* interExt = fusedTrack->GetExtRpt();
//    const SPxPacketTrackExtended* trackExt = track->GetExtRpt();
//    if (!interExt || !trackExt)
//        return false;
//
//    // 观测误差：使用可配置的类成员
//    const double aziDiffDeg = radarAziObserveDiffDegs;
//    const double aziDiffRad = aziDiffDeg * PI / 180.0;
//    const double disDiffMetres = radarDisObserveDiffMetres;
//
//    // 融合航迹当前极坐标 / 经纬度
//    double interRange = interExt->norm.min.rangeMetres;
//    double interAziDeg = interExt->norm.min.azimuthDegrees;
//    double interAziRad = interAziDeg * PI / 180.0;
//    double interLatDeg = interExt->latDegs;
//    double interLonDeg = interExt->longDegs;
//
//    // 待融合雷达航迹极坐标 / 经纬度
//    double trackRange = trackExt->norm.min.rangeMetres;
//    double trackAziDeg = trackExt->norm.min.azimuthDegrees;
//    double trackAziRad = trackAziDeg * PI / 180.0;
//    double trackLatDeg = trackExt->latDegs;
//    double trackLonDeg = trackExt->longDegs;
//
//    // 将方位/距离误差分解到 X/Y 方向的测量误差
//    double interErrX =
//        disDiffMetres * std::sin(interAziRad) +
//        interRange * aziDiffRad * std::cos(interAziRad);
//    double interErrY =
//        disDiffMetres * std::cos(interAziRad) +
//        interRange * aziDiffRad * std::sin(interAziRad);
//
//    double trackErrX =
//        disDiffMetres * std::sin(trackAziRad) +
//        trackRange * aziDiffRad * std::cos(trackAziRad);
//    double trackErrY =
//        disDiffMetres * std::cos(trackAziRad) +
//        trackRange * aziDiffRad * std::sin(trackAziRad);
//
//    // 经纬度差 -> 米级 X/Y 差
//    double dxMetres = 0.0;
//    double dyMetres = 0.0;
//    SPxLatLongDiffMetres(
//        interLatDeg, interLonDeg,
//        trackLatDeg, trackLonDeg,
//        &dxMetres, &dyMetres);
//    double diffXMetres = std::fabs(dxMetres);
//    double diffYMetres = std::fabs(dyMetres);
//
//    // 椭圆门限（threshMulti 为 3 或 4）
//    double threshXMetres = threshMulti *
//        std::sqrt(interErrX * interErrX + trackErrX * trackErrX);
//    double threshYMetres = threshMulti *
//        std::sqrt(interErrY * interErrY + trackErrY * trackErrY);
//
//    bool isXHit = diffXMetres <= (threshXMetres + EPS);
//    bool isYHit = diffYMetres <= (threshYMetres + EPS);
//
//    bool ret = isXHit && isYHit;
//
//    // 如果选中该雷达航迹，则输出详细门限信息用于调试
//    if (track->GetIsSelected())
//    {
//        char buf[256] = { 0 };
//        sprintf_s(buf,
//            "fuseEngine: track=%d fused=%d diffX=%.2f thrX=%.2f diffY=%.2f thrY=%.2f hit=%d",
//            track->GetID(), fusedTrack->GetID(),
//            diffXMetres, threshXMetres,
//            diffYMetres, threshYMetres,
//            ret ? 1 : 0);
//        OWlError(OWL_ERR_INFO, buf);
//    }
//
//    return ret;
//}
//
//// 预融合：rud->preFusedID != 0 时调用
//void OWlRadarTracksFusion::preFuse(SPxRadarTrack* track)
//{
//    if (!track)
//        return;
//
//    RadarTrackUserData* rud =
//        (RadarTrackUserData*)track->GetUserData();
//    if (!rud || rud->preFusedID == 0)
//        return;
//
//    SPxRadarTrack* fusedTrack =
//        (SPxRadarTrack*)fusedTrackDB->GetTrack(rud->preFusedID);
//    if (!fusedTrack)
//    {
//        // 候选 fused 已被删除，清空预融合状态
//        rud->preFusedID = 0;
//        rud->hitCount = 0;
//        return;
//    }
//
//    bool isFused = fuseEngine(track, fusedTrack, 3);
//    if (isFused)
//    {
//        if (rud->hitCount < 255)
//            ++rud->hitCount;
//    }
//    else
//    {
//        // 一次失败就放弃当前候选
//        rud->preFusedID = 0;
//        rud->hitCount = 0;
//        return;
//    }
//
//    if (rud->hitCount < FUSION_HIT_COUNT_THRESH)
//    {
//        // 命中次数还不够，继续等待
//        return;
//    }
//
//    //========================================================
//    // 命中次数达到阈值：把本雷达真正挂到 fusedTrack 上
//    //========================================================
//
//    // 如之前已经挂在另一个 fused 下，需要先从旧 fused 解绑
//    if (rud->fusedID != 0 && rud->fusedID != fusedTrack->GetID())
//    {
//        SPxRadarTrack* oldFusedTrack =
//            (SPxRadarTrack*)fusedTrackDB->GetTrack(rud->fusedID);
//        if (oldFusedTrack)
//        {
//            FusedTrackUserData* oldFud =
//                (FusedTrackUserData*)oldFusedTrack->GetUserData();
//            if (oldFud)
//            {
//                std::vector<UINT32>& vec = oldFud->trackVec;
//                vec.erase(
//                    std::remove(vec.begin(), vec.end(), track->GetUnifiedID()),
//                    vec.end());
//
//                if (vec.empty())
//                {
//                    // 旧 fused 已经没有任何雷达源 -> 发删除包
//                    sendFusedTrack(oldFusedTrack, FUSED_TRACK_STATUS_DELETE);
//                    g_fusedIdPool.releaseID(oldFusedTrack->GetID());
//                }
//            }
//
//            char buf[256] = { 0 };
//            sprintf_s(buf,
//                "radar track=%d change fused from %u to %u in preFuse",
//                track->GetID(), rud->fusedID, fusedTrack->GetID());
//            OWlError(OWL_ERR_WARNING, buf);
//        }
//    }
//
//    // 把当前雷达航迹加入新 fusedTrack
//    FusedTrackUserData* fud =
//        (FusedTrackUserData*)fusedTrack->GetUserData();
//    if (!fud)
//    {
//        fud = new FusedTrackUserData();
//        fud->trackVec.clear();
//        fusedTrack->SetUserData(fud);
//    }
//    if (std::find(fud->trackVec.begin(), fud->trackVec.end(),
//        track->GetUnifiedID()) == fud->trackVec.end())
//    {
//        fud->trackVec.push_back(track->GetUnifiedID());
//    }
//
//    // 重新计算融合航迹位置和速度
//    SPxPacketTrackExtended* fusedExtRpt =
//        (SPxPacketTrackExtended*)fusedTrack->GetExtRpt();
//    if (fusedExtRpt)
//    {
//        updateFusedTrackInfo(fusedTrack, track);
//        sendFusedTrack(fusedTrack, FUSED_TRACK_STATUS_UPDATE);
//    }
//
//    // 刷新雷达 userData
//    rud->fusedID = fusedTrack->GetID();
//    rud->preFusedID = 0;
//    rud->hitCount = 0;
//    rud->failCount = 0;
//}
//
////=======================
//// 多雷达位置 / 速度组合
////=======================
//void OWlRadarTracksFusion::computeFusedTrackPosSpeedCourse(
//    SPxRadarTrack* fusedTrack,
//    SPxLatLong_t* posRet,
//    double* speedRet,
//    double* courseRet)
//{
//    if (!fusedTrack || !posRet || !speedRet || !courseRet)
//        return;
//
//    FusedTrackUserData* fud =
//        (FusedTrackUserData*)fusedTrack->GetUserData();
//    if (!fud || fud->trackVec.empty())
//    {
//        const SPxPacketTrackExtended* extRpt = fusedTrack->GetExtRpt();
//        if (extRpt)
//        {
//            posRet->latDegs = extRpt->latDegs;
//            posRet->longDegs = extRpt->longDegs;
//            *speedRet = extRpt->norm.min.speedMps;
//            *courseRet = extRpt->norm.min.courseDegrees;
//        }
//        return;
//    }
//
//    double sumW = 0.0;
//    double latW = 0.0;
//    double lonW = 0.0;
//    double vxW = 0.0;
//    double vyW = 0.0;
//
//    for (size_t i = 0; i < fud->trackVec.size(); ++i)
//    {
//        UINT32 uniqueID = fud->trackVec[i];
//        SPxRadarTrack* radarTrack =
//            (SPxRadarTrack*)m_uniTrackDb->GetTrack(uniqueID);
//        if (!radarTrack)
//            continue;
//
//        double rangeMetres = radarTrack->GetRangeMetres();
//        if (rangeMetres < EPS)
//            rangeMetres = 100.0;   // 避免除零
//        double weight = 1.0 / rangeMetres;
//
//        double latDegs = 0.0, lonDegs = 0.0;
//        radarTrack->GetLatLong(&latDegs, &lonDegs);
//
//        double speed = 0.0;
//        double courseDeg = 0.0;
//        radarTrack->GetVelocity(&speed, &courseDeg);
//        double courseRad = courseDeg * PI / 180.0;
//        double vx = speed * std::sin(courseRad);
//        double vy = speed * std::cos(courseRad);
//
//        sumW += weight;
//        latW += latDegs * weight;
//        lonW += lonDegs * weight;
//        vxW += vx * weight;
//        vyW += vy * weight;
//    }
//
//    if (sumW < EPS)
//    {
//        const SPxPacketTrackExtended* extRpt = fusedTrack->GetExtRpt();
//        if (extRpt)
//        {
//            posRet->latDegs = extRpt->latDegs;
//            posRet->longDegs = extRpt->longDegs;
//            *speedRet = extRpt->norm.min.speedMps;
//            *courseRet = extRpt->norm.min.courseDegrees;
//        }
//        return;
//    }
//
//    posRet->latDegs = latW / sumW;
//    posRet->longDegs = lonW / sumW;
//
//    double vx = vxW / sumW;
//    double vy = vyW / sumW;
//
//    double speed = std::sqrt(vx * vx + vy * vy);
//    double courseDeg = 0.0;
//
//    if (std::fabs(vx) < EPS && std::fabs(vy) < EPS)
//    {
//        courseDeg = 0.0;
//    }
//    else
//    {
//        courseDeg = std::atan2(vx, vy) * 180.0 / PI;
//        if (courseDeg < 0.0)
//            courseDeg += 360.0;
//    }
//
//    *speedRet = speed;
//    *courseRet = courseDeg;
//}
//
////============================================================
//// 融合航迹创建：由本模块回灌的 fused 创建时调用
////============================================================
//void OWlRadarTracksFusion::onFusedTrackCreate(SPxRadarTrack* track)
//{
//    if (!track)
//        return;
//
//    const SPxPacketTrackExtended* extRpt = track->GetExtRpt();
//    if (!extRpt)
//        return;
//
//    int trackUnifiedID = extRpt->fusion.trackID[0];
//    SPxRadarTrack* radarTrack =
//        (SPxRadarTrack*)m_uniTrackDb->GetTrack(trackUnifiedID);
//    if (radarTrack)
//    {
//        RadarTrackUserData* rud =
//            (RadarTrackUserData*)radarTrack->GetUserData();
//        if (rud)
//        {
//            // 让该雷达航迹知道它现在挂在哪个 fused 下
//            rud->fusedID = track->GetID();
//        }
//    }
//}
//
////============================================================
//// 未关联雷达航迹：尝试与现有 fused 预融合 / 新建 fused
////============================================================
//void OWlRadarTracksFusion::handleUnAssiocatedRadarTrack(SPxRadarTrack* track)
//{
//    if (!track)
//        return;
//
//    RadarTrackUserData* rud =
//        (RadarTrackUserData*)track->GetUserData();
//    const SPxPacketTrackExtended* extRpt = track->GetExtRpt();
//    if (!rud || !extRpt)
//        return;
//
//    // 已处于预融合阶段，则优先走预融合逻辑
//    if (rud->preFusedID != 0)
//    {
//        preFuse(track);
//        return;
//    }
//
//    // 以当前雷达位置在 fusedTrackDB 中搜索最近的融合航迹
//    double disMetresRtn = 0.0;
//    SPxRadarTrack* fusedTrack =
//        fusedTrackDB->GetNearestTrackLatLong(
//            extRpt->latDegs, extRpt->longDegs, 1, &disMetresRtn);
//
//    bool isNearestFusedValid = false;
//    bool isFused = false;
//
//    if (fusedTrack)
//    {
//        checkFusedTrackUserData(fusedTrack);
//        FusedTrackUserData* fud =
//            (FusedTrackUserData*)fusedTrack->GetUserData();
//
//        // 限制：同一个 fused 中，不允许来自同一雷达源的多条航迹
//        int  srcIndex = track->GetSourceIndex();
//        bool isFromOtherSensor = true;
//
//        if (srcIndex != -1)
//        {
//            for (size_t i = 0; i < fud->trackVec.size(); ++i)
//            {
//                UINT32 uniqueID = fud->trackVec[i];
//                SPxRadarTrack* t =
//                    (SPxRadarTrack*)m_uniTrackDb->GetTrack(uniqueID);
//                if (!t)
//                    continue;
//
//                if (t->GetSourceIndex() == srcIndex)
//                {
//                    isFromOtherSensor = false;
//                    break;
//                }
//            }
//        }
//
//        if (srcIndex != -1 &&
//            isFromOtherSensor &&
//            disMetresRtn < OWL_FUSED_DISTANCE_THRESH_METRES + EPS)
//        {
//            isNearestFusedValid = true;
//
//            // 使用 fuseEngine 做一次空间门限判定
//            isFused = fuseEngine(track, fusedTrack, 3);
//            if (isFused)
//            {
//                rud->preFusedID = fusedTrack->GetID();
//                ++rud->hitCount;
//                rud->failCount = 0;
//            }
//            else
//            {
//                ++rud->failCount;
//            }
//        }
//    }
//
//    // 没有有效候选，或连续失败过多 -> 新建融合航迹
//    if (!isNearestFusedValid || rud->failCount > FUSION_FIAL_COUNT_THRESH)
//    {
//        rud->preFusedID = 0;
//        rud->hitCount = 0;
//        rud->failCount = 0;
//
//    
//		UINT32 newFusedID = 0;
//        sendFusedTrack(NULL, FUSED_TRACK_STATUS_CREATE,track,&newFusedID);
//        rud->fusedID = newFusedID;
//
//        char logbuf[128] = { 0 };
//        sprintf_s(logbuf,
//            "create fusedTrack=%u from radarTrack=%d",
//            rud->fusedID, track->GetID());
//        //OWlError(OWL_ERR_INFO, logbuf);
//    }
//}
//
////============================================================
//// 已关联雷达航迹：维护或迁移融合关系
////============================================================
//void OWlRadarTracksFusion::handleAassiocatedRadarTrack(SPxRadarTrack* track)
//{
//    if (!track)
//        return;
//
//    RadarTrackUserData* rud =
//        (RadarTrackUserData*)track->GetUserData();
//    if (!rud)
//        return;
//
//    SPxRadarTrack* fusedTrack =
//        (SPxRadarTrack*)fusedTrackDB->GetTrack(rud->fusedID);
//    if (!fusedTrack)
//    {
//        // 融合航迹已不存在，清空状态
//        resetRadarTrackUserData(rud);
//        return;
//    }
//
//    FusedTrackUserData* fud =
//        (FusedTrackUserData*)fusedTrack->GetUserData();
//
//    if (!fud || fud->trackVec.size() <= 1)
//    {
//        // 只有本雷达参与该 fused -> 允许迁移
//        handleAassiocatedRadarTrackFusedSingle(track, fusedTrack);
//        return;
//    }
//
//    // 多雷达共同支撑该 fused：只做维持性门限判定
//    bool isFused = fuseEngine(track, fusedTrack, 4);
//    if (isFused)
//    {
//        rud->failCount = 0;
//    }
//    else
//    {
//        ++rud->failCount;
//        if (rud->failCount > FUSION_FIAL_COUNT_THRESH)
//        {
//            // 移除本雷达
//            std::vector<UINT32>& vec = fud->trackVec;
//            vec.erase(
//                std::remove(vec.begin(), vec.end(), track->GetUnifiedID()),
//                vec.end());
//
//            char buf[160] = { 0 };
//            sprintf_s(buf,
//                "remove radar track=%d from fused=%u (failCount=%u)",
//                track->GetID(),
//                fusedTrack->GetID(),
//                rud->failCount);
//            OWlError(OWL_ERR_INFO, buf);
//
//            resetRadarTrackUserData(rud);
//            return;
//        }
//    }
//
//    // 如果仍然认为本雷达与 fused 匹配，则更新 fused 的位置/速度
//    const SPxPacketTrackExtended* fusedExtConst = fusedTrack->GetExtRpt();
//    if (!fusedExtConst)
//        return;
//
//    SPxPacketTrackExtended* fusedExt =
//        (SPxPacketTrackExtended*)fusedExtConst;
//
//    SPxLatLong_t pos;
//    double       speed = 0.0;
//    double       course = 0.0;
//
//    if (fud && fud->trackVec.size() > 1)
//    {
//        computeFusedTrackPosSpeedCourse(fusedTrack, &pos, &speed, &course);
//    }
//    else
//    {
//        // 防御性处理：退化为单雷达情况
//        double lat = 0.0, lon = 0.0;
//        track->GetLatLong(&lat, &lon);
//        double s = 0.0, c = 0.0;
//        track->GetVelocity(&s, &c);
//
//        pos.latDegs = lat;
//        pos.longDegs = lon;
//        speed = s;
//        course = c;
//    }
//
//    fusedExt->latDegs = pos.latDegs;
//    fusedExt->longDegs = pos.longDegs;
//    fusedExt->norm.min.speedMps = speed;
//    fusedExt->norm.min.courseDegrees = course;
//    fusedExt->age++;
//
//    fusedExt->norm.min.sizeDegrees = track->GetMinRpt()->sizeDegrees;
//    fusedExt->norm.min.sizeMetres = track->GetMinRpt()->sizeMetres;
//
//    double azi = 0.0, range = 0.0;
//    CoordinateTransformation ct;
//    ct.PolarCoordinates_Calculate(
//        m_sensorFusedRadarLong, m_sensorFusedRadarLat,
//        fusedExt->longDegs, fusedExt->latDegs,
//        azi, range);
//    fusedExt->norm.min.azimuthDegrees = azi;
//    fusedExt->norm.min.rangeMetres = range;
//
//    double xM = 0.0, yM = 0.0;
//    SPxLatLongDiffMetres(
//        m_sensorFusedRadarLat, m_sensorFusedRadarLong,
//        fusedExt->latDegs, fusedExt->longDegs,
//        &xM, &yM);
//    fusedExt->norm.xMetres = xM;
//    fusedExt->norm.yMetres = yM;
//
//    SPxTime_t now;
//    SPxTimeGetEpoch(&now);
//    fusedTrack->Update(&now,
//        &(fusedExt->norm.min),
//        &(fusedExt->norm),
//        fusedExt);
//
//    const int bufLen = sizeof(SPxPacketTrackExtended);
//    unsigned char buf[bufLen] = { 0 };
//    const SPxPacketTrackExtended* ext = fusedTrack->GetExtRpt();
//    SPxRemoteServer::TrackPackNet(&(ext->norm.min), &(ext->norm), ext, buf, bufLen);
//    m_trackFusedSenderLocal->SendPacketB(SPX_PACKET_TYPEB_TRACK_EXT, now, &buf, bufLen);
//    m_trackFusedSender->SendPacketB(SPX_PACKET_TYPEB_TRACK_EXT, now, &buf, bufLen);
//   
//}
//
//// 单雷达 fused：允许该雷达迁移到更合适的 fused
//void OWlRadarTracksFusion::handleAassiocatedRadarTrackFusedSingle(
//    SPxRadarTrack* track,
//    SPxRadarTrack* fusedTrack)
//{
//    if (!track || !fusedTrack)
//        return;
//
//    RadarTrackUserData* rud =
//        (RadarTrackUserData*)track->GetUserData();
//    const SPxPacketTrackExtended* extRpt = track->GetExtRpt();
//    if (!rud || !extRpt)
//        return;
//
//    // 若已经有预融合目标，则按预融合状态机走
//    if (rud->preFusedID != 0)
//    {
//        preFuse(track);
//        return;
//    }
//
//    // 找到距离该雷达最近的融合航迹（不能是自己）
//    double disMetresRtn = 0.0;
//    SPxRadarTrack* fusedNearest =
//        fusedTrackDB->GetNearestTrackLatLong(
//            extRpt->latDegs, extRpt->longDegs, 1, &disMetresRtn);
//
//    if (!fusedNearest || fusedNearest->GetID() == fusedTrack->GetID())
//        return;
//
//    FusedTrackUserData* fudNearest =
//        (FusedTrackUserData*)fusedNearest->GetUserData();
//    if (!fudNearest)
//    {
//        fudNearest = new FusedTrackUserData();
//        fudNearest->trackVec.clear();
//        fusedNearest->SetUserData(fudNearest);
//    }
//
//    int  srcIndex = track->GetSourceIndex();
//    bool isFromOtherSensor = true;
//
//    if (srcIndex != -1)
//    {
//        for (size_t i = 0; i < fudNearest->trackVec.size(); ++i)
//        {
//            UINT32 uniqueID = fudNearest->trackVec[i];
//            SPxRadarTrack* t =
//                (SPxRadarTrack*)m_uniTrackDb->GetTrack(uniqueID);
//            if (!t)
//                continue;
//
//            if (t->GetSourceIndex() == srcIndex)
//            {
//                isFromOtherSensor = false;
//                break;
//            }
//        }
//    }
//
//    bool isNearestFusedValid =
//        (srcIndex != -1 &&
//            isFromOtherSensor &&
//            disMetresRtn < OWL_FUSED_DISTANCE_THRESH_METRES + EPS);
//
//    if (isNearestFusedValid)
//    {
//        bool isFused = fuseEngine(track, fusedNearest, 3);
//        if (isFused)
//        {
//            rud->preFusedID = fusedNearest->GetID();
//            ++rud->hitCount;
//            rud->failCount = 0;
//
//            char buf[160] = { 0 };
//            sprintf_s(buf,
//                "single-fused pre-fusion start: radar=%d candidate fused=%u",
//                track->GetID(), fusedNearest->GetID());
//            OWlError(OWL_ERR_INFO, buf);
//        }
//        else
//        {
//            ++rud->failCount;
//        }
//    }
//
//    if (!isNearestFusedValid || rud->failCount > FUSION_FIAL_COUNT_THRESH)
//    {
//        rud->preFusedID = 0;
//        rud->hitCount = 0;
//        rud->failCount = 0;
//    }
//}
//
//// 重置雷达 userData 的融合状态
//void OWlRadarTracksFusion::resetRadarTrackUserData(RadarTrackUserData* rud)
//{
//    if (!rud)
//        return;
//
//    rud->fusedID = 0;
//    rud->preFusedID = 0;
//    rud->hitCount = 0;
//    rud->failCount = 0;
//    rud->count = 0;
//}
//
//void OWlRadarTracksFusion::sendFusedTrack(SPxRadarTrack* fusedTrack, OWL_FUSED_TRACK_STATUS sta
//    , SPxRadarTrack* radarTrack,UINT32* newFusedIDRet)
//{
//    SPxTime_t now;
//    SPxTimeGetEpoch(&now);
//    if (sta == FUSED_TRACK_STATUS_DELETE)
//    {
//        SPxPacketTrackExtended ext = { 0 };
//        ext.norm.min.id = fusedTrack->GetID();
//        ext.norm.min.status = SPX_PACKET_TRACK_STATUS_DELETED;
//
//        const int bufLen = sizeof(SPxPacketTrackExtended);
//        unsigned char buf[bufLen] = { 0 };
//        SPxRemoteServer::TrackPackNet(&(ext.norm.min), &(ext.norm), &ext, buf, bufLen);
//
//        m_trackFusedSenderLocal->SendPacketB(
//            SPX_PACKET_TYPEB_TRACK_EXT, now, &buf, bufLen);
//        m_trackFusedSender->SendPacketB(
//            SPX_PACKET_TYPEB_TRACK_EXT, now, &buf, bufLen);
//    }
//
//    if (sta == FUSED_TRACK_STATUS_CREATE)
//    {
//        const SPxPacketTrackExtended* extRpt = radarTrack->GetExtRpt();
//        SPxPacketTrackExtended ext = { 0 };
//        memcpy(&ext, extRpt, sizeof(SPxPacketTrackExtended));
//
//        ext.norm.min.uniTrackType = SPxUniTrack::TRACK_TYPE_FUSED;
//        ext.norm.min.senderID = OWL_FUSED_SENDER_ID;
//
//        int newID = g_fusedIdPool.getID();
//        if (newID <= 0)
//        {
//            OWlError(OWL_ERR_ERROR, "handleUnAssiocated: no free fused ID in pool.");
//            return;
//        }
//
//        ext.norm.min.id = (UINT32)newID;
//        ext.extMask = 0xFFFFFFFF;
//
//        ext.fusion.sensors = SENSOR_0_FLAG;          // 简单约定：第 0 bit 表示某一雷达源
//        ext.fusion.trackID[0] = radarTrack->GetUnifiedID();
//        ext.age = 1;
//
//        // 尺寸信息沿用当前雷达航迹
//        ext.norm.min.sizeDegrees = radarTrack->GetMinRpt()->sizeDegrees;
//        ext.norm.min.sizeMetres = radarTrack->GetMinRpt()->sizeMetres;
//
//        // 经纬度 -> 极坐标（以融合雷达为基准）
//        double azi = 0.0, range = 0.0;
//        CoordinateTransformation ct;
//        ct.PolarCoordinates_Calculate(
//            m_sensorFusedRadarLong, m_sensorFusedRadarLat,
//            extRpt->longDegs, extRpt->latDegs,
//            azi, range);
//        ext.norm.min.azimuthDegrees = azi;
//        ext.norm.min.rangeMetres = range;
//
//        // 经纬度 -> 笛卡尔坐标
//        double xM = 0.0, yM = 0.0;
//        SPxLatLongDiffMetres(
//            m_sensorFusedRadarLat, m_sensorFusedRadarLong,
//            extRpt->latDegs, extRpt->longDegs,
//            &xM, &yM);
//        ext.norm.xMetres = xM;
//        ext.norm.yMetres = yM;
//
//        // 本地回灌，触发融合航迹创建
//        const int bufLen = sizeof(SPxPacketTrackExtended);
//        unsigned char buf[bufLen] = { 0 };
//        SPxRemoteServer::TrackPackNet(
//            &(ext.norm.min), &(ext.norm), &ext, buf, bufLen);
//
//        SPxTime_t now;
//        SPxTimeGetEpoch(&now);
//     
//        m_trackFusedSenderLocal->SendPacketB(
//            SPX_PACKET_TYPEB_TRACK_EXT, now, &buf, bufLen);
//        m_trackFusedSender->SendPacketB(
//            SPX_PACKET_TYPEB_TRACK_EXT, now, &buf, bufLen);
//        *newFusedIDRet = ext.norm.min.id;
//    }
//
//    if (sta == FUSED_TRACK_STATUS_UPDATE)
//    {
//        const int bufLen = sizeof(SPxPacketTrackExtended);
//        unsigned char buf[bufLen] = { 0 };
//        const SPxPacketTrackExtended* ext = fusedTrack->GetExtRpt();
//        SPxRemoteServer::TrackPackNet(&(ext->norm.min), &(ext->norm), ext, buf, bufLen);
//        m_trackFusedSenderLocal->SendPacketB(
//            SPX_PACKET_TYPEB_TRACK_EXT, now, &buf, bufLen);
//        m_trackFusedSender->SendPacketB(
//            SPX_PACKET_TYPEB_TRACK_EXT, now, &buf, bufLen);
//    }
//   
//}
//
//void OWlRadarTracksFusion::updateFusedTrackInfo(SPxRadarTrack* fusedTrack, SPxRadarTrack* radarTrack)
//{
//    SPxTime_t now;
//    SPxTimeGetEpoch(&now);
//
//    const SPxPacketTrackExtended* fusedExtRptConst =
//        fusedTrack->GetExtRpt();
//    SPxPacketTrackExtended* fusedExtRpt =
//        (SPxPacketTrackExtended*)fusedExtRptConst;
//    // 至少还有一个雷达源：重新计算 fused 轨迹的综合位置和速度
//    SPxLatLong_t pos;
//    double       speed = 0.0;
//    double       course = 0.0;
//    computeFusedTrackPosSpeedCourse(fusedTrack, &pos, &speed, &course);
//
//    fusedExtRpt->latDegs = pos.latDegs;
//    fusedExtRpt->longDegs = pos.longDegs;
//    fusedExtRpt->norm.min.speedMps = speed;
//    fusedExtRpt->norm.min.courseDegrees = course;
//    if (radarTrack)
//    {
//        fusedExtRpt->norm.min.sizeDegrees = radarTrack->GetMinRpt()->sizeDegrees;
//		fusedExtRpt->norm.min.sizeMetres = radarTrack->GetMinRpt()->sizeMetres;
//    }
//
//    // 极坐标更新（以融合雷达为中心）
//    double azi = 0.0, range = 0.0;
//    CoordinateTransformation ct;
//    ct.PolarCoordinates_Calculate(
//        m_sensorFusedRadarLong, m_sensorFusedRadarLat,
//        fusedExtRpt->longDegs, fusedExtRpt->latDegs,
//        azi, range);
//    fusedExtRpt->norm.min.azimuthDegrees = azi;
//    fusedExtRpt->norm.min.rangeMetres = range;
//
//    // 笛卡尔坐标更新
//    double xM = 0.0, yM = 0.0;
//    SPxLatLongDiffMetres(
//        m_sensorFusedRadarLat, m_sensorFusedRadarLong,
//        fusedExtRpt->latDegs, fusedExtRpt->longDegs,
//        &xM, &yM);
//    fusedExtRpt->norm.xMetres = xM;
//    fusedExtRpt->norm.yMetres = yM;
//
//    // 更新数据库并广播
//    fusedTrack->Update(&now,
//        &(fusedExtRpt->norm.min),
//        &(fusedExtRpt->norm),
//        fusedExtRpt);
//}
//
////============================================================
//// 参数接口：当前实现支持两个简单参数
////   radarAziObsDiff    方位测量标准差（度）
////   radarDisObsDiff    距离测量标准差（米）
////============================================================
//int OWlRadarTracksFusion::SetParameter(char* parameterName, char* parameterValue)
//{
//    if (!parameterName || !parameterValue)
//        return SPX_ERR_BAD_PARAMETER_FOR_COMMAND;
//
//    if (strcmp(parameterName, "radarAziObsDiff") == 0)
//    {
//        radarAziObserveDiffDegs = atof(parameterValue);
//        m_fuseParam.radarAziObserveDifferenceVal = radarAziObserveDiffDegs;
//        return 0;
//    }
//    else if (strcmp(parameterName, "radarDisObsDiff") == 0)
//    {
//        radarDisObserveDiffMetres = atof(parameterValue);
//        m_fuseParam.radarDisObserveDifferenceVal = radarDisObserveDiffMetres;
//        return 0;
//    }
//
//    // 其他参数暂未开放
//    return SPX_ERR_BAD_PARAMETER_FOR_COMMAND;
//}
//
//int OWlRadarTracksFusion::GetParameter(char* parameterName,
//    char* valueBuf,
//    int   bufLen)
//{
//    if (!parameterName || !valueBuf || bufLen <= 0)
//        return SPX_ERR_BAD_PARAMETER_FOR_COMMAND;
//
//    if (strcmp(parameterName, "radarAziObsDiff") == 0)
//    {
//        _snprintf_s(valueBuf, bufLen, _TRUNCATE, "%.3f", radarAziObserveDiffDegs);
//        return 0;
//    }
//    else if (strcmp(parameterName, "radarDisObsDiff") == 0)
//    {
//        _snprintf_s(valueBuf, bufLen, _TRUNCATE, "%.3f", radarDisObserveDiffMetres);
//        return 0;
//    }
//
//    return SPX_ERR_BAD_PARAMETER_FOR_COMMAND;
//}
