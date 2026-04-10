
// MFCApplication1.cpp: 定义应用程序的类行为。
//

#include "pch.h"
#include "framework.h"
#include "MFCApplication1.h"
#include "MFCApplication1Dlg.h"
#include <afxole.h>
#ifdef _DEBUG
#include <crtdbg.h>
#endif

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CMFCApplication1App

BEGIN_MESSAGE_MAP(CMFCApplication1App, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CMFCApplication1App 构造

CMFCApplication1App::CMFCApplication1App()
{
	// 支持重新启动管理器
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

	// TODO: 在此处添加构造代码，
	// 将所有重要的初始化放置在 InitInstance 中
}


// 唯一的 CMFCApplication1App 对象

CMFCApplication1App theApp;


// CMFCApplication1App 初始化

BOOL CMFCApplication1App::InitInstance()
{
	// 如果应用程序存在以下情况，Windows XP 上需要 InitCommonControlsEx()
	// 使用 ComCtl32.dll 版本 6 或更高版本来启用可视化方式，
	//则需要 InitCommonControlsEx()。  否则，将无法创建窗口。
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// 将它设置为包括所有要在应用程序中使用的
	// 公共控件类。
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();


    AfxEnableControlContainer();

#ifdef _DEBUG
	// Enable CRT debug heap leak checking and automatic dump at program exit.
	int dbgFlag = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);
	// Enable allocation tracking but do NOT enable automatic leak dump at CRT exit here.
	// Some MFC/ATL objects are torn down by AfxOleTerm and other cleanup routines
	// and enabling automatic leak dump earlier can show false positives. We'll
	// call _CrtDumpMemoryLeaks() explicitly after doing OLE/MFC cleanup below.
	dbgFlag |= _CRTDBG_ALLOC_MEM_DF;
	_CrtSetDbgFlag(dbgFlag);
	// Optionally: write leak dump to debug output
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);

   // Debug: allocation tracking enabled; no automatic breakpoints set here.

	// Optional: allow setting a CRT allocation breakpoint via environment variable
	// Set CRT_BREAK_ALLOC to an allocation number (e.g. "258") to call
	// _CrtSetBreakAlloc at startup and break on that allocation.
	// Example in PowerShell before launching VS debug session:
	//  $env:CRT_BREAK_ALLOC = "258"
	TCHAR envBuf[64] = {0};
	DWORD r = GetEnvironmentVariable(_T("CRT_BREAK_ALLOC"), envBuf, _countof(envBuf));
	if (r > 0 && r < _countof(envBuf))
	{
		int alloc = _ttoi(envBuf);
		if (alloc > 0)
		{
			_CrtSetBreakAlloc((size_t)alloc);
		}
	}
#endif

    // Initialize OLE libraries for drag/drop and other OLE features
	if (!AfxOleInit())
	{
		AfxMessageBox(_T("初始化 OLE 失败，无法使用拖放功能。"));
		return FALSE;
	}

    // 创建 shell 管理器，以防对话框包含
	// 任何 shell 树视图控件或 shell 列表视图控件。
	CShellManager *pShellManager = new CShellManager;

    // 激活“Windows Native”视觉管理器，以便在 MFC 控件中启用主题
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	// 标准初始化
	// 如果未使用这些功能并希望减小
	// 最终可执行文件的大小，则应移除下列
	// 不需要的特定初始化例程
	// 更改用于存储设置的注册表项
	// TODO: 应适当修改该字符串，
	// 例如修改为公司或组织名
	// SetRegistryKey(_T("应用程序向导生成的本地应用程序"));

	// 获取程序当前路径，并设置 ini 文件路径
	TCHAR szAppPath[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, szAppPath, MAX_PATH);
	TCHAR* p = _tcsrchr(szAppPath, _T('\\'));
	if (p)
	{
		*p = _T('\0');
	}
	CString strIniPath;
	strIniPath.Format(_T("%s\\config.ini"), szAppPath);

	// 释放默认的 m_pszProfileName 并将其指向新的.ini文件路径
	if (m_pszProfileName != NULL)
	{
		free((void*)m_pszProfileName);
	}
	m_pszProfileName = _tcsdup(strIniPath);

    // Create and run the main dialog inside its own scope so that its
	// destructor (which destroys all child controls, including any OLE
	// related controls) runs before we call AfxOleTerm and free the
	// profile string. This prevents leaked OLE objects reported by CRT.
	{
		CMFCApplication1Dlg dlg;
		m_pMainWnd = &dlg;
		INT_PTR nResponse = dlg.DoModal();
		if (nResponse == IDOK)
		{
			// TODO: 在此放置处理何时用
			//  “确定”来关闭对话框的代码
		}
		else if (nResponse == IDCANCEL)
		{
			// TODO: 在此放置处理何时用
			//  “取消”来关闭对话框的代码
		}
		else if (nResponse == -1)
		{
			TRACE(traceAppMsg, 0, "警告: 对话框创建失败，应用程序将意外终止。\n");
			TRACE(traceAppMsg, 0, "警告: 如果您在对话框上使用 MFC 控件，则无法 #define _AFX_NO_MFC_CONTROLS_IN_DIALOGS。\n");
		}
	}

    // 删除上面创建的 shell 管理器。
	if (pShellManager != nullptr)
	{
		delete pShellManager;
		pShellManager = nullptr;
	}

#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
	ControlBarCleanUp();
#endif

	// Clean up OLE and any app-allocated profile string to avoid memory leaks reported by CRT.
	if (m_pszProfileName != NULL)
	{
		free((void*)m_pszProfileName);
		m_pszProfileName = NULL;
	}
    // Ensure OLE is properly terminated. Pass TRUE to fully clean up OLE-related
	// MFC/ATL objects that may have been created by controls (e.g. CMFCEditBrowseCtrl).
	AfxOleTerm(TRUE);

#ifdef _DEBUG
	// In debug builds explicitly dump leaks after we've terminated OLE and
	// cleaned up application objects. This avoids false-positive leak reports
	// caused by objects that are destroyed during AfxOleTerm/other cleanup.
	_CrtDumpMemoryLeaks();
#endif

	// 由于对话框已关闭，所以将返回 FALSE 以便退出应用程序，
	//  而不是启动应用程序的消息泵。
	return FALSE;
}

