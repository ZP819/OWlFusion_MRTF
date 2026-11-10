
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
static const int OWL_FUSED_TRACK_MAX_IDS = 3000;

/*
 * P0：发送逻辑移出大锁（仅改内部实现，不改外部接口）。
 *
 * 设计说明：
 * 1) onRadarTrackUpdate 在大锁内执行成员关系与融合状态更新；
 * 2) 锁内不直接做网络发送，而是把待发送包暂存到“线程本地队列”；
 * 3) 退出大锁后统一 flush，避免网络发送阻塞融合主锁。
 *
 * 采用 thread_local 的原因：
 * - 不改变现有函数签名与回调路径；
 * - 同一线程内可自然继承“锁内 defer、锁外 flush”的语义；
 * - 避免跨线程共享队列导致额外锁竞争。
 */
struct DeferredTrackPacketSend
{
	SPxPacketTrackExtended ext;
	bool sendToLocal = false;
	bool sendToExternal = false;
};

static thread_local bool g_isDeferringTrackPacketSend = false;
static thread_local vector<DeferredTrackPacketSend> g_deferredTrackPacketSends;

/*
 * 立即发送单个航迹扩展包。
 * 返回值：所有目标 sender 均发送成功返回 true，否则 false。
 */
static bool SendTrackPacketNow(
	SPxPacketSender* localSender,
	SPxPacketSender* externalSender,
	const SPxPacketTrackExtended& ext,
	bool sendToLocal,
	bool sendToExternal)
{
	const int bufLen = sizeof(SPxPacketTrackExtended);
	unsigned char buf[bufLen] = { 0 };
	SPxRemoteServer::TrackPackNet(&(ext.norm.min), &(ext.norm), &ext, buf, bufLen);

	SPxTime_t now;
	SPxTimeGetEpoch(&now);

	bool ok = true;
	if (sendToLocal && localSender)
	{
		SPxErrorCode err = localSender->SendPacketB(SPX_PACKET_TYPEB_TRACK_EXT, now, &buf, bufLen);
		if (err != SPX_NO_ERROR)
		{
			ok = false;
		}
	}

	if (sendToExternal && externalSender)
	{
		SPxErrorCode err = externalSender->SendPacketB(SPX_PACKET_TYPEB_TRACK_EXT, now, &buf, bufLen);
		if (err != SPX_NO_ERROR)
		{
			ok = false;
		}
	}

	return ok;
}

/*
 * 锁内调用时：入队，不发送；
 * 非锁内 defer 场景：立即发送。
 */
static bool QueueOrSendTrackPacket(
	SPxPacketSender* localSender,
	SPxPacketSender* externalSender,
	const SPxPacketTrackExtended& ext,
	bool sendToLocal,
	bool sendToExternal)
{
	if (g_isDeferringTrackPacketSend)
	{
		DeferredTrackPacketSend item;
		item.ext = ext;
		item.sendToLocal = sendToLocal;
		item.sendToExternal = sendToExternal;
		g_deferredTrackPacketSends.push_back(item);
		return true;
	}

	return SendTrackPacketNow(localSender, externalSender, ext, sendToLocal, sendToExternal);
}

/* 锁外统一下发锁内积压的发送包。 */
static void FlushDeferredTrackPackets(
	SPxPacketSender* localSender,
	SPxPacketSender* externalSender)
{
	if (g_deferredTrackPacketSends.empty())
	{
		return;
	}

	for (int i = 0; i < (int)g_deferredTrackPacketSends.size(); i++)
	{
		const DeferredTrackPacketSend& item = g_deferredTrackPacketSends[i];
		SendTrackPacketNow(localSender, externalSender, item.ext, item.sendToLocal, item.sendToExternal);
	}

	g_deferredTrackPacketSends.clear();
}

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
	std::vector<unsigned char> m_idInPool;
	int m_maxID;

public:
	/*
	 * P0-D：强化 ID 池边界与状态管理。
	 * - m_idInPool[id] == 1 表示该 ID 当前在池内可分配；
	 * - getID() 返回后会把状态置 0，releaseID() 成功回收后置 1；
	 * - 防止重复回收、非法 ID 回收以及池耗尽后脏值传播。
	 */
	IDPool(int maxIDs) : m_maxID(maxIDs)
	{
		if (m_maxID < 1)
		{
			m_maxID = 1;
		}

		m_idInPool.resize(m_maxID + 1, 0);
		for (int i = 1; i <= m_maxID; ++i)
		{
			availableIDs.push(i);
			m_idInPool[i] = 1;
		}
	}

	int getID()
	{
		/*
		 * 采用“弹出并校验”策略，兼容历史脏数据。
		 * 若池耗尽，返回 -1，由调用方显式处理失败路径。
		 */
		while (!availableIDs.empty())
		{
			int id = availableIDs.front();
			availableIDs.pop();

			if (id >= 1 && id <= m_maxID && m_idInPool[id])
			{
				m_idInPool[id] = 0;
				return id;
			}
		}

		return -1;
	}

	void releaseID(int id)
	{
		/*
		 * 非法 ID 或重复回收直接忽略，避免污染可分配队列。
		 */
		if (id < 1 || id > m_maxID)
		{
			return;
		}
		if (m_idInPool[id])
		{
			return;
		}

		m_idInPool[id] = 1;
		availableIDs.push(id);
	}

	int getMaxID() const
	{
		return m_maxID;
	}
};



