#include "stdafx.h"
#include "EncryptHasp.h"

#include "hasp_api.h"
#include "hasp_api_cpp.h"


/* º”√‹π∑π¶ƒ‹ID */
#define HASP_GENERAL_SETTING_FUNC_ID 7581
#define HASP_PRIMARY_FUNC_ID 3205
#define HASP_PROCESS_FUNC_ID 419
#define HASP_ENHANCE_FUNC_ID 7508
#define HASP_SUPER_FUNC_ID 105

unsigned char vendorCodeOWL[] =
"k+RCJ5x1Yge9gyUgCukIVEejKhPn+vYS2F7RtgaqhFh5KaqQsenSo8QXK2xCdOtvcwnBIZO7i9ep3/9I"
"zEsaqzy8ZS1kyF/XwYfJSd6O19TJ3Sfh59ZquKJyLrtPt66W6NLrKYvI8rJHxqFgb90ArISM4yQGOoaY"
"ewORpo90iyEroEgF6ZzttIh6R6ndFiozIi/VFoU+lMCPEFSWM3BlmMixhjHehcjK8dKbOg+JIcxuZb6J"
"bn24+kKKXakeDAWS62KcyzcBorKrhXSKd5SV5CGpoHP6C/0euAJgI1sudRwN6JCzAqS+uBsxf9glJE6x"
"KLlH0E40Nik0V5Jr+ZfPxxqstFW+jO0DxKpuEzZvkK9Z5tSKTC7/Y3+b74Vh+yjk9VjMSjuYzC/4rdOV"
"yPbfbqlqj/AbPMrEklNG6tSky8z1gh6W8Eq4JtANuzqEQVLJaXfOKzI8tb7y72dcPttFvdxgqkCtVZpw"
"UolZuA7ItZgC7oTZMKtU22kXQmXnW8sX/x5x97TMpbtwwk4FxOCS7Z01zKDNygcnulHgbT4zdDw4EnsK"
"7FRhsC0rM4imeHVovIizKW7YJNyrl488YQv7FtXfXOvmk7lMHui9msRrfhWlz6EQmUux6m0zRHILUbXo"
"p4j9iTHELSW+ROR8VjnGtXplbaBFxCIkUOdBpcLz1IPFwpUa9lB4c/PBAGdWVPA2/TXEnggpErhGj3sD"
"oNGlRQq8vLL0OjPf+i3CRm8iMQzhXb9KwQTLvm56bCkWaYv4bAPGvgdSwkQQVPsqMTxRfo2WGyez1jyZ"
"uki7ofvf4WxTgvq7BbNpk/bvr99vQXBydlzuKUYSQu0qiAboNCoqBm9GF9cQBUZn3QEyh2OdlWgC9toJ"
"AsY6PGu/UturhhAiPTKypOXPVqffKLZ58jUC+KgsdlJuRPG6LfS56NadZQCBF4HxwndG/nSAd6CB+IK8"
"F/zoYPXtRq72j2Mc1kicFQ==";


EncryptHasp::EncryptHasp()
{
}


EncryptHasp::~EncryptHasp()
{
}


bool EncryptHasp::CheckHaspIsExist()
{
	ChaspFeature feature1 = ChaspFeature::fromFeature(HASP_GENERAL_SETTING_FUNC_ID);
	Chasp hasp1(feature1);
	haspStatus status1 = hasp1.login(vendorCodeOWL);

	ChaspFeature feature2 = ChaspFeature::fromFeature(HASP_PRIMARY_FUNC_ID);
	Chasp hasp2(feature2);
	haspStatus status2 = hasp2.login(vendorCodeOWL);

	ChaspFeature feature3 = ChaspFeature::fromFeature(HASP_PROCESS_FUNC_ID);
	Chasp hasp3(feature3);
	haspStatus status3 = hasp3.login(vendorCodeOWL);

	ChaspFeature feature4 = ChaspFeature::fromFeature(HASP_ENHANCE_FUNC_ID);
	Chasp hasp4(feature4);
	haspStatus status4 = hasp4.login(vendorCodeOWL);

	ChaspFeature feature5 = ChaspFeature::fromFeature(HASP_SUPER_FUNC_ID);
	Chasp hasp5(feature5);
	haspStatus status5 = hasp5.login(vendorCodeOWL);

	bool HaspIsExist = false;
	if (HASP_SUCCEEDED(status1) && HASP_SUCCEEDED(status2)
		&& HASP_SUCCEEDED(status3) && HASP_SUCCEEDED(status4) && HASP_SUCCEEDED(status5))
	{
		HaspIsExist = true;
	}

	if (HASP_SUCCEEDED(status1))
		hasp1.logout();
	if (HASP_SUCCEEDED(status2))
		hasp2.logout();
	if (HASP_SUCCEEDED(status3))
		hasp3.logout();
	if (HASP_SUCCEEDED(status4))
		hasp4.logout();
	if (HASP_SUCCEEDED(status5))
		hasp5.logout();

	return HaspIsExist;
}