#pragma once

enum FusionThreshold//总观测误差的倍数
{
	ONE = 1,
	TWO = 2,//2倍观测误差，初次被融合则门限严格
	THREE = 3,
	FOUR = 4,//4倍观测误差，已融合则门限相对宽松
	FIVE = 5,
	SIX = 6,
};

struct FuseUserParam
{
	bool isFuseActive = false;
	bool isSpeedRuleActive = false;
	bool isCourseRuleActive = false;
	//误差
	double radarAziObsDiff = 0.2; //度
	double radarDisObsDiff = 5;   //m
	double GpsDisObsDiff = 3;     //m
	FusionThreshold ThreshStart = TWO;
	FusionThreshold ThreshMaintain = FOUR;
	double StartAisExtrapTimeSecs = 60;
	double MaintainAisExtrapTimeSecs = 180;
	double startActiveRangeMetres = 50;
	double minSpeedMps = 0.5;
};

struct AISTrackParam
{
	int isFused = 0;
	UINT32 radarID = 0;
	/*int isDebug = 0;
	int isSpeedCourserVaild = 0;
	int isPosVaild = 0;

	SPxTime_t extrapTimeStamp;
	SPxLatLong_t extrapPos;*/
};



const int FUSE_HIT_TIMES_THRESH = 3;
const int COAST_MAX_NUMS = 3;

struct RadarTrackInternalParam
{
	UINT8 isFused;
	UINT8 isDeleted;
	UINT8 isDebug;
	UINT8 hitCount;//连续3次hit创建融合
	UINT8 coastCount;//连续3次则删除
	UINT32 age;//圈数
	UINT32 mmsi;
};
/*****************向BS端推送数据*********************/
//struct OWlCompletePacket
//{
//	string batch;
//	string uniqueSign;
//	UINT8 fused;
//	UINT8 fuseFlags;
//	string name;
//	string localName;
//	string aisName;
//	UINT32 mmsi;
//	string globalId;
//	float length;
//	float width;
//	float height;
//	double lon;
//	double lat;
//	float sog;
//	float cog;
//	float rot;
//	float heading;
//	UINT64 time;
//	
//};
//struct OWlCoordinatePacket
//{
//
//};


class Fusion
{
public:
	Fusion(SPxUniTrackDatabase* uniTrackDb
		, SPxTrackDatabase* trackDb
		, SPxAISDecoder* aisDecoder);
	~Fusion();

private:
	SPxUniTrackDatabase* m_uniTrackDb;
	SPxTrackDatabase* m_trackDb;
	SPxAISDecoder* m_aisDecoder;

private:
	FuseUserParam m_fuseUserParam;
	SPxPacketSender* m_trackFusedSender;
	SPxPacketSender* m_trackFusedSenderLocal;

	SPxLatLong_t m_radarPos;
	bool isSendNoFusedRadarTrack;
	SPxCriticalSection mut;
private:
	void fuse(SPxRadarTrack* radarTrack);
	void checkInternalParam(SPxRadarTrack* radarTrack);
	void checkDebug(SPxRadarTrack* radarTrack);
	void checkDeleted(SPxRadarTrack* radarTrack);
	bool isInActiveRange(SPxRadarTrack* radarTrack);
	void startFuse(SPxRadarTrack* radarTrack);
	void maintainFuse(SPxRadarTrack* radarTrack);

	void sendTrack(SPxRadarTrack* radarTrack);
public:
	static int AISHandlerFunc(void *invokingObject,
		void *userObject,
		void *arg);

	static int TrackFuseAISHandlerFunc(void *invokingObject,
		void *userObject,
		void *arg);

	static int UniTrackDBEventFunc(void *invokingObject,
		void *userObject,
		void *arg);

public:
	SPxPacketSender* GetPacketSender() {
		return m_trackFusedSender;
	}

	//参数设置接口
	void SetSpeedRuleActive(bool active) { m_fuseUserParam.isSpeedRuleActive = active; }
	bool GetSpeedRuleActive() { return m_fuseUserParam.isSpeedRuleActive; }

	void SetCourseRuleActive(bool active) { m_fuseUserParam.isCourseRuleActive = active; }
	bool GetCourseRuleActive() { return m_fuseUserParam.isCourseRuleActive; }

	void SetRadarAziObsDiff(double diff) { m_fuseUserParam.radarAziObsDiff = diff; }
	double GetRadarAziObsDiff() { return m_fuseUserParam.radarAziObsDiff; }

	void SetRadarDisObsDiff(double diff) { m_fuseUserParam.radarDisObsDiff = diff; }
	double GetRadarDisObsDiff() { return m_fuseUserParam.radarDisObsDiff; }

	void SetGpsDisObsDiff(double diff) { m_fuseUserParam.GpsDisObsDiff = diff; }
	double GetGpsDisObsDiff() { return m_fuseUserParam.GpsDisObsDiff; }

	void SetThreshStart(FusionThreshold thresh) { m_fuseUserParam.ThreshStart = thresh; }
	FusionThreshold GetThreshStart() { return m_fuseUserParam.ThreshStart; }

	void SetThreshMaintain(FusionThreshold thresh) { m_fuseUserParam.ThreshMaintain = thresh; }
	FusionThreshold GetThreshMaintain() { return m_fuseUserParam.ThreshMaintain; }
};