IDPool m_idPool(OWL_FUSED_TRACK_MAX_IDS);



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
	/*
	 * P0-1：调试快照锁初始化。
	 * 该锁仅保护调试快照 map，不参与融合主流程判定，
	 * 避免调试读取影响融合主链路性能。
	 */
	m_debugLock.Initialise();
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
	/*
	 * P0-1：雷达目标创建时立即写入初始调试快照。
	 * 这样在目标刚出现时，标牌就能显示组网内部状态。
	 */
	updateRadarTrackDebugSnapshot(
		track,
		"radar_create",
		NULL,
		0.0,
		false,
		false);
}
void OWlRadarTracksFusion::onFusedTrackUpdate(SPxRadarTrack* track)
{
	if (!track)
	{
		return;
	}

	/*
	 * 维护 fused 的内部成员列表。
	 * P0-B 核心：不再假设 fusion.trackID[0] 是 UnifiedID，
	 * 而是按 SDK 规定通过 sensors+trackID[] 反解成员轨迹。
	 */
	FusedTrackUserData* fud = (FusedTrackUserData*)track->GetUserData();
	if (!fud)
	{
		fud = new FusedTrackUserData();
		fud->trackVec.clear();
		track->SetUserData(fud);
	}

	const SPxPacketTrackExtended* extRpt = track->GetExtRpt();
	if (!extRpt)
	{
		updateFusedTrackDebugSnapshot(track, "fused_update_no_ext", false);
		return;
	}

	vector<UINT32> memberUnifiedIDs;
	for (int slot = 0; slot < SPX_MAX_NUM_TRACK_IDS; slot++)
	{
		SPxRadarTrack* radarTrack = findRadarTrackByFusionSlot(extRpt, slot);
		if (radarTrack)
		{
			memberUnifiedIDs.push_back(radarTrack->GetUnifiedID());
		}
	}

	/*
	 * 兼容历史报文：旧逻辑把 UnifiedID 填在 trackID[0]。
	 * 当按新语义反解失败时，回退到旧写法，避免中间版本失联。
	 */
	if (memberUnifiedIDs.empty() && extRpt->fusion.trackID[0] != 0)
	{
		SPxRadarTrack* radarTrack = (SPxRadarTrack*)m_uniTrackDb->GetTrack(extRpt->fusion.trackID[0]);
		if (radarTrack)
		{
			memberUnifiedIDs.push_back(radarTrack->GetUnifiedID());
		}
	}

	if (!memberUnifiedIDs.empty())
	{
		deduplicateTrackVec(memberUnifiedIDs);
		fud->trackVec = memberUnifiedIDs;
	}

	/*
	 * P0-1：记录 fused 在“外部更新回调”下的最新状态。
	 */
	updateFusedTrackDebugSnapshot(track, "fused_update", false);
}
void OWlRadarTracksFusion::onRadarTrackDelete(SPxRadarTrack* track)
{
	if (!track)
	{
		return;
	}

	UINT32 radarUnifiedID = track->GetUnifiedID();
	RadarTrackUserData* rud = (RadarTrackUserData*)track->GetUserData();
	if (!rud)
	{
		removeRadarTrackDebugSnapshot(radarUnifiedID);
		return;
	}

	updateRadarTrackDebugSnapshot(
		track,
		"radar_delete",
		NULL,
		0.0,
		false,
		false);

	SPxRadarTrack* fusedTrack = (SPxRadarTrack*)fusedTrackDB->GetTrack(rud->fusedID);
	if (fusedTrack)
	{
		FusedTrackUserData* fud = (FusedTrackUserData*)fusedTrack->GetUserData();
		if (fud)
		{
			/*
			 * P0-A：成员删除后立即按剩余成员重算 fused。
			 * 旧逻辑在成员删除后可能沿用旧 fused 状态，导致输出短暂错误。
			 */
			vector<UINT32> vec = fud->trackVec;
			RemoveUnifiedTrackFromVec(vec, track->GetUnifiedID());
			fud->trackVec = vec;

			refreshFusedTrackFromMembers(fusedTrack, NULL);
		}
	}
	else
	{
		/* 该雷达记录的 fusedID 在 fusedDB 中不存在：仅告警，不阻断删除流程 */
		int zp = 10;
	}

	delete rud;
	removeRadarTrackDebugSnapshot(radarUnifiedID);
}
void OWlRadarTracksFusion::onFusedTrackDelete(SPxRadarTrack* track)
{
	if (!track)
	{
		return;
	}

	FusedTrackUserData* fud = (FusedTrackUserData*)track->GetUserData();
	if (fud)
	{
		for (int i = 0; i < fud->trackVec.size(); i++)
		{
			UINT32 uniqueID = (fud->trackVec[i]);
			SPxRadarTrack* memberTrack = (SPxRadarTrack*)m_uniTrackDb->GetTrack(uniqueID);
			if (memberTrack)
			{
				RadarTrackUserData* rud = (RadarTrackUserData*)memberTrack->GetUserData();
				if (rud)
				{
					if (rud->fusedID != 0)
					{
						rud->fusedID = 0;
					}
				}

				/*
				 * P0-1：fused 删除后，成员雷达轨迹的调试快照也要同步为“已解绑”。
				 */
				updateRadarTrackDebugSnapshot(
					memberTrack,
					"fused_delete_detach_member",
					NULL,
					0.0,
					false,
					false);
			}
		}

		delete fud;
	}

	UINT32 id = track->GetID();
	m_idPool.releaseID(id);
	removeFusedTrackDebugSnapshot(id);
}
void OWlRadarTracksFusion::onRadarTrackUpdate(SPxRadarTrack* track)
{
	if (!track)
	{
		return;
	}

	bool shouldFlushDeferredPackets = false;

	{
		SPxAutoLock lock(m_lock);

		/*
		 * P0：在进入大锁时打开“发送延迟”开关，
		 * 锁内所有发送请求仅入队，不做网络 I/O。
		 */
		g_deferredTrackPacketSends.clear();
		g_isDeferringTrackPacketSend = true;

		//check userData
		RadarTrackUserData* rud = (RadarTrackUserData*)track->GetUserData();
		if (!rud)
		{
			rud = new RadarTrackUserData();
			memset(rud, 0, sizeof(RadarTrackUserData));
			track->SetUserData(rud);
		}

		rud->count++;
		updateRadarTrackDebugSnapshot(
			track,
			"radar_update_enter",
			NULL,
			0.0,
			false,
			false);
		if (rud->count < 2)//延迟一圈，否则来不及存入内存数据库
		{
			updateRadarTrackDebugSnapshot(
				track,
				"radar_update_wait_next_scan",
				NULL,
				0.0,
				false,
				false);
			g_isDeferringTrackPacketSend = false;
			g_deferredTrackPacketSends.clear();
			return;
		}

		/**********************************************************************/
		if (track->GetIsSelected())
		{
			char buf[1024] = { 0 };
			sprintf_s(buf, "track fusedId=%d  perfusedId=%d   hitCount=%d", rud->fusedID, rud->preFusedID, rud->hitCount);
			OWlError(OWL_ERR_INFO, buf);
		}

		if (rud->fusedID == 0)//该目标还未与融合航迹关联
		{
			handleUnAssiocatedRadarTrack(track);
		}
		else//该航迹已和融合航迹关联
		{
			handleAassiocatedRadarTrack(track);
		}

		updateRadarTrackDebugSnapshot(
			track,
			"radar_update_done",
			NULL,
			0.0,
			false,
			false);

		/* 锁内状态处理结束，允许锁外统一发送。 */
		g_isDeferringTrackPacketSend = false;
		shouldFlushDeferredPackets = true;
	}

	/*
	 * P0：网络发送统一放在锁外执行，
	 * 降低大锁持有时间，避免网络抖动放大为融合主链路阻塞。
	 */
	if (shouldFlushDeferredPackets)
	{
		FlushDeferredTrackPackets(m_trackFusedSenderLocal, m_trackFusedSender);
	}
}

