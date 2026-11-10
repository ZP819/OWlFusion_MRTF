

#pragma once
#include <map>

struct RadarTrackUserData
{
	//UINT32 fusedUnifiedID = 0;
	int count = 0;
	UINT32 fusedID = 0;
	
	UINT32 preFusedID = 0;//预备关联的融合航迹ID
	UINT8 hitCount = 0;//连续3次hit加入融合

	UINT32 failCount=0;//连续3次融合失败  确认为独立融合航迹

	//todo: other param
};

struct RadarFuseParam
{
	bool isFuseActive = false;
	//误差
	double radarAziObsDiff = 0.2; //度
	double radarDisObsDiff = 5;   //m
	
	/*FusionThreshold ThreshStart = TWO;
	FusionThreshold ThreshMaintain = FOUR;
	double StartAisExtrapTimeSecs = 60;
	double MaintainAisExtrapTimeSecs = 180;
	double startActiveRangeMetres = 50;
	double minSpeedMps = 0.5;*/
};


struct FusionCallbackParam
{
	//误差
	double radarAziObserveDifferenceVal = 2; //度
	double radarDisObserveDifferenceVal = 20;   //m
	double speedSDVal = 1;   //航速的标准差,航速的标准差是1m/s
	double courseSDVal = 3;   //航向的标准差,航向的标准差是：3度+10/航速


	double SpeedDifferenceVal = 1.0;           //m/s

	int ThreshStart = 1;
	int ThreshMaintain = 2;

	bool isSpeedRuleActive = false;
	bool isCourseRuleActive = false;

	double GpsDisObserveDifferenceVal = 3;     //m

	double TrackAzimuthOffset = 0;
	double TrackRangeOffset = 0;
};

/*
 * 运行期调试快照：用于选中目标实时显示组网状态（P0-1）。
 * 说明：
 * - Radar 快照以 unifiedID 作为键；
 * - Fused 快照以 fusedID 作为键；
 * - 仅保存“最近一次”状态，保持开销可控。
 */
struct OWlRadarTrackDebugSnapshot
{
	UINT32 unifiedID = 0;
	UINT32 trackID = 0;
	int sourceIndex = -1;

	UINT32 fusedID = 0;
	UINT32 preFusedID = 0;
	UINT8 hitCount = 0;
	UINT32 failCount = 0;
	int count = 0;
	bool isAssociated = false;

	UINT32 candidateFusedID = 0;
	double candidateDistMetres = 0.0;
	bool candidateValid = false;
	bool candidateFuseHit = false;

	/*
	 * Last fuse decision details (threshold + measured errors).
	 * - fuseEvalAvailable=false: no valid sample for the current snapshot.
	 * - diff/thresh: actual X/Y error and applied X/Y threshold.
	 */
	bool fuseEvalAvailable = false;
	UINT8 fuseEvalThreshMulti = 0;
	double fuseEvalCfgAziObsDiffDegs = 0.0;
	double fuseEvalCfgDisObsDiffMetres = 0.0;
	double fuseEvalDxMetres = 0.0;
	double fuseEvalDyMetres = 0.0;
	double fuseEvalDiffXMetres = 0.0;
	double fuseEvalDiffYMetres = 0.0;
	double fuseEvalThreshXMetres = 0.0;
	double fuseEvalThreshYMetres = 0.0;
	double fuseEvalDiffRangeMetres = 0.0;
	double fuseEvalDiffAzimuthDegs = 0.0;
	bool fuseEvalXHit = false;
	bool fuseEvalYHit = false;
	bool fuseEvalHit = false;

	UINT32 deferredQueueSize = 0;
	SPxTime_t lastUpdateTs = { 0 };
	char stage[64] = { 0 };
};

struct OWlFusedTrackDebugSnapshot
{
	UINT32 fusedID = 0;
	UINT32 memberCount = 0;
	UINT32 sensorsMask = 0;
	UINT32 sensorTypesMask = 0;

	double speedMps = 0.0;
	double courseDegrees = 0.0;
	bool deletedByRefresh = false;

