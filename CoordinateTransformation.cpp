#include "stdafx.h"
#include "CoordinateTransformation.h"
#include "math.h"
#define M_PI       3.14159265358979323846


CoordinateTransformation::CoordinateTransformation()
{
	
}


CoordinateTransformation::~CoordinateTransformation()
{
	
}


//从两个点的经纬度计算相对的方位、距离、仰角，本函数的计算方法见"地球上任意两点直线距离的真值计算ver1.doc"文档
int CoordinateTransformation::PolarCoordinates_Calculate(double RadarLongitude, double RadarLatitude, double RadarHeight,
	double TargetLongitude, double TargetLatitude, double TargetHeight,
	double &TargetAzimuth, double &TargetDistance, double &TargetElevation)
{
	
	 
	//对输入参数的范围进行判断
	if (RadarLongitude > 180 || RadarLongitude < -180)
	{
		return -1;
		
	}

	if (RadarLatitude > 90 || RadarLatitude < -90)
	{
		return  -1;
	}

	if (TargetLongitude > 180 || TargetLongitude < -180)
	{
		return  -1;
	}

	if (TargetLatitude > 90 || TargetLatitude < -90)
	{
		return  -1;
	}

	//先定义地球的地理常数
	double a = 6378137.0; //地球长半径，按WGS-84坐标系，单位：米
	double b = a * (1 - 1 / 298.257223563);//  地球短半径，单位：米
	double e = sqrt((a * a - b * b) / (a * a)); // 地球偏心率



	double B0;   //雷达在空间大地坐标中的位置 纬度
	double L0;    //雷达在空间大地坐标中的位置 经度
	double H0;    //雷达在空间大地坐标中的位置 高度
	double N0;   //雷达所在位置的卯酉曲率半径
	double X0, Y0, Z0;   //雷达在空间直角坐标系中的位置

	B0 = RadarLatitude * M_PI / 180.0;   //用户设置的雷达的空间大地坐标
	L0 = RadarLongitude * M_PI / 180.0;
	H0 = RadarHeight;

	N0 = a / sqrt(1 - e * e * sin(B0) * sin(B0));

	X0 = (N0 + H0) * cos(B0) * cos(L0);     //转换到空间直角坐标
	Y0 = (N0 + H0) * cos(B0) * sin(L0);
	Z0 = (N0 * (1 - e * e) + H0) * sin(B0);


	double B1;   //目标在空间大地坐标中的位置 纬度
	double L1;    //目标在空间大地坐标中的位置 经度
	double H1;    //目标在空间大地坐标中的位置 高度
	double N1;    //目标所在位置的卯酉曲率半径
	double X1, Y1, Z1;   //目标在空间直角坐标系中的位置

	B1 = TargetLatitude * M_PI / 180.0;   //用户设置的目标的空间大地坐标
	L1 = TargetLongitude *M_PI / 180.0;
	H1 = TargetHeight;

	N1 = a / sqrt(1 - e * e * sin(B1) * sin(B1));

	X1 = (N1 + H1) * cos(B1) * cos(L1);    //转换到空间直角坐标
	Y1 = (N1 + H1) * cos(B1) * sin(L1);
	Z1 = (N1 * (1 - e * e) + H1) * sin(B1);


	double dX, dY, dZ;   //雷达与目标之间的相差
	double X, Y, Z;  //雷达所在位置的局部直角坐标
	dX = X1 - X0;
	dY = Y1 - Y0;
	dZ = Z1 - Z0;

	//进行坐标变换，将空间直角坐标系变换为以雷达所在位置的局部直角坐标系
	//转换矩阵为：见《大地测量学》第48页
	//| -Sin(B0) * Cos(L0)   -Sin(B0) * Sin(L0)   Cos(B0)  |
	//| -Sin(L0)             Cos(L0)              0        |
	//| Cos(B0) * Cos(L0)    Cos(B0) * Sin(L0)    Sin(B0)  | 
	X = (-sin(B0) * cos(L0) * dX) + (-sin(B0) * sin(L0) * dY) + (cos(B0) * dZ);
	Y = (-sin(L0) * dX) + (cos(L0) * dY);
	Z = (cos(B0) * cos(L0) * dX) + (cos(B0) * sin(L0) * dY) + (sin(B0) * dZ);

	//如果距离相差<0.001m，即1mm，则认为是0，防止计算时产生的舍入误差
	if (fabs(X) < 0.001)
	{
		X = 0;
	}
	if (fabs(Y) < 0.001)
	{
		Y = 0;
	}
	if (fabs(Z) < 0.001)
	{
		Z = 0;
	}


	TargetDistance = sqrt(X * X + Y * Y + Z * Z); //计算目标相对于雷达的直线距离（米）
	TargetAzimuth = atan2(Y, X) / M_PI * 180.0; //计算目标相对于雷达的方位（度）
	TargetElevation = atan2(Z, sqrt(X * X + Y * Y)) / M_PI * 180.0;  //计算目标相对于雷达的仰角（度）

	//下面要将方位转换成雷达坐标系中的方位
	if (TargetAzimuth < 0)
	{
		TargetAzimuth += 360;
	}

	
	return 0;


}