bool OWlRadarTracksFusion::fuseEngine(SPxRadarTrack* track, SPxRadarTrack* fusedTrack, UINT8 threshMulti)
{
	/*
	 * Detailed runtime metrics are collected here for debug overlay:
	 * - configured threshold parameters;
	 * - actual x/y errors and x/y thresholds;
	 * - range/azimuth differences in polar space.
	 */
	bool ret = false;

	OWlFuseEvalDebugMetrics metrics = { 0 };
	metrics.threshMulti = threshMulti;
	if (track)
	{
		metrics.radarUnifiedID = track->GetUnifiedID();
		metrics.radarTrackID = track->GetID();
	}
	if (fusedTrack)
	{
		metrics.fusedID = fusedTrack->GetID();
	}

	SPxRadarTrack* interTrack = fusedTrack;
	if (!track || !interTrack)
	{
		recordFuseEvalDebugMetrics(metrics);
		return false;
	}

	const double radarAziObserveDiffDegsLocal =
		(this->radarAziObserveDiffDegs > EPS) ? this->radarAziObserveDiffDegs : EPS;
	const double radarDisObserveDiffMetresLocal =
		(this->radarDisObserveDiffMetres > EPS) ? this->radarDisObserveDiffMetres : EPS;
	const double radarAziObserveDiffRad = radarAziObserveDiffDegsLocal / 180.0 * PI;

	metrics.cfgAziObsDiffDegs = radarAziObserveDiffDegsLocal;
	metrics.cfgDisObsDiffMetres = radarDisObserveDiffMetresLocal;

	const SPxPacketTrackExtended* interTrackExtRpt = interTrack->GetExtRpt();
	const SPxPacketTrackExtended* trackExtRpt = track->GetExtRpt();
	if (!interTrackExtRpt || !trackExtRpt)
	{
		recordFuseEvalDebugMetrics(metrics);
		return false;
	}

	const double interTrackRangeMetres = interTrackExtRpt->norm.min.rangeMetres;
	const double interTrackAziDegs = interTrackExtRpt->norm.min.azimuthDegrees;
	const double interTrackAziRad = interTrackAziDegs / 180.0 * PI;
	const double interTrackLatDegs = interTrackExtRpt->latDegs;
	const double interTrackLonDegs = interTrackExtRpt->longDegs;

	const double trackRangeMetres = trackExtRpt->norm.min.rangeMetres;
	const double trackAziDegs = trackExtRpt->norm.min.azimuthDegrees;
	const double trackAziRad = trackAziDegs / 180.0 * PI;
	const double trackLatDegs = trackExtRpt->latDegs;
	const double trackLonDegs = trackExtRpt->longDegs;

	/* Convert measurement uncertainty to Cartesian axes. */
	const double interTrackMeasErrXMetres =
		radarDisObserveDiffMetresLocal * sin(interTrackAziRad)
		+ interTrackRangeMetres * radarAziObserveDiffRad * cos(interTrackAziRad);
	const double interTrackMeasErrYMetres =
		radarDisObserveDiffMetresLocal * cos(interTrackAziRad)
		+ interTrackRangeMetres * radarAziObserveDiffRad * sin(interTrackAziRad);

	const double trackMeasErrXMetres =
		radarDisObserveDiffMetresLocal * sin(trackAziRad)
		+ trackRangeMetres * radarAziObserveDiffRad * cos(trackAziRad);
	const double trackMeasErrYMetres =
		radarDisObserveDiffMetresLocal * cos(trackAziRad)
		+ trackRangeMetres * radarAziObserveDiffRad * sin(trackAziRad);

	const double threshXMetres = threshMulti * sqrt(
		(interTrackMeasErrXMetres * interTrackMeasErrXMetres)
		+ (trackMeasErrXMetres * trackMeasErrXMetres));
	const double threshYMetres = threshMulti * sqrt(
		(interTrackMeasErrYMetres * interTrackMeasErrYMetres)
		+ (trackMeasErrYMetres * trackMeasErrYMetres));

	double dxMetres = 0.0;
	double dyMetres = 0.0;
	SPxLatLongDiffMetres(interTrackLatDegs, interTrackLonDegs,
		trackLatDegs, trackLonDegs, &dxMetres, &dyMetres);

	const double diffXMetres = fabs(dxMetres);
	const double diffYMetres = fabs(dyMetres);
	const bool isXHit = (diffXMetres < threshXMetres + EPS);
	const bool isYHit = (diffYMetres < threshYMetres);

	ret = (isXHit && isYHit);

	/* Normalize azimuth difference to [0, 180] deg. */
	double diffAzimuthDegs = fabs(interTrackAziDegs - trackAziDegs);
	while (diffAzimuthDegs > 360.0)
	{
		diffAzimuthDegs -= 360.0;
	}
	if (diffAzimuthDegs > 180.0)
	{
		diffAzimuthDegs = 360.0 - diffAzimuthDegs;
	}

	metrics.hasValue = true;
	metrics.dxMetres = dxMetres;
	metrics.dyMetres = dyMetres;
	metrics.diffXMetres = diffXMetres;
	metrics.diffYMetres = diffYMetres;
	metrics.threshXMetres = threshXMetres;
	metrics.threshYMetres = threshYMetres;
	metrics.diffRangeMetres = fabs(interTrackRangeMetres - trackRangeMetres);
	metrics.diffAzimuthDegs = diffAzimuthDegs;
	metrics.isXHit = isXHit;
	metrics.isYHit = isYHit;
	metrics.isFuseHit = ret;

	recordFuseEvalDebugMetrics(metrics);

	if (track->GetIsSelected())
	{
		char buf[1024] = { 0 };
		sprintf_s(buf,
			"track=%d iner=%d diffX=%.2f thX=%.2f diffY=%.2f thY=%.2f hit=%d",
			track->GetID(), interTrack->GetID(),
			diffXMetres, threshXMetres, diffYMetres, threshYMetres,
			(ret ? 1 : 0));
		OWlError(OWL_ERR_INFO, buf);
	}

	return ret;
}

void OWlRadarTracksFusion::deduplicateTrackVec(vector<UINT32>& trackVec)
{
	for (int i = 0; i < (int)trackVec.size(); i++)
	{
		UINT32 id = trackVec[i];
		for (int j = i + 1; j < (int)trackVec.size(); )
		{
			if (trackVec[j] == id)
			{
				trackVec.erase(trackVec.begin() + j);
			}
			else
			{
				j++;
			}
		}
	}
}

/*
 * 按 fusion 槽位反查成员雷达航迹。
 * SDK 规则：trackID[] 的索引不是 sourceIndex，
 * 而是 sensors 位图中“从低到高第 N 个置位 bit”。
 */
SPxRadarTrack* OWlRadarTracksFusion::findRadarTrackByFusionSlot(
	const SPxPacketTrackExtended* extRpt,
	int slotIndex)
{
	if (!extRpt || slotIndex < 0 || slotIndex >= SPX_MAX_NUM_TRACK_IDS)
	{
		return NULL;
	}

	UINT32 sensorTrackID = extRpt->fusion.trackID[slotIndex];
	if (sensorTrackID == 0)
	{
		return NULL;
	}

	int sourceIndex = -1;
	int mappedSlot = 0;
	UINT32 sensors = extRpt->fusion.sensors;
	for (int bit = 0; bit < 32; bit++)
	{
		if ((sensors & (1u << bit)) != 0)
		{
			if (mappedSlot == slotIndex)
			{
				sourceIndex = bit;
				break;
			}
			mappedSlot++;
		}
	}
	if (sourceIndex < 0)
	{
		return NULL;
	}

	SPxUniTrack* uniTrack = m_uniTrackDb->GetTrack(
		sensorTrackID,
		SPxUniTrack::TRACK_TYPE_RADAR,
		FALSE,
		sourceIndex);
	if (!uniTrack)
	{
		return NULL;
	}

	return (SPxRadarTrack*)uniTrack;
}

/*
 * 从成员集合重建 fusion 元数据。
 * 目标：
 * - 修复 fusion.sensors / fusion.sensorTypes / fusion.trackID[] 维护不一致；
 * - 对外报文字段遵循 SDK 映射语义；
 * - 内部仍继续使用 UnifiedID 维护成员关系。
 */
void OWlRadarTracksFusion::rebuildFusionMetadataFromMembers(SPxRadarTrack* fusedTrack)
{
	if (!fusedTrack)
	{
		return;
	}

	SPxPacketTrackExtended* fusedTrackExtRpt = (SPxPacketTrackExtended*)fusedTrack->GetExtRpt();
	FusedTrackUserData* fud = (FusedTrackUserData*)fusedTrack->GetUserData();
	if (!fusedTrackExtRpt || !fud)
	{
		return;
	}

	/* 先做一次去重，确保成员集合稳定。 */
	deduplicateTrackVec(fud->trackVec);

	UINT32 sensorTrackIdBySource[32] = { 0 };
	UINT32 sensorsMask = 0;
	UINT32 sensorTypesMask = 0;

	for (int i = 0; i < (int)fud->trackVec.size(); i++)
	{
		UINT32 unifiedID = fud->trackVec[i];
		SPxRadarTrack* memberTrack = (SPxRadarTrack*)m_uniTrackDb->GetTrack(unifiedID);
		if (!memberTrack)
		{
			continue;
		}

		int sourceIndex = memberTrack->GetSourceIndex();
		if (sourceIndex < 0 || sourceIndex >= 32)
		{
			continue;
		}

		UINT32 sensorBit = (1u << sourceIndex);
		if ((sensorsMask & sensorBit) != 0)
		{
			/* 同一 source 的重复成员忽略，保持每 source 仅一个外发 trackID。 */
			continue;
		}

		UINT32 sourceTrackID = memberTrack->GetID();
		if (sourceTrackID == 0)
		{
			continue;
		}

		sensorsMask |= sensorBit;
		sensorTrackIdBySource[sourceIndex] = sourceTrackID;

		UINT32 sensorType = SPX_PACKET_TRACK_SENSOR_OTHER;
		switch (memberTrack->GetTrackType())
		{
		case SPxUniTrack::TRACK_TYPE_RADAR:
			sensorType = SPX_PACKET_TRACK_SENSOR_PRIMARY;
			break;
		case SPxUniTrack::TRACK_TYPE_AIS:
			sensorType = SPX_PACKET_TRACK_SENSOR_AIS;
			break;
		case SPxUniTrack::TRACK_TYPE_ADSB:
			sensorType = SPX_PACKET_TRACK_SENSOR_ADSB;
			break;
		default:
			sensorType = SPX_PACKET_TRACK_SENSOR_OTHER;
			break;
		}
		sensorTypesMask |= sensorType;
	}

	fusedTrackExtRpt->fusion.sensors = sensorsMask;
	fusedTrackExtRpt->fusion.sensorTypes = sensorTypesMask;
	for (int i = 0; i < SPX_MAX_NUM_TRACK_IDS; i++)
	{
		fusedTrackExtRpt->fusion.trackID[i] = 0;
	}

	int trackIdSlot = 0;
	for (int sourceIndex = 0; sourceIndex < 32 && trackIdSlot < SPX_MAX_NUM_TRACK_IDS; sourceIndex++)
	{
		UINT32 sensorBit = (1u << sourceIndex);
		if ((sensorsMask & sensorBit) == 0)
		{
			continue;
		}

		fusedTrackExtRpt->fusion.trackID[trackIdSlot] = sensorTrackIdBySource[sourceIndex];
		trackIdSlot++;
	}
}