	/* Last radar-vs-fused fuse decision for this fused track. */
	bool lastEvalAvailable = false;
	UINT32 lastEvalRadarUnifiedID = 0;
	UINT32 lastEvalRadarTrackID = 0;
	UINT8 lastEvalThreshMulti = 0;
	double lastEvalDiffXMetres = 0.0;
	double lastEvalDiffYMetres = 0.0;
	double lastEvalThreshXMetres = 0.0;
	double lastEvalThreshYMetres = 0.0;
	double lastEvalDiffRangeMetres = 0.0;
	double lastEvalDiffAzimuthDegs = 0.0;
	bool lastEvalXHit = false;
	bool lastEvalYHit = false;
	bool lastEvalHit = false;

	UINT32 deferredQueueSize = 0;
	SPxTime_t lastUpdateTs = { 0 };
	char stage[64] = { 0 };
};

/* Internal fuse-engine metrics sample used by both radar/fused snapshots. */
struct OWlFuseEvalDebugMetrics
{
	bool hasValue = false;

	UINT32 radarUnifiedID = 0;
	UINT32 radarTrackID = 0;
	UINT32 fusedID = 0;

	UINT8 threshMulti = 0;
	double cfgAziObsDiffDegs = 0.0;
	double cfgDisObsDiffMetres = 0.0;

	double dxMetres = 0.0;
	double dyMetres = 0.0;
	double diffXMetres = 0.0;
	double diffYMetres = 0.0;
	double threshXMetres = 0.0;
	double threshYMetres = 0.0;
	double diffRangeMetres = 0.0;
	double diffAzimuthDegs = 0.0;

	bool isXHit = false;
	bool isYHit = false;
	bool isFuseHit = false;
};

enum OWL_SENSOR_FLAG
{
	SENSOR_NULL_FLAG = 0,
	SENSOR_0_FLAG = 1,
	SENSOR_1_FLAG = 2,
	SENSOR_0_1_FLAG = 3,//all
	//SENSOR_2_FLAG = 3,
	//SENSOR_0_1_2_FLAG = 3,//all
};

class OWlRadarTracksFusion :public SPxObj
{
public:
	OWlRadarTracksFusion(SPxUniTrackDatabase* uniTrackDb);
	~OWlRadarTracksFusion();


	/* Overrides for virtual functions in SPxObj base class. */
	virtual int SetParameter(char* parameterName, char* parameterValue);
	virtual int GetParameter(char* parameterName, char* valueBuf, int bufLen);

public:
	static int UniTrackDBEventFunc(void *invokingObject,
		void *userObject,
		void *arg);

private:
	SPxUniTrackDatabase* m_uniTrackDb;
	//SPxPacketSender* m_trackFusedSenderLocal;
	FusionCallbackParam m_fuseParam;

	SPxUniTrackDatabase::TrackDist_t ** m_tracksRtn;
	SPxTrackDatabase* fusedTrackDB;
	CoordinateTransformation coordinateTransfor;

	SPxCriticalSection m_lock;

	SPxCriticalSection m_debugLock;
	std::map<UINT32, OWlRadarTrackDebugSnapshot> m_radarTrackDebugSnapshots;
	std::map<UINT32, OWlFusedTrackDebugSnapshot> m_fusedTrackDebugSnapshots;
	std::map<UINT32, OWlFuseEvalDebugMetrics> m_lastFuseEvalByRadarUnifiedID;
	std::map<UINT32, OWlFuseEvalDebugMetrics> m_lastFuseEvalByFusedID;

	SPxPacketSender* m_trackFusedSenderLocal;
	SPxPacketSender* m_trackFusedSender;

	//融合雷达传感器的经纬度，需要将其他雷达站目标的距离方位进行换算
	double m_sensorFusedRadarLat = 0;
	double m_sensorFusedRadarLong = 0;

	double radarAziObserveDiffDegs = 0.2;// 0.2;//todo:参数从外部传入
	double radarDisObserveDiffMetres = 5;//5;  脉宽0.2微妙

	void onRadarTrackCreate(SPxRadarTrack* track);
	void onFusedTrackCreate(SPxRadarTrack* fusedTrack);

	void onRadarTrackDelete(SPxRadarTrack* track);
	void onFusedTrackDelete(SPxRadarTrack* fusedTrack);

