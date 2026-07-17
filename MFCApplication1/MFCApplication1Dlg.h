// MFCApplication1Dlg.h: 头文件
//

#pragma once

#include <vector>
#include <thread>
#include <stop_token>
#include <string>
#include <memory>
#include "AutoClicker.h"

// 按钮语义化别名
#define IDC_BTN_SHUTDOWN       IDC_BUTTON1   // 关机/重启
#define IDC_BTN_CANCEL_SHUTDOWN IDC_BUTTON2  // 取消关机
#define IDC_BTN_MAKE_COPY      IDC_BUTTON3   // 生成副本
#define IDC_BTN_WECHAT         IDC_BUTTON4   // 启动微信
#define IDC_BTN_QQ             IDC_BUTTON5   // 启动QQ
#define IDC_BTN_VSCODE         IDC_BUTTON6   // 启动VS Code
#define IDC_BTN_VS             IDC_BUTTON7   // 启动Visual Studio
#define IDC_BTN_BILIBILI       IDC_BUTTON8   // 启动哔哩哔哩
#define IDC_BTN_STUDY          IDC_BUTTON9   // 打开学习文件夹
#define IDC_BTN_LOCAL_SERVER   IDC_BUTTON10  // 打开本地服务器
#define IDC_BTN_MOOC           IDC_BUTTON11  // 打开中国大学MOOC
#define IDC_BTN_VOLUME_APPLY   IDC_BUTTON12  // 应用音量
#define IDC_BTN_VOLUME_0       IDC_BUTTON13  // 音量设为0
#define IDC_BTN_VOLUME_10      IDC_BUTTON14  // 音量设为10
#define IDC_BTN_RUN_CMD        IDC_BUTTON17  // 运行命令
#define IDC_BTN_CLEAR_CMD      IDC_BUTTON18  // 清空命令
#define IDC_BTN_LOCATE         IDC_BUTTON19  // 窗口定位/置顶
#define IDC_BTN_TASK_MGR       IDC_BUTTON20  // 打开任务管理器
#define IDC_BTN_DOWNLOAD       IDC_BUTTON21  // 打开下载文件夹
#define IDC_BTN_YUANBAO        IDC_BUTTON22  // 启动元宝
#define IDC_BTN_RENAME_FILE    IDC_BUTTON23  // 重命名文件
#define IDC_BTN_DELETE_FILE    IDC_BUTTON24  // 删除文件
#define IDC_BTN_OPEN_FOLDER    IDC_BUTTON25  // 打开文件夹
#define IDC_BTN_COPY_FILE      IDC_BUTTON26  // 复制文件
#define IDC_BTN_POWERSHELL     IDC_BUTTON27  // 启动PowerShell
#define IDC_BTN_WSL            IDC_BUTTON28  // 启动WSL
#define IDC_BTN_LEETCODE       IDC_BUTTON29  // 打开LeetCode
#define IDC_BTN_GITHUB         IDC_BUTTON30  // 打开GitHub
#define IDC_BTN_GIT_BASH       IDC_BUTTON31  // 启动Git Bash
#define IDC_BTN_CLEAR_PATH     IDC_BUTTON32  // 清除拖入路径
#define IDC_BTN_BILI_NEXT      IDC_BUTTON33  // 哔哩哔哩下一首

// CMFCApplication1Dlg 对话框
class CMFCApplication1Dlg : public CDialogEx
{
// 构造
public:
	CMFCApplication1Dlg(CWnd* pParent = nullptr);	// 标准构造函数

	virtual BOOL PreTranslateMessage(MSG* pMsg);
	[[nodiscard]] void RefreshProcessList();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_MFCAPPLICATION1_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 初始化辅助函数
	void InitTabControl();
	void InitProcessTab();
	void InitStartupTab();
	void InitClipboardTab();
	void InitWindowTab();
	void InitFileTab();
	void InitGitTab();
	void UpdateTabVisibility(int nTab);

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

	NOTIFYICONDATA m_nid{};
	bool m_bTrayVisible{false};
    bool m_bExiting{false};
    bool m_bMinimizeOnClose{true};
	static constexpr UINT WM_TRAYICON = WM_APP + 1;

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
	static constexpr int CLIP_HISTORY_MAX = 10;