/*
 * 统一的 fused 删除报文发送函数。
 * 说明：
 * - 删除 fused 航迹时，必须同时发本地 sender（驱动本地 fusedTrackDB 删除）和外发 sender。
 * - P0-A 目标是将删除路径统一，避免分支散落导致行为不一致。
 */
void OWlRadarTracksFusion::sendDeletePacketForFusedTrack(UINT32 fusedID)
{
	SPxPacketTrackExtended ext = { 0 };
	ext.norm.min.id = fusedID;
	ext.norm.min.status = SPX_PACKET_TRACK_STATUS_DELETED;

	/*
	 * P0-1：记录 fused 删除发送动作，便于界面观察“由重算触发删除”的链路。
	 */
	SPxRadarTrack* fusedTrack = fusedTrackDB ? fusedTrackDB->GetTrack(fusedID) : NULL;
	if (fusedTrack)
	{
		updateFusedTrackDebugSnapshot(fusedTrack, "fused_send_delete_packet", true);
	}
	else
	{
		removeFusedTrackDebugSnapshot(fusedID);
	}

	/*
	 * P0：删除包在锁内场景只入队，锁外统一 flush。
	 * 语义保持不变：本地回灌 + 对外分发都发送。
	 */
	QueueOrSendTrackPacket(m_trackFusedSenderLocal, m_trackFusedSender, ext, true, true);
}

/*
 * 按成员集合重算 fused 航迹状态并发布更新。
 * 说明：
 * - 这是 P0-A 的核心“止血点”：成员集合只要变化，就必须从成员重新计算 fused。
 * - 这样可以修复“剔除成员后仍用被剔除轨迹刷新 fused”以及“成员增删后旧 fused 不重算”的问题。
 * - 函数返回 false 表示 fused 已无成员并已发删除包；返回 true 表示 fused 已完成重算并发布更新。
 */
bool OWlRadarTracksFusion::refreshFusedTrackFromMembers(
	SPxRadarTrack* fusedTrack,
	SPxRadarTrack* preferredSizeTrack)
{
	if (!fusedTrack)
	{
		return false;
	}

	FusedTrackUserData* fud = (FusedTrackUserData*)fusedTrack->GetUserData();
	if (!fud || fud->trackVec.empty())
	{
		/*
		 * P0-1：记录“成员为空触发删除”。
		 */
		updateFusedTrackDebugSnapshot(fusedTrack, "fused_refresh_empty_members", true);
		sendDeletePacketForFusedTrack(fusedTrack->GetID());
		return false;
	}

	/*
	 * P0-B：成员列表在计算前先做去重，避免重复成员导致权重和元数据失真。
	 */
	deduplicateTrackVec(fud->trackVec);
	if (fud->trackVec.empty())
	{
		/*
		 * P0-1：去重后为空也属于可观测删除路径。
		 */
		updateFusedTrackDebugSnapshot(fusedTrack, "fused_refresh_empty_after_dedup", true);
		sendDeletePacketForFusedTrack(fusedTrack->GetID());
		return false;
	}

	SPxPacketTrackExtended* fusedTrackExtRpt = (SPxPacketTrackExtended*)fusedTrack->GetExtRpt();
	if (!fusedTrackExtRpt)
	{
		updateFusedTrackDebugSnapshot(fusedTrack, "fused_refresh_no_ext", false);
		return false;
	}

	/*
	 * P0-B：每次输出 fused 前，按当前成员集重建 fusion 元数据，
	 * 保证 sensors / sensorTypes / trackID[] 始终与成员集合一致。
	 */
	rebuildFusionMetadataFromMembers(fusedTrack);

	SPxLatLong_t pos = { 0 };
	double speed = 0.0;
	double course = 0.0;
	computeFusedTrackPosSpeedCourse(fusedTrack, &pos, &speed, &course);

	fusedTrackExtRpt->latDegs = pos.latDegs;
	fusedTrackExtRpt->longDegs = pos.longDegs;
	fusedTrackExtRpt->norm.min.speedMps = speed;
	fusedTrackExtRpt->norm.min.courseDegrees = course;
	fusedTrackExtRpt->age++;

	/*
	 * size 字段选择策略：
	 * 1) 优先使用调用方传入的 preferredSizeTrack（仅当它仍是成员）；
	 * 2) 否则从剩余成员中选第一个可用成员。
	 */
	SPxRadarTrack* sizeTrack = NULL;
	if (preferredSizeTrack)
	{
		UINT32 preferredUnifiedID = preferredSizeTrack->GetUnifiedID();
		for (int i = 0; i < (int)fud->trackVec.size(); i++)
		{
			if (fud->trackVec[i] == preferredUnifiedID)
			{
				sizeTrack = preferredSizeTrack;
				break;
			}
		}
	}
	if (!sizeTrack)
	{
		for (int i = 0; i < (int)fud->trackVec.size(); i++)
		{
			UINT32 uniqueID = fud->trackVec[i];
			SPxRadarTrack* memberTrack = (SPxRadarTrack*)m_uniTrackDb->GetTrack(uniqueID);
			if (memberTrack)
			{
				sizeTrack = memberTrack;
				break;
			}
		}
	}
	if (sizeTrack && sizeTrack->GetMinRpt())
	{
		fusedTrackExtRpt->norm.min.sizeDegrees = sizeTrack->GetMinRpt()->sizeDegrees;
		fusedTrackExtRpt->norm.min.sizeMetres = sizeTrack->GetMinRpt()->sizeMetres;
	}

	SPxTime_t now;
	SPxTimeGetEpoch(&now);
	fusedTrack->Update(&now, &(fusedTrackExtRpt->norm.min), &(fusedTrackExtRpt->norm), fusedTrackExtRpt);

	/*
	 * 保持现有接口语义：
	 * - Update 后对外发包；
	 * - 本地 fused DB 通过 Update 已同步，不额外走 local sender。
	 */
	double azi = 0;
	double range = 0;
	CoordinateTransformation ct;
	ct.PolarCoordinates_Calculate(m_sensorFusedRadarLong, m_sensorFusedRadarLat,
		fusedTrackExtRpt->longDegs, fusedTrackExtRpt->latDegs,
		azi, range);
	fusedTrackExtRpt->norm.min.azimuthDegrees = azi;
	fusedTrackExtRpt->norm.min.rangeMetres = range;

	double xM = 0;
	double yM = 0;
	SPxLatLongDiffMetres(m_sensorFusedRadarLat, m_sensorFusedRadarLong,
		fusedTrackExtRpt->latDegs, fusedTrackExtRpt->longDegs,
		&xM, &yM);
	fusedTrackExtRpt->norm.xMetres = xM;
	fusedTrackExtRpt->norm.yMetres = yM;

	const SPxPacketTrackExtended* ext = fusedTrack->GetExtRpt();
	if (ext)
	{
		/*
		 * P0：锁内路径只入队，锁外统一发送。
		 * 该路径保持原语义：仅对外发送，不走 local sender。
		 */
		QueueOrSendTrackPacket(m_trackFusedSenderLocal, m_trackFusedSender, *ext, false, true);
	}

	/*
	 * P0-1：记录 fused 重算并发布后的最终状态。
	 */
	updateFusedTrackDebugSnapshot(fusedTrack, "fused_refresh_publish", false);

	return true;
}

