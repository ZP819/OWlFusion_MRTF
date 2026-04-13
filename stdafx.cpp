// stdafx.cpp : source file that includes just the standard includes

#include "stdafx.h"


void OWlError(OWlErrorType errType, const char* msg)//是否要加入errtype
{
	SPxErrorType spxErrType;
	switch (errType)
	{
	case OWL_ERR_ERROR:
		spxErrType = SPxErrorType(SPX_ERR_USER);
		break;
	case OWL_ERR_WARNING:
		spxErrType = SPxErrorType(SPX_ERR_WARNING);
		break;
	case OWL_ERR_INFO:
		spxErrType = SPxErrorType(SPX_ERR_INFO);
		break;
	default:
		break;
	}
	SPxError(spxErrType, SPX_NO_ERROR, 0, 0, msg);

	//TRACE(CString("\n"));
	//TRACE(CString(msg));
}