// 对PolarCoordinates_Calculate进行重载，雷达与目标的高度固定为0,返回方位和距离的值 （注意：由于没有高度数据，因此计算结果会有一定的误差）
int CoordinateTransformation::PolarCoordinates_Calculate(double RadarLongitude, double RadarLatitude, double TargetLongitude, double TargetLatitude, double &TargetAzimuth, double &TargetDistance)
{
	double TargetElevation = 0;  //该参数没用到
	return PolarCoordinates_Calculate(RadarLongitude, RadarLatitude, 0,	TargetLongitude, TargetLatitude, 0,	 TargetAzimuth,  TargetDistance,  TargetElevation);

}

////////////////////////////////////////////////////////////////////////////////////////////
// 从目标相对雷达的方位、距离、仰角，计算目标的经纬度，本函数的计算方法是 PolarCoordinates_Calculate 函数的逆运算
int CoordinateTransformation::GeographicCoordinates_Calculate(double RadarLongitude, double RadarLatitude, double RadarHeight,
	double TargetAzimuth, double TargetDistance, double TargetElevation,
	double &TargetLongitude, double &TargetLatitude, double &TargetHeight)
{


	//对输入参数的范围进行判断
	if (RadarLongitude > 180 || RadarLongitude < -180)
	{
		return -1;
	}

	if (RadarLatitude > 90 || RadarLatitude < -90)
	{
		return -1;
	}

	/*
	 * P0-C：修复方位输入参数合法性判断。
	 * 旧逻辑误把 RadarLongitude<0 当作方位非法条件，
	 * 导致西经区域被错误判定为失败。
	 * 这里应只校验 TargetAzimuth 是否落在 [0, 360]。
	 */
	if (TargetAzimuth > 360 || TargetAzimuth < 0)
	{
		return -1;
	}

	if (TargetDistance < 0)
	{
		return -1;
	}

	if (TargetElevation > 90 || TargetElevation < -90)
	{
		return -1;
	}

	//先定义地球的地理常数
	double a = 6378137.0; //地球长半径，按WGS-84坐标系，单位：米
	double b = a * (1 - 1 / 298.257223563);//  地球短半径，单位：米
	double e = sqrt((a * a - b * b) / (a * a)); // 地球偏心率

	double B0;  //雷达在空间大地坐标中的位置 纬度
	double L0;   //雷达在空间大地坐标中的位置 经度
	double H0;   //雷达在空间大地坐标中的位置 高度
	double N0;  //雷达所在位置的卯酉曲率半径
	double X0, Y0, Z0;   //雷达在空间直角坐标系中的位置

	B0 = RadarLatitude * M_PI / 180.0;  //用户设置的雷达的空间大地坐标
	L0 = RadarLongitude * M_PI / 180.0;
	H0 = RadarHeight;

	N0 = a / sqrt(1 - e * e * sin(B0) * sin(B0));
	X0 = (N0 + H0) * cos(B0) * cos(L0);   //转换到空间直角坐标
	Y0 = (N0 + H0) * cos(B0) * sin(L0);
	Z0 = (N0 * (1 - e * e) + H0) * sin(B0);

	//计算目标在雷达局部直角坐标系中的位置
	double X, Y, Z;   //雷达所在位置的局部直角坐标
	X = TargetDistance * cos(TargetElevation / 180.0 *M_PI) * cos(TargetAzimuth / 180.0 * M_PI);
	Y = TargetDistance * cos(TargetElevation / 180.0 * M_PI) * sin(TargetAzimuth / 180.0 *M_PI);
	Z = TargetDistance * sin(TargetElevation / 180.0 * M_PI);


	//进行坐标变换，将雷达所在位置的局部直角坐标系变换为 空间直角坐标系
	double dX, dY, dZ; //雷达与目标之间的相差
	//通过采用matalb求逆simple(inv(转换矩阵))获得的，转换矩阵的定义见《大地测量学》第48页
	//[ -sin(b0)*cos(l0),         -sin(l0),  cos(b0)*cos(l0)]
	//[ -sin(b0)*sin(l0),          cos(l0),  cos(b0)*sin(l0)]
	//[          cos(b0),                0,          sin(b0)]
	dX = -sin(B0) * cos(L0) * X - sin(L0) * Y + cos(B0) * cos(L0) * Z;
	dY = -sin(B0) * sin(L0) * X + cos(L0) * Y + cos(B0) * sin(L0) * Z;
	dZ = cos(B0) * X + sin(B0) * Z;

	//计算目标在空间直角坐标系中的位置
	double X1, Y1, Z1;   //目标在空间直角坐标系中的位置
	X1 = X0 + dX;
	Y1 = Y0 + dY;
	Z1 = Z0 + dZ;

	//空间直角坐标系转换成空间大地坐标系
	double B1;  //目标在空间大地坐标中的位置 纬度
	double L1;   //目标在空间大地坐标中的位置 经度
	double H1;   //'目标在空间大地坐标中的位置 高度
	double N1;   //目标所在位置的卯酉曲率半径


	H1 = 0; //先给高度赋一个初值
	B1 = 0; //先给纬度也赋一个初值

	//迭代计算6次，精度足够了
	N1 = a / sqrt(1 - e * e * sin(B1) * sin(B1));

	B1 = atan2(Z1 * (N1 + H1), sqrt((X1 * X1 + Y1 * Y1)) * (N1 * (1 - e * e) + H1));
	N1 = a / sqrt(1 - e * e * sin(B1) * sin(B1));
	H1 = Z1 / sin(B1) - N1 * (1 - e * e);

	B1 = atan2(Z1 * (N1 + H1), sqrt((X1 * X1 + Y1 * Y1)) * (N1 * (1 - e * e) + H1));
	N1 = a / sqrt(1 - e * e * sin(B1) * sin(B1));
	H1 = Z1 / sin(B1) - N1 * (1 - e * e);

	B1 = atan2(Z1 * (N1 + H1), sqrt((X1 * X1 + Y1 * Y1)) * (N1 * (1 - e * e) + H1));
	N1 = a / sqrt(1 - e * e * sin(B1) * sin(B1));
	H1 = Z1 / sin(B1) - N1 * (1 - e * e);

	B1 = atan2(Z1 * (N1 + H1), sqrt((X1 * X1 + Y1 * Y1)) * (N1 * (1 - e * e) + H1));
	N1 = a / sqrt(1 - e * e * sin(B1) * sin(B1));
	H1 = Z1 /sin(B1) - N1 * (1 - e * e);

	B1 = atan2(Z1 * (N1 + H1), sqrt((X1 * X1 + Y1 * Y1)) * (N1 * (1 - e * e) + H1));
	N1 = a / sqrt(1 - e * e * sin(B1) * sin(B1));
	H1 = Z1 / sin(B1) - N1 * (1 - e * e);

	B1 = atan2(Z1 * (N1 + H1), sqrt((X1 * X1 + Y1 * Y1)) * (N1 * (1 - e * e) + H1));
	N1 = a / sqrt(1 - e * e * sin(B1) * sin(B1));
	H1 = Z1 / sin(B1) - N1 * (1 - e * e);

	L1 = atan2(Y1, X1);

	//转换成以度为单位的经纬度
	TargetLatitude = B1 / M_PI * 180.0;
	TargetLongitude = L1 / M_PI * 180.0;
	TargetHeight = H1;


	return 0;

}


// 对GeographicCoordinates_Calculate进行重载，雷达高度与目标的仰角固定为0,返回目标的地理位置(注意：由于没有高度数据，因此计算结果会有一定的误差)
int CoordinateTransformation::GeographicCoordinates_Calculate(double RadarLongitude, double RadarLatitude, double TargetAzimuth, double TargetDistance, double &TargetLongitude, double &TargetLatitude)
{
	double TargetHeight = 0;  //未用

	return GeographicCoordinates_Calculate(RadarLongitude, RadarLatitude, 0, TargetAzimuth, TargetDistance, 0, TargetLongitude, TargetLatitude, TargetHeight);

}
