#include "stdafx.h"
#include "OWlUniTrackRender.h"


void OWlConvertTimeZoneToCN(SPxTime_t* ti)
{
	ti->secs += (8 * 60 * 60);
}

namespace
{
const char* kCaptionUnknown = "未知";

void appendCaptionLine(string& txt, const char* key, const string& value)
{
	txt += key;
	txt += value;
	txt += "\n";
}

string formatDouble(const char* fmt, double val)
{
	char buf[128] = { 0 };
	sprintf_s(buf, fmt, val);
	return string(buf);
}

string formatDoublePair(const char* fmt, double v1, double v2)
{
	char buf[128] = { 0 };
	sprintf_s(buf, fmt, v1, v2);
	return string(buf);
}

string formatHex32(UINT32 val)
{
	char buf[32] = { 0 };
	sprintf_s(buf, "0x%08X", val);
	return string(buf);
}

string normalizeAisText(const char* txt)
{
	if (!txt || txt[0] == '\0')
	{
		return kCaptionUnknown;
	}

	string value = txt;
	if (value == "Unknown" || value == "Not Available")
	{
		return kCaptionUnknown;
	}

	return value;
}

string getTrackStatusText(UINT8 status)
{
	switch (status)
	{
	case SPX_PACKET_TRACK_STATUS_DELETED:
		return "删除(" + to_string(status) + ")";
	case SPX_PACKET_TRACK_STATUS_PROVISIONAL:
		return "暂态(" + to_string(status) + ")";
	case SPX_PACKET_TRACK_STATUS_ESTABLISHED:
		return "稳定(" + to_string(status) + ")";
	case SPX_PACKET_TRACK_STATUS_LOST:
		return "丢失(" + to_string(status) + ")";
	case SPX_PACKET_TRACK_STATUS_PLOT:
		return "点迹(" + to_string(status) + ")";
	default:
		return "未知(" + to_string(status) + ")";
	}
}

string getTrackValidityText(UINT8 validity)
{
	switch (validity)
	{
	case SPX_PACKET_TRACK_INVALID_NONE:
		return "有效(" + to_string(validity) + ")";
	case SPX_PACKET_TRACK_INVALID_GENERAL:
		return "无效-常规(" + to_string(validity) + ")";
	case SPX_PACKET_TRACK_INVALID_CONFIDENCE:
		return "无效-置信度(" + to_string(validity) + ")";
	case SPX_PACKET_TRACK_INVALID_LOST:
		return "无效-丢失(" + to_string(validity) + ")";
	case SPX_PACKET_TRACK_INVALID_SPEED_BAD:
		return "无效-航速异常(" + to_string(validity) + ")";
	case SPX_PACKET_TRACK_INVALID_SPEED_UNSTABLE:
		return "无效-航速不稳(" + to_string(validity) + ")";
	case SPX_PACKET_TRACK_INVALID_SPEED_LOW:
		return "无效-航速过低(" + to_string(validity) + ")";
	case SPX_PACKET_TRACK_INVALID_SPEED_HIGH:
		return "无效-航速过高(" + to_string(validity) + ")";
	case SPX_PACKET_TRACK_INVALID_SPEED_RADIAL:
		return "无效-径向航速(" + to_string(validity) + ")";
	case SPX_PACKET_TRACK_INVALID_COURSE_BAD:
		return "无效-航向异常(" + to_string(validity) + ")";
	case SPX_PACKET_TRACK_INVALID_COURSE_UNSTABLE:
		return "无效-航向不稳(" + to_string(validity) + ")";
	case SPX_PACKET_TRACK_INVALID_POSN_LOCATION:
		return "无效-位置异常(" + to_string(validity) + ")";
	case SPX_PACKET_TRACK_INVALID_POSN_RANGE:
		return "无效-超距(" + to_string(validity) + ")";
	case SPX_PACKET_TRACK_INVALID_POSN_TERRAIN:
		return "无效-地形遮挡(" + to_string(validity) + ")";
	default:
		return "未知(" + to_string(validity) + ")";
	}
}

string getSensorTypeText(UINT32 sensorType)
{
	switch (sensorType)
	{
	case SPX_PACKET_TRACK_SENSOR_PRIMARY:
		return "雷达";
	case SPX_PACKET_TRACK_SENSOR_AIS:
		return "AIS";
	case SPX_PACKET_TRACK_SENSOR_ADSB:
		return "ADSB";
	case SPX_PACKET_TRACK_SENSOR_IFF:
		return "IFF";
	case SPX_PACKET_TRACK_SENSOR_USER:
		return "USER";
	case SPX_PACKET_TRACK_SENSOR_OTHER:
		return "OTHER";
	default:
		return "未知(" + to_string(sensorType) + ")";
	}
}

void appendSensorTypeMask(string& out, UINT32 sensorTypes, UINT32 bit, const char* name)
{
	if ((sensorTypes & bit) == 0)
	{
		return;
	}

	if (!out.empty())
	{
		out += ",";
	}
	out += name;
}

string getSensorTypesMaskText(UINT32 sensorTypes)
{
	string out;
	appendSensorTypeMask(out, sensorTypes, SPX_PACKET_TRACK_SENSOR_PRIMARY, "雷达");
	appendSensorTypeMask(out, sensorTypes, SPX_PACKET_TRACK_SENSOR_AIS, "AIS");
	appendSensorTypeMask(out, sensorTypes, SPX_PACKET_TRACK_SENSOR_ADSB, "ADSB");
	appendSensorTypeMask(out, sensorTypes, SPX_PACKET_TRACK_SENSOR_IFF, "IFF");
	appendSensorTypeMask(out, sensorTypes, SPX_PACKET_TRACK_SENSOR_USER, "USER");
	appendSensorTypeMask(out, sensorTypes, SPX_PACKET_TRACK_SENSOR_OTHER, "OTHER");

	if (out.empty())
	{
		return kCaptionUnknown;
	}

	return out;
}

string getUpdateTimeText(SPxUniTrack* track)
{
	if (!track)
	{
		return kCaptionUnknown;
	}

	SPxTime_t updateTime = { 0 };
	if (track->GetUpdateTime(&updateTime) != SPX_NO_ERROR)
	{
		return kCaptionUnknown;
	}

	OWlConvertTimeZoneToCN(&updateTime);
	tm time = { 0 };
	SPxTimeGetTmFromEpoch(&time, &updateTime);

	char buf[64] = { 0 };
	sprintf_s(buf, "%04d-%02d-%02d %02d:%02d:%02d",
		time.tm_year + 1900,
		time.tm_mon + 1,
		time.tm_mday,
		time.tm_hour,
		time.tm_min,
		time.tm_sec);
	return string(buf);
}

void appendCommonTrackInfo(string& txt, SPxUniTrack* track)
{
	if (!track)
	{
		return;
	}

	appendCaptionLine(txt, "ID: ", to_string(track->GetID()));
	appendCaptionLine(txt, "统一ID: ", to_string(track->GetUnifiedID()));
	appendCaptionLine(txt, "次级ID: ", to_string(track->GetSecondaryID()));
	appendCaptionLine(txt, "次级来源: ", getSensorTypeText(track->GetSecondarySensorType()));

	int sourceIndex = track->GetSourceIndex();
	if (sourceIndex >= 0)
	{
		appendCaptionLine(txt, "站点号: ", to_string(sourceIndex + 1));
	}
	else
	{
		appendCaptionLine(txt, "站点号: ", kCaptionUnknown);
	}

	appendCaptionLine(txt, "更新时间: ", getUpdateTimeText(track));
}

void appendSpeedAndCourseInfo(string& txt,
	SPxUniTrack* track,
	const SPxPacketTrackMinimal* minRpt)
{
	double speedMps = 0.0;
	double courseDegs = 0.0;
	if (track && track->GetVelocity(&speedMps, &courseDegs) == SPX_NO_ERROR)
	{
		appendCaptionLine(txt, "航速: ", formatDouble("%.2f m/s", speedMps));
		appendCaptionLine(txt, "航向: ", formatDouble("%.2f °", courseDegs));
		return;
	}

	if (minRpt)
	{
		appendCaptionLine(txt, "航速: ", formatDouble("%.2f m/s(报文)", minRpt->speedMps));
		appendCaptionLine(txt, "航向: ", formatDouble("%.2f °(报文)", minRpt->courseDegrees));
		return;
	}

	appendCaptionLine(txt, "航速: ", kCaptionUnknown);
	appendCaptionLine(txt, "航向: ", kCaptionUnknown);
}

void appendLatLongInfo(string& txt,
	SPxUniTrack* track,
	const SPxPacketTrackExtended* extRpt)
{
	double latDegs = 0.0;
	double longDegs = 0.0;
	double ageSecs = 0.0;
	if (track && track->GetLatLong(&latDegs, &longDegs, TRUE, &ageSecs) == SPX_NO_ERROR)
	{
		appendCaptionLine(txt, "纬度: ", formatDouble("%.6f °", latDegs));
		appendCaptionLine(txt, "经度: ", formatDouble("%.6f °", longDegs));
		appendCaptionLine(txt, "位置龄期: ", formatDouble("%.2f s", ageSecs));
		return;
	}

	if (extRpt && (extRpt->extMask & SPX_PACKET_TRACK_EXT_LATLONG))
	{
		appendCaptionLine(txt, "纬度: ", formatDouble("%.6f °(扩展报文)", extRpt->latDegs));
		appendCaptionLine(txt, "经度: ", formatDouble("%.6f °(扩展报文)", extRpt->longDegs));
		return;
	}

	appendCaptionLine(txt, "纬度: ", kCaptionUnknown);
	appendCaptionLine(txt, "经度: ", kCaptionUnknown);
}

void appendRadarRptInfo(string& txt, SPxRadarTrack* track)
{
	if (!track)
	{
		return;
	}

	const SPxPacketTrackMinimal* minRpt = track->GetMinRpt();
	const SPxPacketTrackNormal* normRpt = track->GetNormRpt();
	const SPxPacketTrackExtended* extRpt = track->GetExtRpt();

	if (minRpt)
	{
		appendCaptionLine(txt, "发送器ID: ", to_string(minRpt->senderID));
		appendCaptionLine(txt, "航迹状态: ", getTrackStatusText(minRpt->status));
		appendCaptionLine(txt, "有效性: ", getTrackValidityText(minRpt->validity));
		appendCaptionLine(txt, "极坐标: ", formatDoublePair("%.2f m, %.2f °", minRpt->rangeMetres, minRpt->azimuthDegrees));
		appendCaptionLine(txt, "尺寸: ", formatDoublePair("%.2f m, %.2f °", minRpt->sizeMetres, minRpt->sizeDegrees));
		appendCaptionLine(txt, "标志位: ", formatHex32(minRpt->flags));
	}

	if (normRpt)
	{
		appendCaptionLine(txt, "平面坐标: ", formatDoublePair("%.2f m, %.2f m", normRpt->xMetres, normRpt->yMetres));
	}

	appendSpeedAndCourseInfo(txt, track, minRpt);
	appendLatLongInfo(txt, track, extRpt);

	if (extRpt)
	{
		if (extRpt->extMask & SPX_PACKET_TRACK_EXT_AGE)
		{
			appendCaptionLine(txt, "年龄(圈): ", to_string(extRpt->age));
		}
		if (extRpt->extMask & SPX_PACKET_TRACK_EXT_FUSION)
		{
			appendCaptionLine(txt, "融合传感器类型: ", getSensorTypesMaskText(extRpt->fusion.sensorTypes));
			appendCaptionLine(txt, "融合传感器位图: ", formatHex32(extRpt->fusion.sensors));
		}
	}
}

void appendFusedMembersInfo(string& txt,
	SPxUniTrackDatabase* uniDb,
	SPxRadarTrack* fusedTrack)
{
	if (!fusedTrack)
	{
		appendCaptionLine(txt, "参与航迹数: ", "0");
		return;
	}

	FusedTrackUserData* fud = (FusedTrackUserData*)fusedTrack->GetUserData();
	if (!fud)
	{
		appendCaptionLine(txt, "参与航迹数: ", "0");
		return;
	}

	const int sensorNums = (int)fud->trackVec.size();
	appendCaptionLine(txt, "参与航迹数: ", to_string(sensorNums));

	if (!uniDb)
	{
		return;
	}

	for (int i = 0; i < sensorNums; i++)
	{
		UINT32 unifiedID = fud->trackVec[i];
		SPxUniTrack* memberTrack = uniDb->GetTrack(unifiedID);
		if (memberTrack)
		{
			char buf[128] = { 0 };
			int sourceIndex = memberTrack->GetSourceIndex();
			if (sourceIndex >= 0)
			{
				sprintf_s(buf, "#%d U:%u ID:%u 站:%d", i + 1, unifiedID, memberTrack->GetID(), sourceIndex + 1);
			}
			else
			{
				sprintf_s(buf, "#%d U:%u ID:%u", i + 1, unifiedID, memberTrack->GetID());
			}
			appendCaptionLine(txt, "成员: ", buf);
		}
		else
		{
			char buf[96] = { 0 };
			sprintf_s(buf, "#%d U:%u (未找到)", i + 1, unifiedID);
			appendCaptionLine(txt, "成员: ", buf);
		}
	}
}

} // namespace


