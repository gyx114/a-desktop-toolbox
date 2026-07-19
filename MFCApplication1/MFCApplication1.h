// MFCApplication1.h: main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "在包含此文件之前包含 'pch.h' 以生成 PCH"
#endif

#include "resource.h"		// Main symbols


// CMFCApplication1App:
// See MFCApplication1.cpp for the implementation of this class
//

class CMFCApplication1App : public CWinApp
{
public:
	CMFCApplication1App();

// Overrides
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

// Implementation
	DECLARE_MESSAGE_MAP()

private:
	ULONG_PTR m_gdiplusToken{0};
};

extern CMFCApplication1App theApp;