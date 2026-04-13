

#pragma once

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
	
	void resetRadarTrackUserData(RadarTrackUserData* rud);
public:

	void SetRadarAziObsDiff(double diff) { radarAziObserveDiffDegs = diff; }
	double GetRadarAziObsDiff() { return radarAziObserveDiffDegs; }

	void SetRadarDisObsDiff(double diff) { radarDisObserveDiffMetres = diff; }
	double GetRadarDisObsDiff() { return radarDisObserveDiffMetres; }

	SPxPacketSender* GetTrackFusedSender() { return m_trackFusedSender; }
};

///************************************************/
//#pragma once
//
///**************************************************************
// * RadarTrackUserData
// * 说明：
// *   挂在每一条“原始雷达航迹”上的用户数据，用于记录
// *   该航迹在融合过程中的状态（是否已融合、预融合命中次数等）。
// *************************************************************/
//struct RadarTrackUserData
//{
//    int    count = 0;       // 已收到的更新次数（<2 时不参与融合逻辑）
//    UINT32 fusedID = 0;     // 已正式关联的融合航迹 ID（0 表示尚未融合）
//
//    UINT32 preFusedID = 0;  // 预融合阶段的候选融合航迹 ID（0 表示当前没有候选）
//    UINT8  hitCount = 0;   // 预融合：与同一候选 fused 连续命中次数
//
//    UINT32 failCount = 0;   // 预融合 / 维护阶段：连续失败次数，用于判定“脱网”
//
//    // 若后续需要记录更多信息（如上次时间戳等），可在此处扩展字段
//};
//
///**************************************************************
// * RadarFuseParam
// * 说明：
// *   一些雷达融合相关的开关参数，目前主要保留方位/距离
// *   观测误差，便于在外部配置时使用。
// *************************************************************/
//struct RadarFuseParam
//{
//    bool   isFuseActive = false;  // 是否启用融合（当前代码中未直接使用，可扩展）
//
//    double radarAziObsDiff = 0.2;   // 雷达方位观测标准差（度）
//    double radarDisObsDiff = 5.0;   // 雷达距离观测标准差（米）
//
//    // 以下参数为历史/预留字段，如需可打开使用
//    /*
//    FusionThreshold ThreshStart = TWO;
//    FusionThreshold ThreshMaintain = FOUR;
//    double StartAisExtrapTimeSecs = 60;
//    double MaintainAisExtrapTimeSecs = 180;
//    double startActiveRangeMetres = 50;
//    double minSpeedMps = 0.5;
//    */
//};
//
///**************************************************************
// * FusionCallbackParam
// * 说明：
// *   融合算法的配置参数集合，主要用于控制门限和权重。
// *   当前代码中只用到了方位/距离观测误差，其余字段
// *   为日后扩展（速度/航向规则）预留。
// *************************************************************/
//struct FusionCallbackParam
//{
//    double radarAziObserveDifferenceVal = 0.2;   // 方位观测标准差（度）
//    double radarDisObserveDifferenceVal = 5.0;   // 距离观测标准差（米）
//
//    double speedSDVal = 1.0;   // 速度标准差（m/s），预留
//    double courseSDVal = 1.0;   // 航向标准差（度），预留
//
//    double SpeedDifferenceVal = 1.0;   // 速度差绝对门限（m/s）
//
//    int    ThreshStart = 1;     // 起始阶段命中门限（预留）
//    int    ThreshMaintain = 2;     // 维护阶段命中门限（预留）
//
//    bool   isSpeedRuleActive = false; // 是否启用速度约束（预留）
//    bool   isCourseRuleActive = false; // 是否启用航向约束（预留）
//
//    double GpsDisObserveDifferenceVal = 3.0;   // GPS 距离观测标准差（米），预留
//
//    double TrackAzimuthOffset = 0.0;   // 方位整体偏置（度），预留
//    double TrackRangeOffset = 0.0;   // 距离整体偏置（米），预留
//};
////
///**************************************************************
// * OWL_SENSOR_FLAG
// * 说明：
// *   融合航迹中参与的传感器标志位。
// *   当前仅用到 SENSOR_0_FLAG / SENSOR_1_FLAG / SENSOR_0_1_FLAG，
// *   如有更多雷达，可在此处扩展。
// *************************************************************/
//enum OWL_SENSOR_FLAG
//{
//    SENSOR_NULL_FLAG = 0,
//    SENSOR_0_FLAG = 1,
//    SENSOR_1_FLAG = 2,
//    SENSOR_0_1_FLAG = 3,    // 当前 0 和 1 两个雷达都参与
//
//    // 预留扩展示例：
//    // SENSOR_2_FLAG    = 4,
//    // SENSOR_0_1_2_FLAG= 7,
//};
//
//
//enum OWL_FUSED_TRACK_STATUS
//{
//    FUSED_TRACK_STATUS_CREATE,
//    FUSED_TRACK_STATUS_UPDATE,
//    FUSED_TRACK_STATUS_DELETE,
//
//};
//
////
////// 注意：下面用到的 SPx 相关类型（SPxObj、SPxUniTrackDatabase 等）
////// 一般在 stdafx.h 或其它 SPx 库头文件中已经引入，这里不再重复包含。
//
///**************************************************************
// * OWlRadarTracksFusion
// * 说明：
// *   负责“雷达航迹 → 融合航迹”的组网逻辑。
// *   - 输入：各雷达站的原始雷达航迹（SPxRadarTrack）
// *   - 输出：融合后的航迹（SPxRadarTrack, TRACK_TYPE_FUSED），
// *           通过 m_trackFusedSender 对外组网广播。
// *   - 不从网络接收外部融合航迹，所有 fused 均由本类创建。
// *************************************************************/
//class OWlRadarTracksFusion : public SPxObj
//{
//public:
//    OWlRadarTracksFusion(SPxUniTrackDatabase* uniTrackDb);
//    ~OWlRadarTracksFusion();
//
//    /**********************************************************
//     * 参数接口（继承自 SPxObj）
//     * 当前实现支持：
//     *   - "radarAziObsDiff"  : 方位观测标准差（度）
//     *   - "radarDisObsDiff"  : 距离观测标准差（米）
//     **********************************************************/
//    virtual int SetParameter(char* parameterName, char* parameterValue) override;
//    virtual int GetParameter(char* parameterName, char* valueBuf, int bufLen) override;
//
//public:
//    /**********************************************************
//     * UniTrack 数据库事件回调入口（静态函数）
//     * 说明：
//     *   由 SPxUniTrackDatabase 在轨迹创建 / 更新 / 删除时调用，
//     *   内部根据轨迹类型分别转给 onRadarXXX / onFusedXXX。
//     **********************************************************/
//    static int UniTrackDBEventFunc(
//        void* invokingObject,
//        void* userObject,
//        void* arg);
//
//private:
//    /******************* 成员指针与基础对象 *******************/
//    SPxUniTrackDatabase* m_uniTrackDb;     // 统一航迹数据库指针
//    FusionCallbackParam  m_fuseParam;      // 融合参数配置
//
//    // 预留：用于 GetNearestTrack 接口的临时结果数组
//    SPxUniTrackDatabase::TrackDist_t** m_tracksRtn;
//
//    SPxTrackDatabase* fusedTrackDB;     // 融合航迹所在的 TrackDB
//    CoordinateTransformation coordinateTransfor; // 坐标转换工具
//
//    SPxCriticalSection   m_lock;           // 融合逻辑互斥锁（用于多线程回调保护）
//
//    // 发送器：
//    //  - m_trackFusedSenderLocal：发至本机 127.0.0.1，将 fused 包“回灌”到 SPx；
//    //  - m_trackFusedSender     ：对外组网广播 fused 航迹。
//    SPxPacketSender* m_trackFusedSenderLocal;
//    SPxPacketSender* m_trackFusedSender;
//
//    // 融合参考雷达的位置（经纬度）
//    // 所有 fused 极坐标 / 笛卡尔坐标均以该点为原点计算。
//    double m_sensorFusedRadarLat = 0.0;
//    double m_sensorFusedRadarLong = 0.0;
//
//    // 观测误差参数（真正用于 fuseEngine）
//    double radarAziObserveDiffDegs = 0.2; // 方位观测标准差（度）
//    double radarDisObserveDiffMetres = 5.0; // 距离观测标准差（米）
//
//    /******************* 各类轨迹事件处理 *******************/
//    // 雷达轨迹创建：初始化 RadarTrackUserData
//    void onRadarTrackCreate(SPxRadarTrack* track);
//
//    // 融合轨迹创建：由本模块创建的 fused 回灌进入数据库时触发
//    void onFusedTrackCreate(SPxRadarTrack* fusedTrack);
//
//    // 雷达轨迹删除：从对应 fused 中剔除该雷达，并在必要时触发 fused 删除
//    void onRadarTrackDelete(SPxRadarTrack* track);
//
//    // 融合轨迹删除：将所有关联雷达的 fusedID 置 0，并释放 ID
//    void onFusedTrackDelete(SPxRadarTrack* fusedTrack);
//
//    // 雷达轨迹更新：核心入口，驱动整个融合流程
//    void onRadarTrackUpdate(SPxRadarTrack* track);
//
//    // 融合轨迹更新：确保 FusedTrackUserData 存在，维护参与雷达列表
//    void onFusedTrackUpdate(SPxRadarTrack* fusedTrack);
//
//    /******************* 核心融合逻辑 *******************/
//    /**********************************************************
//     * fuseEngine
//     * 说明：
//     *   根据方位/距离观测误差，在二维平面上构造椭圆门限，
//     *   判断当前雷达航迹是否在给定 fused 航迹的门限范围内。
//     *
//     * 参数：
//     *   track      - 待检验的雷达航迹
//     *   fusedTrack - 参考的融合航迹
//     *   threshMulti- 门限倍数（3 表示约 3σ，4 表示约 4σ）
//     **********************************************************/
//    bool fuseEngine(SPxRadarTrack* track,
//        SPxRadarTrack* fusedTrack,
//        UINT8          threshMulti = 3);
//
//    /**********************************************************
//     * preFuse
//     * 说明：
//     *   处理“预融合”状态（rud->preFusedID != 0）：
//     *   - fuseEngine 命中 -> hitCount++；
//     *   - 未命中       -> 清空预融合状态；
//     *   - 连续命中达到 FUSION_HIT_COUNT_THRESH -> 正式关联。
//     **********************************************************/
//    void preFuse(SPxRadarTrack* track);
//
//    /**********************************************************
//     * computeFusedTrackPosSpeedCourse
//     * 说明：
//     *   根据参与该 fused 的所有雷达航迹，按 1/距离 加权，
//     *   计算融合后航迹的经纬度 / 速度 / 航向。
//     **********************************************************/
//    void computeFusedTrackPosSpeedCourse(SPxRadarTrack* fusedTrack,
//        SPxLatLong_t* posRet,
//        double* speedRet,
//        double* courseRet);
//
//    /**********************************************************
//     * handleUnAssiocatedRadarTrack
//     * 说明：
//     *   处理尚未关联任何 fused 的雷达航迹：
//     *   - 若已有 preFusedID，则继续预融合；
//     *   - 否则寻找最近的 fused 尝试预融合；
//     *   - 找不到或连续失败太多，则创建新的 fused 航迹。
//     **********************************************************/
//    void handleUnAssiocatedRadarTrack(SPxRadarTrack* track);
//
//    /**********************************************************
//     * handleAassiocatedRadarTrack
//     * 说明：
//     *   处理已经与某个 fused 关联的雷达航迹：
//     *   - 若 fused 中只有这一个雷达源，则允许其迁移；
//     *   - 若 fused 中有多雷达源，则只做维护门限判定，
//     *     连续失败则将该雷达从 fused 中剔除。
//     **********************************************************/
//    void handleAassiocatedRadarTrack(SPxRadarTrack* track);
//
//    /**********************************************************
//     * handleAassiocatedRadarTrackFusedSingle
//     * 说明：
//     *   当前 fused 只有一个雷达源时的特殊处理：
//     *   - 允许该雷达在满足条件的情况下“换绑”到更合适的 fused；
//     *   - 通过预融合状态机完成迁移（设置 preFusedID，最终由 preFuse 确认）。
//     **********************************************************/
//    void handleAassiocatedRadarTrackFusedSingle(SPxRadarTrack* track,
//        SPxRadarTrack* fusedTrack);
//
//    /**********************************************************
//     * resetRadarTrackUserData
//     * 说明：
//     *   将某条雷达航迹的融合相关状态全部清零，
//     *   通常在“融合关系解除/失败过多”等场景下调用。
//     **********************************************************/
//    void resetRadarTrackUserData(RadarTrackUserData* rud);
//
//
//    //发送融合航迹
//    void sendFusedTrack(SPxRadarTrack* fusedTrack, OWL_FUSED_TRACK_STATUS sta
//        , SPxRadarTrack* radarTrack = NULL, UINT32* newFusedIDRet = NULL/* 新建时要传递 */);
//    //更新融合航迹内部航迹信息
//    void updateFusedTrackInfo(SPxRadarTrack* fusedTrack, SPxRadarTrack* radarTrack = NULL);
//    void checkRadarTrackUserData(SPxRadarTrack* fusedTrack);
//    void checkFusedTrackUserData(SPxRadarTrack* radarTrack);
//
//public:
//    /******************* 观测误差参数的便捷接口 *******************/
//    void   SetRadarAziObsDiff(double diff) { radarAziObserveDiffDegs = diff; }
//    double GetRadarAziObsDiff() { return radarAziObserveDiffDegs; }
//
//    void   SetRadarDisObsDiff(double diff) { radarDisObserveDiffMetres = diff; }
//    double GetRadarDisObsDiff() { return radarDisObserveDiffMetres; }
//
//    /******************* 对外广播 sender 访问接口 *******************/
//    SPxPacketSender* GetTrackFusedSender() { return m_trackFusedSender; }
//};