OWlUniTrackRender::OWlUniTrackRender(
	SPxUniTrackDatabase* uniDb,
	SPxUniTrackRenderer* uniTrackRender)
{
	m_uniDb = uniDb;
	m_uniTrackRender = uniTrackRender;


	SetClassName("OWlUniTrackRender");
}


OWlUniTrackRender::~OWlUniTrackRender()
{
}


SPxErrorCode OWlUniTrackRender::renderAll(void)
{
	SPxErrorCode err = SPX_NO_ERROR;
	SPxLatLong_t posLatLong = { 0 };
	int mouseX = 0;
	int mouseY = 0;
	GetMousePos(&mouseX, &mouseY);
	if (SPX_NO_ERROR == GetLatLongFromWinXY(mouseX, mouseY, &posLatLong.latDegs, &posLatLong.longDegs))
	{
		int maxClickPixels = 12;//todo:支持可设置
		int dpiSize = getDpiScaledSize(maxClickPixels);
		double vaildRangeMetres = dpiSize / GetViewPixelsPerMetre();
		double disMetresRet = 0;
		SPxUniTrack* track = m_uniDb->GetNearestTrack(posLatLong.latDegs, posLatLong.longDegs, 1, &disMetresRet);
		if (track
			&& disMetresRet < vaildRangeMetres
			&& !track->GetIsSelected())//防止重叠
		{

			renderCaption(track, mouseX, mouseY);
		}

		track = m_uniDb->GetSelectedTrack();
		if (track)
		{

			err = track->GetLatLong(&posLatLong.latDegs, &posLatLong.longDegs);
			if (err == SPX_NO_ERROR)
			{
				int isInWindow = 0;
				GetWinXYFromLatLong(posLatLong.latDegs, posLatLong.longDegs, &mouseX, &mouseY, &isInWindow);
				if (isInWindow)
				{
					renderCaption(track, mouseX, mouseY);
				}
			}
		}
	}

	return SPX_NO_ERROR;
}