/*
 * 在排除“当前 fused”后，为单源 fused 场景选择更优候选。
 * 说明：
 * - 旧逻辑只看最近 1 条 fused，若最近的正好是自己就直接返回，导致两条独立单源 fused 难以再合并。
 * - 新逻辑遍历全部 fused，跳过自己后，选择“通过门限判定且距离最小”的候选。
 */
SPxRadarTrack* OWlRadarTracksFusion::findBestFusedCandidateForTrack(
	SPxRadarTrack* track,
	UINT32 excludedFusedID,
	double* bestDistMetresRtn)
{
	if (bestDistMetresRtn)
	{
		*bestDistMetresRtn = 0.0;
	}

	if (!track || !fusedTrackDB)
	{
		return NULL;
	}

	const SPxPacketTrackExtended* trackExtRpt = track->GetExtRpt();
	if (!trackExtRpt)
	{
		return NULL;
	}

	SPxRadarTrack* bestTrack = NULL;
	double bestDistMetres = 0.0;
	bool hasBest = false;

	SPxRadarTrack* candidate = fusedTrackDB->GetNextTrack(0, TRUE, FALSE);
	while (candidate)
	{
		if (candidate->GetID() != excludedFusedID)
		{
			const SPxPacketTrackExtended* candidateExtRpt = candidate->GetExtRpt();
			if (candidateExtRpt && candidateExtRpt->norm.min.status != SPX_PACKET_TRACK_STATUS_DELETED)
			{
				double dxMetres = 0.0;
				double dyMetres = 0.0;
				SPxLatLongDiffMetres(trackExtRpt->latDegs, trackExtRpt->longDegs,
					candidateExtRpt->latDegs, candidateExtRpt->longDegs,
					&dxMetres, &dyMetres);
				double distMetres = sqrt(dxMetres * dxMetres + dyMetres * dyMetres);

				bool isValid = IsFusedCandidateValid(m_uniTrackDb, track, candidate, distMetres);
				if (isValid && fuseEngine(track, candidate))
				{
					if (!hasBest || distMetres < bestDistMetres)
					{
						bestTrack = candidate;
						bestDistMetres = distMetres;
						hasBest = true;
					}
				}
			}
		}

		candidate = fusedTrackDB->GetNextTrack(candidate->GetID(), TRUE, FALSE);
	}

	if (bestDistMetresRtn && hasBest)
	{
		*bestDistMetresRtn = bestDistMetres;
	}

	return bestTrack;
}
void OWlRadarTracksFusion::preFuse(SPxRadarTrack* track)
{
	/*
	 * P0-A 预融合确认入口。
	 * 设计要点：
	 * 1) 仅当 preFusedID 连续命中达到阈值后，才执行“正式挂接”。
	 * 2) 若雷达轨迹从旧 fused 迁移到新 fused，必须先从旧 fused 成员集中剔除，
	 *    然后立即按剩余成员重算旧 fused（或删除）。
	 * 3) 新 fused 增加成员后，也必须立即按成员集合重算，避免状态滞后。
	 */
	if (!track)
	{
		return;
	}

	RadarTrackUserData* rud = (RadarTrackUserData*)track->GetUserData();
	if (!rud)
	{
		return;
	}

	/* 预融合阶段必须存在候选 fused。 */
	if (rud->preFusedID == 0)
	{
		updateRadarTrackDebugSnapshot(track, "prefuse_no_candidate", NULL, 0.0, false, false);
		return;
	}

	SPxRadarTrack* fusedTrack = fusedTrackDB->GetTrack(rud->preFusedID);
	if (!fusedTrack)
	{
		/* 候选 fused 不存在：清空预融合状态，等待后续重新选择候选。 */
		rud->preFusedID = 0;
		rud->hitCount = 0;
		updateRadarTrackDebugSnapshot(track, "prefuse_candidate_missing", NULL, 0.0, false, false);
		return;
	}

	bool isFused = fuseEngine(track, fusedTrack);
	if (isFused)
	{
		if (rud->hitCount < 255)
		{
			rud->hitCount++;
		}
	}
	else
	{
		/* 本次判定失败：放弃当前候选。 */
		rud->preFusedID = 0;
		rud->hitCount = 0;
		updateRadarTrackDebugSnapshot(track, "prefuse_reject", fusedTrack, 0.0, true, false);
		return;
	}

	/* 命中次数不足：继续预融合。 */
	if (rud->hitCount < FUSION_HIT_COUNT_THRESH)
	{
		updateRadarTrackDebugSnapshot(track, "prefuse_wait_hit", fusedTrack, 0.0, true, true);
		return;
	}

	/*
	 * 命中阈值达成：执行正式并入。
	 * 如果原来已挂在其它 fused 上，先从旧 fused 解绑并立即重算旧 fused。
	 */
	if (rud->fusedID != 0 && rud->fusedID != fusedTrack->GetID())
	{
		char buf[1024] = { 0 };
		sprintf_s(buf, "radar track=%d  change fused track from %d to %d",
			track->GetID(), rud->fusedID, fusedTrack->GetID());
		OWlError(OWL_ERR_WARNING, buf);

		SPxRadarTrack* oldFusedTrack = fusedTrackDB->GetTrack(rud->fusedID);
		if (oldFusedTrack)
		{
			FusedTrackUserData* oldFud = (FusedTrackUserData*)oldFusedTrack->GetUserData();
			if (oldFud)
			{
				vector<UINT32> vec = oldFud->trackVec;
				RemoveUnifiedTrackFromVec(vec, track->GetUnifiedID());
				oldFud->trackVec = vec;
			}

			/*
			 * P0-A 核心：成员变化后立即重算（无成员则发送删除报文）。
			 * 这一步修复“成员增删后旧 fused 不重算”。
			 */
			refreshFusedTrackFromMembers(oldFusedTrack, NULL);
		}
	}

	/*
	 * 将雷达轨迹并入新 fused：
	 * 1) 保证 fused 有 userData；
	 * 2) trackVec 去重后再追加，避免重复成员。
	 */
	FusedTrackUserData* fud = (FusedTrackUserData*)fusedTrack->GetUserData();
	if (!fud)
	{
		fud = new FusedTrackUserData();
		fud->trackVec.clear();
		fusedTrack->SetUserData(fud);
	}

	bool exists = false;
	for (int i = 0; i < (int)fud->trackVec.size(); i++)
	{
		if (fud->trackVec[i] == track->GetUnifiedID())
		{
			exists = true;
			break;
		}
	}
	if (!exists)
	{
		fud->trackVec.push_back(track->GetUnifiedID());
	}

	/* 刷新该雷达轨迹的融合状态。 */
	rud->fusedID = fusedTrack->GetID();
	rud->preFusedID = 0;
	rud->failCount = 0;
	rud->hitCount = 0;

	/*
	 * P0-A 核心：新增成员后立即按成员集重算 fused。
	 * preferredSizeTrack 使用当前并入轨迹，确保尺寸字段有可用来源。
	 */
	refreshFusedTrackFromMembers(fusedTrack, track);

	/*
	 * P0-1：记录预融合正式提交结果。
	 */
	updateRadarTrackDebugSnapshot(track, "prefuse_commit", fusedTrack, 0.0, true, true);
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
					all_rangeMeteres += 1.0 / 100.0;
				}
				else
				{
					all_rangeMeteres += 1.0 / rangeMetres;
				}
			}
		}

		double x = 0, y = 0;
		/*
		 * P0：分母防护。
		 * 当有效成员缺失或权重和过小（<=EPS）时，禁止进入除法；
		 * 回退策略：保持输出为“当前 fused 报文中的速度/航向”，位置使用已累计值（默认 0）。
		 */
		if (all_rangeMeteres <= EPS)
		{
			if (extRpt)
			{
				*speedRet = extRpt->norm.min.speedMps;
				*courseRet = extRpt->norm.min.courseDegrees;
			}
			return;
		}
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
				if (rangeMetres < EPS)
				{
					m_weight = (1.0 / 100.0) / all_rangeMeteres;
				}
				else
				{
					m_weight = (1.0 / rangeMetres) / all_rangeMeteres;
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

		/*
		 * P0-C：修复航向计算边界错误。
		 * 旧逻辑只判断 tracky==0 就把 course 置 0，
		 * 会把“正东/正西等横向运动”误判成 0°。
		 * 正确做法是：只有速度向量近似为 0 时才置 0°，否则统一使用 atan2。
		 */
		if (fabs(trackx) < EPS && fabs(tracky) < EPS)
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
	if (!track)
	{
		return;
	}

	const SPxPacketTrackExtended* extRpt = track->GetExtRpt();
	if (!extRpt)
	{
		return;
	}

	/*
	 * fused 创建后，把“成员雷达轨迹 -> fusedID”的关系回填到雷达 userData。
	 * P0-B：按 sensors+trackID[] 映射遍历所有成员，不再只使用 trackID[0]。
	 */
	bool hasMappedMember = false;
	for (int slot = 0; slot < SPX_MAX_NUM_TRACK_IDS; slot++)
	{
		SPxRadarTrack* radarTrack = findRadarTrackByFusionSlot(extRpt, slot);
		if (!radarTrack)
		{
			continue;
		}

		hasMappedMember = true;
		RadarTrackUserData* rud = (RadarTrackUserData*)radarTrack->GetUserData();
		if (rud)
		{
			rud->fusedID = track->GetID();
		}

		updateRadarTrackDebugSnapshot(
			radarTrack,
			"fused_create_attach_member",
			track,
			0.0,
			true,
			true);
	}

	/*
	 * 兼容历史报文：旧版本 trackID[0] 可能直接存的是 UnifiedID。
	 */
	if (!hasMappedMember && extRpt->fusion.trackID[0] != 0)
	{
		SPxRadarTrack* radarTrack = (SPxRadarTrack*)m_uniTrackDb->GetTrack(extRpt->fusion.trackID[0]);
		if (radarTrack)
		{
			RadarTrackUserData* rud = (RadarTrackUserData*)radarTrack->GetUserData();
			if (rud)
			{
				rud->fusedID = track->GetID();
			}

			updateRadarTrackDebugSnapshot(
				radarTrack,
				"fused_create_attach_member_legacy",
				track,
				0.0,
				true,
				true);
		}
	}

	/*
	 * P0-1：记录 fused 创建后的初始快照。
	 */
	updateFusedTrackDebugSnapshot(track, "fused_create", false);
}