	void onRadarTrackUpdate(SPxRadarTrack* track);
	void onFusedTrackUpdate(SPxRadarTrack* fusedTrack);

	
	/* 返回融合结果 */
	bool fuseEngine(SPxRadarTrack* track, SPxRadarTrack* fusedTrack, UINT8 threshMulti =3);
	/* 预融合 */
	void preFuse(SPxRadarTrack* track);

	void computeFusedTrackPosSpeedCourse(SPxRadarTrack* fusedTrack
		,SPxLatLong_t* posRet,double* speedRet,double* courseRet);

	void handleUnAssiocatedRadarTrack(SPxRadarTrack* track);
	void handleAassiocatedRadarTrack(SPxRadarTrack* track);
	//雷达航迹对应融合航迹为独立融合航迹
	void handleAassiocatedRadarTrackFusedSingle(SPxRadarTrack* track, SPxRadarTrack* fusedTrack);
	/*
	 * 仅用于 P0-A 止血补丁的内部辅助函数（不改变外部接口）：
	 * 1) findBestFusedCandidateForTrack:
	 *    在“排除当前 fused”的前提下，从 fusedDB 中选择最优候选，修复“单源 fused 后无法再合并”。
	 * 2) refreshFusedTrackFromMembers:
	 *    当成员集合发生变化后，强制按剩余成员重算 fused 状态，修复“剔除/切换后状态不一致”。
	 * 3) sendDeletePacketForFusedTrack:
	 *    统一删除报文发送路径，避免分支散落导致行为不一致。
	 */
	SPxRadarTrack* findBestFusedCandidateForTrack(
		SPxRadarTrack* track,
		UINT32 excludedFusedID,
		double* bestDistMetresRtn);

	bool refreshFusedTrackFromMembers(
		SPxRadarTrack* fusedTrack,
		SPxRadarTrack* preferredSizeTrack);

	void sendDeletePacketForFusedTrack(UINT32 fusedID);

	/*
	 * P0-B：融合元数据与成员列表一致性维护。
	 * 1) deduplicateTrackVec:
	 *    对成员统一 ID 列表做稳定去重，避免重复成员污染融合状态。
	 * 2) rebuildFusionMetadataFromMembers:
	 *    按 SDK 语义从成员重建 fusion.sensors / fusion.sensorTypes / fusion.trackID[]。
	 * 3) findRadarTrackByFusionSlot:
	 *    根据 fusion.sensors 与 trackID[] 的映射规则，反查对应雷达航迹。
	 */
	void deduplicateTrackVec(vector<UINT32>& trackVec);
	void rebuildFusionMetadataFromMembers(SPxRadarTrack* fusedTrack);
	SPxRadarTrack* findRadarTrackByFusionSlot(const SPxPacketTrackExtended* extRpt, int slotIndex);
	
	void resetRadarTrackUserData(RadarTrackUserData* rud);

	/* P0-1：调试快照维护（供渲染标牌读取）。 */
	void recordFuseEvalDebugMetrics(const OWlFuseEvalDebugMetrics& metrics);
	void updateRadarTrackDebugSnapshot(
		SPxRadarTrack* track,
		const char* stage,
		SPxRadarTrack* candidateFusedTrack,
		double candidateDistMetres,
		bool candidateValid,
		bool candidateFuseHit);
	void updateFusedTrackDebugSnapshot(
		SPxRadarTrack* fusedTrack,
		const char* stage,
		bool deletedByRefresh);
	void removeRadarTrackDebugSnapshot(UINT32 unifiedID);
	void removeFusedTrackDebugSnapshot(UINT32 fusedID);
public:

	void SetRadarAziObsDiff(double diff) { radarAziObserveDiffDegs = diff; }
	double GetRadarAziObsDiff() { return radarAziObserveDiffDegs; }

	void SetRadarDisObsDiff(double diff) { radarDisObserveDiffMetres = diff; }
	double GetRadarDisObsDiff() { return radarDisObserveDiffMetres; }


	/* P0-1：对外只读调试快照查询接口（用于选中目标实时标牌）。 */
	bool GetRadarTrackDebugSnapshot(UINT32 unifiedID, OWlRadarTrackDebugSnapshot* snapshotRtn);
	bool GetFusedTrackDebugSnapshot(UINT32 fusedID, OWlFusedTrackDebugSnapshot* snapshotRtn);
	SPxPacketSender* GetTrackFusedSender() { return m_trackFusedSender; }
};