void OWlUniTrackRender::renderCaption(SPxUniTrack* track, int mouseX, int mouseY)
{
	if (!m_uniTrackRender->IsTrackVisible(track))
	{
		return;//20230517 当目标不显示时，则不绘制相关标牌
	}

	SPxRendererXYD_t pos = { mouseX ,mouseY };

	string txt;
	UINT32 trackType = track->GetTrackType();
	if (trackType == SPxUniTrack::TRACK_TYPE_AIS)
	{
		txt = getAISTrackCaptionTxt((SPxAISTrack*)track);
	}
	else if (trackType == SPxUniTrack::TRACK_TYPE_RADAR)
	{
		txt = getRadarTrackCaptionTxt((SPxRadarTrack*)track);
	}
	else if (trackType == SPxUniTrack::TRACK_TYPE_FUSED)
	{
		txt = getRadarFusedTrackCaptionTxt((SPxRadarTrack*)track);
	}
	else
	{
		appendCaptionLine(txt, "类别: ", "未知");
	}

	int isSelected = track->GetIsSelected();
	UINT32 titleColor = 0xff80FF00;
	if (isSelected)
	{
		titleColor = 0xffFFD700;
	}

	drawCaption(titleColor, 0x1f0000ff, 0xffffffff, &pos, track->GetTrackName(), txt.c_str());
}