void OWlRadarTracksFusion::handleUnAssiocatedRadarTrack(SPxRadarTrack* track)
{
	if (!track)
	{
		return;
	}

	//是否处于预融合状态
	RadarTrackUserData* rud = (RadarTrackUserData*)track->GetUserData();
	if (!rud)
	{
		return;
	}

	const SPxPacketTrackExtended* extRpt = track->GetExtRpt();
	if (!extRpt)
	{
		updateRadarTrackDebugSnapshot(track, "unassoc_no_ext", NULL, 0.0, false, false);
		return;
	}

	if (rud->preFusedID != 0)
	{
		SPxRadarTrack* preFusedTrack = fusedTrackDB->GetTrack(rud->preFusedID);
		updateRadarTrackDebugSnapshot(
			track,
			"unassoc_prefuse_continue",
			preFusedTrack,
			0.0,
			(preFusedTrack != NULL),
			false);
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

		updateRadarTrackDebugSnapshot(
			track,
			(isNearestFusedTrackVaild ? (isFused ? "unassoc_candidate_hit" : "unassoc_candidate_miss") : "unassoc_candidate_invalid"),
			fusedTrack,
			disMetresRtn,
			isNearestFusedTrackVaild,
			isFused);

		//不符合关联,则直接新建单独融合航迹
		//if (!isNearestFusedTrackVaild || !isFused)
		if (!isNearestFusedTrackVaild || rud->failCount > FUSION_FIAL_COUNT_THRESH)
		{
			SPxPacketTrackExtended ext = { 0 };
			memcpy(&ext, extRpt, sizeof(SPxPacketTrackExtended));

			ext.norm.min.uniTrackType = SPxUniTrack::TRACK_TYPE_FUSED;
			ext.norm.min.senderID = OWL_FUSED_TRACK_SENDER_ID;
			/*
			 * P0-D：ID 池耗尽防护。
			 * 若 getID() 失败（返回 -1），必须立即终止创建流程，
			 * 禁止把 -1 写入无符号 id 字段导致 0xFFFFFFFF 脏值外发。
			 */
			int newFusedID = m_idPool.getID();
			if (newFusedID <= 0)
			{
				char buf[256] = { 0 };
				sprintf_s(buf,
					"P0-D: fused ID pool exhausted, skip fused create for radar track=%d",
					track->GetID());
				OWlError(OWL_ERR_ERROR, buf);

				rud->fusedID = 0;
				rud->preFusedID = 0;
				rud->hitCount = 0;

				updateRadarTrackDebugSnapshot(
					track,
					"unassoc_create_fused_id_exhausted",
					NULL,
					0.0,
					false,
					false);
				return;
			}
			ext.norm.min.id = (UINT32)newFusedID;
			ext.extMask = 0xFFFFFFFF;

			/*
			 * P0-B：按 SDK 语义初始化 fusion 元数据。
			 * - sensors: 由 sourceIndex 决定置位；
			 * - sensorTypes: 当前雷达-雷达融合场景置 PRIMARY；
			 * - trackID[0]: 填源雷达原始 track ID（非 UnifiedID）。
			 */
			ext.fusion.sensorTypes = SPX_PACKET_TRACK_SENSOR_PRIMARY;
			ext.fusion.sensors = 0;
			for (int i = 0; i < SPX_MAX_NUM_TRACK_IDS; i++)
			{
				ext.fusion.trackID[i] = 0;
			}

			int sourceIndex = track->GetSourceIndex();
			if (sourceIndex >= 0 && sourceIndex < 32)
			{
				ext.fusion.sensors = (1u << sourceIndex);
			}
			else
			{
				/* 防御性回退：sourceIndex 异常时至少保持报文字段可用。 */
				ext.fusion.sensors = SENSOR_0_FLAG;
			}
			ext.fusion.trackID[0] = track->GetID();
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

			/*
			 * P0：创建 fused 的发送改为“可延迟发送”。
			 * - 锁内：仅入队；
			 * - 锁外：统一 flush。
			 */
			bool sendOk = QueueOrSendTrackPacket(m_trackFusedSenderLocal, m_trackFusedSender, ext, true, true);
			if (!sendOk)
			{
				int zp = 1;
			}

			//单个航迹正式融合，后续等待其他航迹发起融合
			rud->fusedID = ext.norm.min.id;

			rud->failCount = 0;
			rud->hitCount = 0;
			rud->preFusedID = 0;

			updateRadarTrackDebugSnapshot(
				track,
				"unassoc_create_single_fused",
				NULL,
				0.0,
				false,
				false);
		}
	}
}
void OWlRadarTracksFusion::handleAassiocatedRadarTrack(SPxRadarTrack* track)
{
	/*
	 * 已关联雷达轨迹的维护入口。
	 * P0-A 目标：
	 * 1) 成员被剔除后，不再使用该成员刷新 fused；
	 * 2) 成员集合发生变化后，统一走 refreshFusedTrackFromMembers 重算。
	 */
	if (!track)
	{
		return;
	}

	RadarTrackUserData* rud = (RadarTrackUserData*)track->GetUserData();
	if (!rud)
	{
		return;
	}

	SPxRadarTrack* fusedTrack = fusedTrackDB->GetTrack(rud->fusedID);
	if (!fusedTrack)
	{
		char buf[1024] = { 0 };
		sprintf_s(buf, "fusedTrack=%d no exist", rud->fusedID);
		OWlError(OWL_ERR_WARNING, buf);

		updateRadarTrackDebugSnapshot(track, "assoc_fused_missing", NULL, 0.0, false, false);

		/* 融合目标已不存在，清空绑定关系。 */
		resetRadarTrackUserData(rud);
		return;
	}

	FusedTrackUserData* fud = (FusedTrackUserData*)fusedTrack->GetUserData();
	if (!fud)
	{
		/* 防御性修复：保证 fused 拥有成员容器。 */
		fud = new FusedTrackUserData();
		fud->trackVec.clear();
		fud->trackVec.push_back(track->GetUnifiedID());
		fusedTrack->SetUserData(fud);
	}

	int numSensors = (int)fud->trackVec.size();
	if (numSensors <= 1)
	{
		/*
		 * 单成员 fused：允许该轨迹寻找更优 fused（迁移逻辑在子函数内）。
		 * 子函数可能触发 preFuse 并改变 rud->fusedID，因此后续使用“最新 fusedID”再刷新。
		 */
		updateRadarTrackDebugSnapshot(track, "assoc_single_member", fusedTrack, 0.0, true, false);
		handleAassiocatedRadarTrackFusedSingle(track, fusedTrack);

		SPxRadarTrack* currentFusedTrack = fusedTrackDB->GetTrack(rud->fusedID);
		if (currentFusedTrack)
		{
			refreshFusedTrackFromMembers(currentFusedTrack, track);
		}

		updateRadarTrackDebugSnapshot(track, "assoc_single_member_done", currentFusedTrack, 0.0, (currentFusedTrack != NULL), false);
		return;
	}

	/* 多成员 fused：执行维持性门限判定。 */
	bool maintainHit = fuseEngine(track, fusedTrack, 4);
	if (!maintainHit)
	{
		rud->failCount++;

		char buf[1024] = { 0 };
		sprintf_s(buf, "fusedTrack=%d  radar track=%d  fuse fail,failCount=%d",
			fusedTrack->GetID(), track->GetID(), rud->failCount);
		OWlError(OWL_ERR_INFO, buf);
	}
	else
	{
		rud->failCount = 0;
	}

	if (rud->failCount > FUSION_FIAL_COUNT_THRESH)
	{
		/*
		 * 连续失败超过阈值：将该雷达轨迹从 fused 成员中移除。
		 * 关键修复：先从 trackVec 删除，再 reset rud，再重算 fused。
		 */
		char buf[1024] = { 0 };
		sprintf_s(buf, "fusedTrack=%d remove radar track=%d", fusedTrack->GetID(), track->GetID());
		OWlError(OWL_ERR_INFO, buf);

		vector<UINT32> vec = fud->trackVec;
		RemoveUnifiedTrackFromVec(vec, track->GetUnifiedID());
		fud->trackVec = vec;

		resetRadarTrackUserData(rud);

		/*
		 * P0-A 核心：成员被剔除后，只按“剩余成员”重算 fused，
		 * 避免再用被剔除轨迹刷新 fused（旧逻辑缺陷）。
		 */
		refreshFusedTrackFromMembers(fusedTrack, NULL);
		updateRadarTrackDebugSnapshot(track, "assoc_remove_member", fusedTrack, 0.0, true, false);
		return;
	}

	/* 正常维持：统一按成员集合重算 fused。 */
	refreshFusedTrackFromMembers(fusedTrack, track);
	updateRadarTrackDebugSnapshot(track, "assoc_refresh", fusedTrack, 0.0, true, maintainHit);
}

