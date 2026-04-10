// MFCApplication1Dlg.h: 头文件
//

#pragma once

#include <vector>
#include <thread>

// CMFCApplication1Dlg 对话框
class CMFCApplication1Dlg : public CDialogEx
{
// 构造
public:
	CMFCApplication1Dlg(CWnd* pParent = nullptr);	// 标准构造函数

	virtual BOOL PreTranslateMessage(MSG* pMsg);
	void RefreshProcessList();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MFCAPPLICATION1_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnTcnSelchangeTab1(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnKillProcess();
	afx_msg void OnLocateProcess();
	afx_msg void OnBnClickedButton1();
	afx_msg void OnBnClickedButton2();
	afx_msg void OnCbnSelchangeCombo1();
   // 启动项管理
	void RefreshStartupList();
	afx_msg void OnAddStartup();
	afx_msg void OnRemoveStartup();

    // (文件管理功能已移除)

	// 文件管理: 拖放的文件路径和生成副本按钮
	afx_msg void OnDropFiles(HDROP hDropInfo);
	afx_msg void OnBnClickedButton3();
	CString m_strDroppedFilePath; // 保存拖入的文件路径



	// 开机自启动复选按钮
	afx_msg void OnBnClickedCheck1();

	// 托盘相关
	afx_msg LRESULT OnTrayNotification(WPARAM wParam, LPARAM lParam);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual void OnClose();
	afx_msg void OnDestroy();

	afx_msg void OnTrayShowWindow();
	afx_msg void OnTrayExit();
	afx_msg void OnHotKey(UINT nHotKeyId, UINT nKey1, UINT nKey2);

	NOTIFYICONDATA m_nid;
	bool m_bTrayVisible;
    bool m_bExiting;
    bool m_bMinimizeOnClose;
	static const UINT WM_TRAYICON = WM_APP + 1;

	DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnStnClickedStaticPath();
	afx_msg void OnBnClickedCheck2();
  // volume controls
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnBnClickedButton12(); // apply edit value
	afx_msg void OnBnClickedButton13(); // set 0
	afx_msg void OnBnClickedButton14(); // set 60
	afx_msg void OnBnClickedButton4();
	afx_msg void OnBnClickedButton5();
	afx_msg void OnBnClickedButton6();
	afx_msg void OnBnClickedButton7();
    afx_msg void OnBnClickedButton9();
	afx_msg void OnBnClickedButton8();
	afx_msg void OnBnClickedButton10();

	afx_msg void OnBnClickedButton11();
 afx_msg void OnBnClickedButton20();
	afx_msg void OnBnClickedButton17();
	afx_msg void OnBnClickedButton18();
	afx_msg void OnBnClickedCheck4();
	afx_msg LRESULT OnAutoClickStopped(WPARAM wParam, LPARAM lParam);

	// 剪贴板管理: 最近复制历史 (双击可重新复制)
	afx_msg LRESULT OnClipboardUpdate(WPARAM wParam, LPARAM lParam);
	afx_msg void OnNMDblclkList3(NMHDR* pNMHDR, LRESULT* pResult);

	std::vector<CString> m_clipHistory;
	static const int CLIP_HISTORY_MAX = 10;

    // custom messages for background refresh completion
	static const UINT WM_REFRESH_PROCESSES_DONE;
	static const UINT WM_REFRESH_STARTUPS_DONE;
	// custom message for async volume update
	static const UINT WM_VOLUME_UPDATED;

    struct ProcInfo { CString name; DWORD pid; CString path; SIZE_T memKB; };
	struct StartupInfo { CString name; CString cmd; };

	afx_msg LRESULT OnRefreshProcessesDone(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnRefreshStartupsDone(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedButton15();
	afx_msg void OnBnClickedButton16();
	afx_msg void OnBnClickedCheck3();
	afx_msg void OnBnClickedCheck5();

	afx_msg LRESULT OnVolumeUpdated(WPARAM wParam, LPARAM lParam);

    // Window locate / topmost controls
	afx_msg void OnBnClickedButton19(); // 开始/取消定位 或 取消当前置顶

	// Called by capture overlay when user selects a target window
	void OnTargetSelected(HWND hTarget, POINT pt);

    HWND m_hCaptureWnd;
	HWND m_hSelectedWnd;
	HWND m_hTopmostWnd;

	// UI: track whether file management tab exists (index 4)
	int m_fileTabIndex = 4;

	// Auto-clicker state
   // Auto-clicker state (legacy globals used in implementation)
	int m_autoclickIntervalMs;
	static const UINT WM_AUTOCLICK_STOPPED = WM_APP + 4;

	// Prevent automatic lock/screen-off checkbox state (IDC_CHECK5)
	bool m_bPreventLockScreen;

	// background worker thread for volume retrieval; ensure joined on destroy
	std::thread m_volumeThread;
	afx_msg void OnBnClickedButton21();
	afx_msg void OnBnClickedButton22();
    afx_msg void OnBnClickedButton23();
    afx_msg void OnBnClickedButton24();
    afx_msg void OnBnClickedButton25();
    afx_msg void OnBnClickedButton26();
    // OnBnClickedButton27 removed
	afx_msg void OnBnClickedCheck6();
	afx_msg void OnBnClickedButton27();
	afx_msg void OnBnClickedButton28();
	afx_msg void OnBnClickedButton29();
};