    // custom messages for background refresh completion
	static constexpr UINT WM_REFRESH_PROCESSES_DONE = WM_APP + 2;
	static constexpr UINT WM_REFRESH_STARTUPS_DONE = WM_APP + 3;
	// custom message for async volume update
	static constexpr UINT WM_VOLUME_UPDATED = WM_APP + 5;

    struct ProcInfo { CString name; DWORD pid; CString path; SIZE_T memKB; };
	struct StartupInfo { CString name; CString cmd; };

	afx_msg LRESULT OnRefreshProcessesDone(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnRefreshStartupsDone(WPARAM wParam, LPARAM lParam);
	afx_msg void OnBnClickedCheck3();
	afx_msg void OnBnClickedCheck5();

	afx_msg LRESULT OnVolumeUpdated(WPARAM wParam, LPARAM lParam);

    // Window locate / topmost controls
	afx_msg void OnBnClickedButton19(); // 开始/取消定位 或 取消当前置顶

	// Called by capture overlay when user selects a target window
	void OnTargetSelected(HWND hTarget, POINT pt);

    HWND m_hCaptureWnd{nullptr};
	HWND m_hSelectedWnd{nullptr};
	std::vector<HWND> m_topmostWnds;       // 置顶窗口列表
	std::vector<HWND> m_historyWnds;       // 历史定位窗口列表

	// 窗口处理新功能: 透明度、强制结束、截图
	afx_msg void OnForceKillProcess();
	afx_msg void OnWindowScreenshot();

	// 菜单命令处理函数
	afx_msg void OnViewProcess();
	afx_msg void OnViewStartup();
	afx_msg void OnViewClipboard();
	afx_msg void OnViewWindow();
	afx_msg void OnViewFile();
	afx_msg void OnViewGit();
	afx_msg void OnWindowLocate();
	afx_msg void OnWindowUntopmost();
	afx_msg void OnWindowClose();
	afx_msg void OnUntopmostWindow();   // LIST6 右键取消置顶
	afx_msg void OnCopyStartupPath();     // LIST2 右键复制路径
	afx_msg void OnNMDblclkList2(NMHDR* pNMHDR, LRESULT* pResult);  // LIST2 双击复制
	afx_msg void OnClickList6(NMHDR* pNMHDR, LRESULT* pResult);       // LIST6 单击加载详情
	afx_msg void OnClickList7(NMHDR* pNMHDR, LRESULT* pResult);       // LIST7 单击加载详情
	afx_msg void OnDeleteList6Record();                                // LIST6 右键删除
	afx_msg void OnDeleteList7Record();                                // LIST7 右键删除
	afx_msg void OnTopmostFromHistory();                               // LIST7 右键置顶
	afx_msg void OnUntopmostFromHistory();                             // LIST7 右键取消置顶
	afx_msg void OnHelpShortcuts();
	afx_msg void OnHelpGithub();

	// UI: track whether file management tab exists (index 4)
	int m_fileTabIndex = 4;

	// Auto-clicker state (C++20: encapsulated in CAutoClicker class)
	CAutoClicker m_autoClicker;
	static constexpr UINT WM_AUTOCLICK_STOPPED = CAutoClicker::WM_STOPPED;
	static constexpr UINT WM_SPEED_DLG_CLOSED = WM_APP + 6;

	// 连点器速度调节窗口（置顶）
	CDialogEx* m_pSpeedDlg{nullptr};

	// 速度窗口关闭回调
	afx_msg LRESULT OnSpeedDlgClosed(WPARAM wParam, LPARAM lParam);

	// Prevent automatic lock/screen-off checkbox state (IDC_CHECK5)
	bool m_bPreventLockScreen{false};

	// 将窗口详细信息加载到 LIST5 中
	void LoadWindowDetailToList5(HWND hWnd);

	// background worker thread for volume retrieval; ensure joined on destroy
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
  afx_msg void OnBnClickedButton30();
	afx_msg void OnBnClickedButton31();
    afx_msg void OnBnClickedButton32();
    afx_msg void OnBiliNext();
    // Git list handlers
	afx_msg void OnCopyGitCommand();
	afx_msg void OnNMDblclkList4(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnLbnDblclkList4();
    afx_msg void OnNMDblclkList5(NMHDR* pNMHDR, LRESULT* pResult);
    afx_msg void OnFileSettings();
    afx_msg void OnFileExit();
    afx_msg void OnHelpAbout();
};