void OWlRadarTracksFusion::handleAassiocatedRadarTrackFusedSingle(SPxRadarTrack* track, SPxRadarTrack* fusedTrack)
{
	/*
	 * 单成员 fused 的维护逻辑。
	 * P0-A 关键修复：
	 * - 旧逻辑只看最近一个 fused，若最近的就是自己则直接返回，
	 *   会导致“两条独立单源 fused 无法再互相靠拢融合”。
	 * - 新逻辑通过 findBestFusedCandidateForTrack 遍历候选并排除当前 fused。
	 */
	if (!track || !fusedTrack)
	{
		return;
	}

	RadarTrackUserData* rud = (RadarTrackUserData*)track->GetUserData();
	if (!rud)
	{
		return;
	}

	/* 已在预融合状态：继续走 preFuse 状态机，不重复挑选候选。 */
	if (rud->preFusedID != 0)
	{
		SPxRadarTrack* preFusedTrack = fusedTrackDB->GetTrack(rud->preFusedID);
		updateRadarTrackDebugSnapshot(track, "single_prefuse_continue", preFusedTrack, 0.0, (preFusedTrack != NULL), false);
		preFuse(track);
		return;
	}

	double bestDistMetres = 0.0;
	SPxRadarTrack* bestFusedTrack = findBestFusedCandidateForTrack(
		track,
		fusedTrack->GetID(),
		&bestDistMetres);

	if (bestFusedTrack)
	{
		/*
		 * 找到更优候选：进入预融合状态。
		 * 注意：这里只“启动预融合”，真正迁移在 preFuse 命中阈值后执行。
		 */
		rud->preFusedID = bestFusedTrack->GetID();
		if (rud->hitCount < 255)
		{
			rud->hitCount++;
		}
		rud->failCount = 0;

		char buf[256] = { 0 };
		sprintf_s(buf, "single fused track=%d choose candidate fused=%d dist=%.2f",
			fusedTrack->GetID(), bestFusedTrack->GetID(), bestDistMetres);
		OWlError(OWL_ERR_INFO, buf);

		updateRadarTrackDebugSnapshot(track, "single_choose_candidate", bestFusedTrack, bestDistMetres, true, true);
	}
	else
	{
		/* 当前无可用候选：清空预融合状态，保持现有绑定。 */
		rud->preFusedID = 0;
		rud->hitCount = 0;
		rud->failCount = 0;
		updateRadarTrackDebugSnapshot(track, "single_no_candidate", NULL, 0.0, false, false);
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


void OWlRadarTracksFusion::recordFuseEvalDebugMetrics(const OWlFuseEvalDebugMetrics& metrics)
{
	/*
	 * Keep the latest fuse decision sample for both dimensions:
	 * - radarUnifiedID view (for radar track caption);
	 * - fusedID view (for fused track caption).
	 */
	if (metrics.radarUnifiedID == 0 && metrics.fusedID == 0)
	{
		return;
	}

	SPxAutoLock lock(m_debugLock);
	if (metrics.radarUnifiedID != 0)
	{
		m_lastFuseEvalByRadarUnifiedID[metrics.radarUnifiedID] = metrics;
	}
	if (metrics.fusedID != 0)
	{
		m_lastFuseEvalByFusedID[metrics.fusedID] = metrics;
	}
}

void OWlRadarTracksFusion::updateRadarTrackDebugSnapshot(
	SPxRadarTrack* track,
	const char* stage,
	SPxRadarTrack* candidateFusedTrack,
	double candidateDistMetres,
	bool candidateValid,
	bool candidateFuseHit)
{
	if (!track)
	{
		return;
	}

	OWlRadarTrackDebugSnapshot snapshot = { 0 };
	snapshot.unifiedID = track->GetUnifiedID();
	snapshot.trackID = track->GetID();
	snapshot.sourceIndex = track->GetSourceIndex();

	RadarTrackUserData* rud = (RadarTrackUserData*)track->GetUserData();
	if (rud)
	{
		snapshot.fusedID = rud->fusedID;
		snapshot.preFusedID = rud->preFusedID;
		snapshot.hitCount = rud->hitCount;
		snapshot.failCount = rud->failCount;
		snapshot.count = rud->count;
	}
	snapshot.isAssociated = (snapshot.fusedID != 0);

	if (candidateFusedTrack)
	{
		snapshot.candidateFusedID = candidateFusedTrack->GetID();
	}
	snapshot.candidateDistMetres = candidateDistMetres;
	snapshot.candidateValid = candidateValid;
	snapshot.candidateFuseHit = candidateFuseHit;
	snapshot.deferredQueueSize = (UINT32)g_deferredTrackPacketSends.size();
	SPxTimeGetEpoch(&snapshot.lastUpdateTs);

	if (stage)
	{
		strncpy_s(snapshot.stage, sizeof(snapshot.stage), stage, _TRUNCATE);
	}
	else
	{
		strncpy_s(snapshot.stage, sizeof(snapshot.stage), "unknown", _TRUNCATE);
	}

	/*
	 * Copy latest fuse-engine metrics into the radar snapshot,
	 * so UI can directly show threshold and measured errors.
	 */
	SPxAutoLock lock(m_debugLock);
	auto evalIt = m_lastFuseEvalByRadarUnifiedID.find(snapshot.unifiedID);
	if (evalIt != m_lastFuseEvalByRadarUnifiedID.end())
	{
		const OWlFuseEvalDebugMetrics& eval = evalIt->second;
		snapshot.fuseEvalAvailable = eval.hasValue;
		snapshot.fuseEvalThreshMulti = eval.threshMulti;
		snapshot.fuseEvalCfgAziObsDiffDegs = eval.cfgAziObsDiffDegs;
		snapshot.fuseEvalCfgDisObsDiffMetres = eval.cfgDisObsDiffMetres;
		snapshot.fuseEvalDxMetres = eval.dxMetres;
		snapshot.fuseEvalDyMetres = eval.dyMetres;
		snapshot.fuseEvalDiffXMetres = eval.diffXMetres;
		snapshot.fuseEvalDiffYMetres = eval.diffYMetres;
		snapshot.fuseEvalThreshXMetres = eval.threshXMetres;
		snapshot.fuseEvalThreshYMetres = eval.threshYMetres;
		snapshot.fuseEvalDiffRangeMetres = eval.diffRangeMetres;
		snapshot.fuseEvalDiffAzimuthDegs = eval.diffAzimuthDegs;
		snapshot.fuseEvalXHit = eval.isXHit;
		snapshot.fuseEvalYHit = eval.isYHit;
		snapshot.fuseEvalHit = eval.isFuseHit;
	}

	m_radarTrackDebugSnapshots[snapshot.unifiedID] = snapshot;
}

void OWlRadarTracksFusion::updateFusedTrackDebugSnapshot(
	SPxRadarTrack* fusedTrack,
	const char* stage,
	bool deletedByRefresh)
{
	if (!fusedTrack)
	{
		return;
	}

	OWlFusedTrackDebugSnapshot snapshot = { 0 };
	snapshot.fusedID = fusedTrack->GetID();
	snapshot.deletedByRefresh = deletedByRefresh;

	FusedTrackUserData* fud = (FusedTrackUserData*)fusedTrack->GetUserData();
	if (fud)
	{
		snapshot.memberCount = (UINT32)fud->trackVec.size();
	}

	const SPxPacketTrackExtended* extRpt = fusedTrack->GetExtRpt();
	if (extRpt)
	{
		snapshot.sensorsMask = extRpt->fusion.sensors;
		snapshot.sensorTypesMask = extRpt->fusion.sensorTypes;
		snapshot.speedMps = extRpt->norm.min.speedMps;
		snapshot.courseDegrees = extRpt->norm.min.courseDegrees;
	}

	snapshot.deferredQueueSize = (UINT32)g_deferredTrackPacketSends.size();
	SPxTimeGetEpoch(&snapshot.lastUpdateTs);

	if (stage)
	{
		strncpy_s(snapshot.stage, sizeof(snapshot.stage), stage, _TRUNCATE);
	}
	else
	{
		strncpy_s(snapshot.stage, sizeof(snapshot.stage), "unknown", _TRUNCATE);
	}

	/*
	 * Copy latest fuse-engine metrics mapped by fusedID.
	 */
	SPxAutoLock lock(m_debugLock);
	auto evalIt = m_lastFuseEvalByFusedID.find(snapshot.fusedID);
	if (evalIt != m_lastFuseEvalByFusedID.end())
	{
		const OWlFuseEvalDebugMetrics& eval = evalIt->second;
		snapshot.lastEvalAvailable = eval.hasValue;
		snapshot.lastEvalRadarUnifiedID = eval.radarUnifiedID;
		snapshot.lastEvalRadarTrackID = eval.radarTrackID;
		snapshot.lastEvalThreshMulti = eval.threshMulti;
		snapshot.lastEvalDiffXMetres = eval.diffXMetres;
		snapshot.lastEvalDiffYMetres = eval.diffYMetres;
		snapshot.lastEvalThreshXMetres = eval.threshXMetres;
		snapshot.lastEvalThreshYMetres = eval.threshYMetres;
		snapshot.lastEvalDiffRangeMetres = eval.diffRangeMetres;
		snapshot.lastEvalDiffAzimuthDegs = eval.diffAzimuthDegs;
		snapshot.lastEvalXHit = eval.isXHit;
		snapshot.lastEvalYHit = eval.isYHit;
		snapshot.lastEvalHit = eval.isFuseHit;
	}

	m_fusedTrackDebugSnapshots[snapshot.fusedID] = snapshot;
}

void OWlRadarTracksFusion::removeRadarTrackDebugSnapshot(UINT32 unifiedID)
{
	if (unifiedID == 0)
	{
		return;
	}

	SPxAutoLock lock(m_debugLock);
	auto it = m_radarTrackDebugSnapshots.find(unifiedID);
	if (it != m_radarTrackDebugSnapshots.end())
	{
		m_radarTrackDebugSnapshots.erase(it);
	}

	auto evalIt = m_lastFuseEvalByRadarUnifiedID.find(unifiedID);
	if (evalIt != m_lastFuseEvalByRadarUnifiedID.end())
	{
		m_lastFuseEvalByRadarUnifiedID.erase(evalIt);
	}
}

void OWlRadarTracksFusion::removeFusedTrackDebugSnapshot(UINT32 fusedID)
{
	if (fusedID == 0)
	{
		return;
	}

	SPxAutoLock lock(m_debugLock);
	auto it = m_fusedTrackDebugSnapshots.find(fusedID);
	if (it != m_fusedTrackDebugSnapshots.end())
	{
		m_fusedTrackDebugSnapshots.erase(it);
	}

	auto evalIt = m_lastFuseEvalByFusedID.find(fusedID);
	if (evalIt != m_lastFuseEvalByFusedID.end())
	{
		m_lastFuseEvalByFusedID.erase(evalIt);
	}
}

bool OWlRadarTracksFusion::GetRadarTrackDebugSnapshot(UINT32 unifiedID, OWlRadarTrackDebugSnapshot* snapshotRtn)
{
	if (unifiedID == 0 || !snapshotRtn)
	{
		return false;
	}

	SPxAutoLock lock(m_debugLock);
	auto it = m_radarTrackDebugSnapshots.find(unifiedID);
	if (it == m_radarTrackDebugSnapshots.end())
	{
		return false;
	}

	*snapshotRtn = it->second;
	return true;
}

bool OWlRadarTracksFusion::GetFusedTrackDebugSnapshot(UINT32 fusedID, OWlFusedTrackDebugSnapshot* snapshotRtn)
{
	if (fusedID == 0 || !snapshotRtn)
	{
		return false;
	}

	SPxAutoLock lock(m_debugLock);
	auto it = m_fusedTrackDebugSnapshots.find(fusedID);
	if (it == m_fusedTrackDebugSnapshots.end())
	{
		return false;
	}

	*snapshotRtn = it->second;
	return true;
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