string OWlUniTrackRender::getRadarTrackCaptionTxt(SPxRadarTrack* track)
{
	string txt;
	appendCaptionLine(txt, "类别: ", "雷达目标");
	if (!track)
	{
		return txt;
	}

	appendCommonTrackInfo(txt, track);
	appendRadarRptInfo(txt, track);
	return txt;
}

string OWlUniTrackRender::getAISTrackCaptionTxt(SPxAISTrack* track)
{
	string txt;
	appendCaptionLine(txt, "类别: ", "AIS目标");
	if (!track)
	{
		return txt;
	}

	appendCommonTrackInfo(txt, track);
	appendCaptionLine(txt, "MMSI: ", to_string(track->GetMMSI()));

	UINT32 imo = 0;
	if (track->GetIMONumber(&imo) == SPX_NO_ERROR && imo > 0)
	{
		appendCaptionLine(txt, "IMO: ", to_string(imo));
	}
	else
	{
		appendCaptionLine(txt, "IMO: ", kCaptionUnknown);
	}

	appendCaptionLine(txt, "船名: ", normalizeAisText(track->GetVesselName()));
	appendCaptionLine(txt, "呼号: ", normalizeAisText(track->GetCallsign()));
	appendCaptionLine(txt, "目的地: ", normalizeAisText(track->GetDestination()));

	appendSpeedAndCourseInfo(txt, track, NULL);
	appendLatLongInfo(txt, track, NULL);

	unsigned int navStatus = 0;
	if (track->GetNavStatus(&navStatus) == SPX_NO_ERROR)
	{
		string navText = normalizeAisText(SPxAISTrack::GetStringForNavStatus(navStatus));
		navText += " (" + to_string(navStatus) + ")";
		appendCaptionLine(txt, "航行状态: ", navText);
	}
	else
	{
		appendCaptionLine(txt, "航行状态: ", kCaptionUnknown);
	}

	UINT32 shipCargoType = 0;
	if (track->GetShipCargoType(&shipCargoType) == SPX_NO_ERROR)
	{
		string shipTypeText = normalizeAisText(SPxAISTrack::GetStringForShipCargoType(shipCargoType));
		shipTypeText += " (" + to_string(shipCargoType) + ")";
		appendCaptionLine(txt, "船型: ", shipTypeText);
	}
	else
	{
		appendCaptionLine(txt, "船型: ", kCaptionUnknown);
	}

	unsigned int metresToBow = 0;
	unsigned int metresToStern = 0;
	unsigned int metresToPort = 0;
	unsigned int metresToStarboard = 0;
	if (track->GetDimensions(&metresToBow, &metresToStern, &metresToPort, &metresToStarboard) == SPX_NO_ERROR)
	{
		char buf[128] = { 0 };
		sprintf_s(buf, "%u x %u m", metresToBow + metresToStern, metresToPort + metresToStarboard);
		appendCaptionLine(txt, "尺寸(长x宽): ", buf);
	}
	else
	{
		appendCaptionLine(txt, "尺寸(长x宽): ", kCaptionUnknown);
	}

	double draughtMetres = 0.0;
	if (track->GetDraught(&draughtMetres) == SPX_NO_ERROR)
	{
		appendCaptionLine(txt, "吃水: ", formatDouble("%.2f m", draughtMetres));
	}
	else
	{
		appendCaptionLine(txt, "吃水: ", kCaptionUnknown);
	}

	return txt;
}

string OWlUniTrackRender::getFusedTrackCaptionTxt(SPxRadarTrack* track)
{
	return getRadarFusedTrackCaptionTxt(track);
}

string OWlUniTrackRender::getRadarFusedTrackCaptionTxt(SPxRadarTrack* track)
{
	string txt;
	appendCaptionLine(txt, "类别: ", "融合目标");
	if (!track)
	{
		return txt;
	}

	appendCommonTrackInfo(txt, track);
	appendRadarRptInfo(txt, track);
	appendFusedMembersInfo(txt, m_uniDb, track);

	const SPxPacketTrackExtended* extRpt = track->GetExtRpt();
	if (extRpt && (extRpt->extMask & SPX_PACKET_TRACK_EXT_FUSION))
	{
		for (int i = 0; i < SPX_MAX_NUM_TRACK_IDS; i++)
		{
			UINT32 fusedSourceTrackID = extRpt->fusion.trackID[i];
			if (fusedSourceTrackID == 0)
			{
				continue;
			}

			char buf[64] = { 0 };
			sprintf_s(buf, "#%d ID:%u", i + 1, fusedSourceTrackID);
			appendCaptionLine(txt, "融合源ID: ", buf);
		}
	}

	return txt;
}