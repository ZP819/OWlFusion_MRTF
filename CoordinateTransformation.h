///////////////////////////////////////////////////////////////////////////
//功能：根据GPS坐标，计算两点的相对位置
//开发人: CJJ
//开发时间：2018年4月9日
//说明：这是在CoordinateTransformation.cs 程序的基础上，将其采用C++类库来实现


#pragma once

//根据GPS坐标，计算两点的相对位置的类
class CoordinateTransformation
{
public:
	CoordinateTransformation();
	~CoordinateTransformation();

	////////////////////////////
public:
	//从两个点的经纬度计算相对的方位、距离、仰角，本函数的计算方法见"地球上任意两点直线距离的真值计算ver1.doc"文档
	int PolarCoordinates_Calculate(double RadarLongitude, double RadarLatitude, double RadarHeight,
		double TargetLongitude, double TargetLatitude, double TargetHeight,
		double &TargetAzimuth, double &TargetDistance, double &TargetElevation);

	// 对PolarCoordinates_Calculate进行重载，雷达与目标的高度固定为0,返回方位和距离的值 （注意：由于没有高度数据，因此计算结果会有一定的误差）
	int PolarCoordinates_Calculate(double RadarLongitude, double RadarLatitude, double TargetLongitude, double TargetLatitude, double &TargetAzimuth, double &TargetDistance);

	////////////////////////////
	// 从目标相对雷达的方位、距离、仰角，计算目标的经纬度，本函数的计算方法是 PolarCoordinates_Calculate 函数的逆运算
	int GeographicCoordinates_Calculate(double RadarLongitude, double RadarLatitude, double RadarHeight,
		double TargetAzimuth, double TargetDistance, double TargetElevation,
		double &TargetLongitude, double &TargetLatitude, double &TargetHeight);

	// 对GeographicCoordinates_Calculate进行重载，雷达高度与目标的仰角固定为0,返回目标的地理位置(注意：由于没有高度数据，因此计算结果会有一定的误差)
	int GeographicCoordinates_Calculate(double RadarLongitude, double RadarLatitude, double TargetAzimuth, double TargetDistance, double &TargetLongitude, double &TargetLatitude);
};

