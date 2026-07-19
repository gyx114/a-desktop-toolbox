#include "pch.h"
#include "framework.h"
#include "MFCApplication1Dlg.h"
#include "resource.h"
#include "Utils.h"
#include <TlHelp32.h>
#include <Shellapi.h>
#include <Psapi.h>
#include <regex>
#include <algorithm>

// Forward declarations for static helper functions
static UINT EnumProcessesThread(LPVOID pParam);
static BOOL CALLBACK EnumWindowsCloseCallback(HWND hWnd, LPARAM lParam);

// Double-click handler for list3: copy selected item back to clipboard
void CMFCApplication1Dlg::OnNMDblclkList3(NMHDR* pNMHDR, LRESULT* pResult)
{
    LPNMITEMACTIVATE pItem = (LPNMITEMACTIVATE)pNMHDR;
    int nItem = pItem->iItem;
    if (nItem >= 0 && nItem < (int)m_clipHistory.size())
    {
        CString text = m_clipHistory[nItem];
        if (!text.IsEmpty())
        {
            if (::OpenClipboard(m_hWnd))
            {
                ::EmptyClipboard();
                int len = (text.GetLength() + 1);
                HGLOBAL hGlob = ::GlobalAlloc(GMEM_MOVEABLE, len * sizeof(WCHAR));
                if (hGlob)
                {
                    LPWSTR pBuf = (LPWSTR)::GlobalLock(hGlob);
                    if (pBuf)
                    {
                        wcscpy_s(pBuf, len, text);
                        ::GlobalUnlock(hGlob);
                        HGLOBAL hSet = ::SetClipboardData(CF_UNICODETEXT, hGlob);
                        if (hSet == NULL)
                        {
                            // SetClipboardData failed; free the allocation to avoid leak
                            ::GlobalFree(hGlob);
                        }
                    }
                    else
                    {
                        ::GlobalFree(hGlob);
                    }
                }
                ::CloseClipboard();
            }
        }
    }
    *pResult = 0;
}

void CMFCApplication1Dlg::RefreshProcessList()
{
    // Start background thread to enumerate processes
    AfxBeginThread(EnumProcessesThread, this);
}

void CMFCApplication1Dlg::OnKillProcess()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST1);
    if (!pList) return;

    int idx = pList->GetNextItem(-1, LVNI_SELECTED);
    if (idx == -1) return;

    CString procName = pList->GetItemText(idx, 0);
    CString pidStr = pList->GetItemText(idx, 1);
    DWORD dwPID = _ttoi(pidStr);

    CString strMsg;
    strMsg.Format(_T("确定要结束进程\n%s (PID: %s) 吗？"), procName, pidStr);
    if (MessageBox(strMsg, _T("确认结束进程"), MB_YESNO | MB_ICONWARNING) != IDYES) return;

    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, dwPID);
    if (hProcess)
    {
        // try to gracefully close: enumerate windows and send WM_CLOSE
        EnumWindows(EnumWindowsCloseCallback, (LPARAM)dwPID);

        // give it a short moment
        Sleep(200);

        if (TerminateProcess(hProcess, 0))
        {
            CloseHandle(hProcess);
            MessageBox(_T("进程已成功结束。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
            RefreshProcessList();
        }
        else
        {
            DWORD err = GetLastError();
            CString msg;
            msg.Format(_T("结束进程失败：%s"), FormatLastError(err));
            MessageBox(msg, _T("错误"), MB_OK | MB_ICONERROR);
        }
    }
    else
    {
        DWORD err = GetLastError();
        CString msg;
        msg.Format(_T("无法打开进程以终止。错误：%s"), FormatLastError(err));
        // if access denied, prompt for elevation
        if (err == ERROR_ACCESS_DENIED)
        {
            if (PromptRestartElevated())
                ::PostMessage(this->GetSafeHwnd(), WM_NULL, 0, 0);
        }
        MessageBox(msg, _T("权限不足"), MB_OK | MB_ICONERROR);
    }
}

void CMFCApplication1Dlg::OnBnClickedButton20()
{
    // Try to launch Task Manager. ShellExecute usually finds it in PATH/System32.
    HINSTANCE h = ::ShellExecute(m_hWnd, _T("open"), _T("taskmgr.exe"), NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)h <= 32)
    {
        // Fallback: try explicit system directory path
        TCHAR sysPath[MAX_PATH] = {0};
        if (GetSystemDirectory(sysPath, MAX_PATH) > 0)
        {
            CString full;
            full.Format(_T("%s\\taskmgr.exe"), sysPath);
            HINSTANCE h2 = ::ShellExecute(m_hWnd, _T("open"), full, NULL, NULL, SW_SHOWNORMAL);
            if ((INT_PTR)h2 <= 32)
            {
                MessageBox(_T("无法启动任务管理器，请手动运行 taskmgr.exe"), _T("错误"), MB_OK | MB_ICONERROR);
            }
        }
        else
        {
            MessageBox(_T("无法启动任务管理器，请手动运行 taskmgr.exe"), _T("错误"), MB_OK | MB_ICONERROR);
        }
    }
}

void CMFCApplication1Dlg::OnLocateProcess()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST1);
    if (!pList) return;

    int idx = pList->GetNextItem(-1, LVNI_SELECTED);
    if (idx == -1) return;

    // 路径在第3列 (index 为 2)
    CString path = pList->GetItemText(idx, 2);
    if (path.IsEmpty())
    {
        MessageBox(_T("无法获取该进程的路径或路径为空。"), _T("提示"), MB_OK | MB_ICONWARNING);
        return;
    }

    if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
    {
        CString args;
        args.Format(_T("/select,\"%s\""), path);
        ::ShellExecute(NULL, _T("open"), _T("explorer.exe"), args, NULL, SW_SHOWNORMAL);
    }
    else
    {
        MessageBox(_T("找不到该文件，可能无权访问或进程已退出。"), _T("提示"), MB_OK | MB_ICONWARNING);
    }
}

static UINT EnumProcessesThread(LPVOID pParam)
{
    auto dlg = reinterpret_cast<CMFCApplication1Dlg*>(pParam);
    std::vector<CMFCApplication1Dlg::ProcInfo>* results = new std::vector<CMFCApplication1Dlg::ProcInfo>();

    // Try to enable SeDebugPrivilege so we can query protected processes when running elevated
    auto EnableDebugPrivilege = []() {
        HANDLE hToken = NULL;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
            return false;

        TOKEN_PRIVILEGES tp = {0};
        LUID luid;
        if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &luid))
        {
            CloseHandle(hToken);
            return false;
        }

        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        BOOL ok = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);
        CloseHandle(hToken);
        return (ok && GetLastError() == ERROR_SUCCESS);
    };

    // best-effort enable
    EnableDebugPrivilege();

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(pe);
        if (Process32First(hSnap, &pe))
        {
            do
            {
                CMFCApplication1Dlg::ProcInfo pi;
                pi.name = pe.szExeFile;
                pi.pid = pe.th32ProcessID;
                pi.path = _T("");
                pi.memKB = 0;

                // Try to obtain process image path. Use QueryFullProcessImageName when possible,
                // otherwise fall back to module snapshot which often yields the path.
                TCHAR bufPath[MAX_PATH] = {0};

                HANDLE hProcPath = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pi.pid);
                if (hProcPath)
                {
                    DWORD size = _countof(bufPath);
                    if (QueryFullProcessImageName(hProcPath, 0, bufPath, &size))
                        pi.path = bufPath;
                    CloseHandle(hProcPath);
                }

                if (pi.path.IsEmpty())
                {
                    // Fallback: enumerate modules to get the executable path
                    HANDLE hModSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pi.pid);
                    if (hModSnap != INVALID_HANDLE_VALUE)
                    {
                        MODULEENTRY32 me;
                        me.dwSize = sizeof(me);
                        if (Module32First(hModSnap, &me))
                        {
                            pi.path = me.szExePath;
                        }
                        CloseHandle(hModSnap);
                    }
                }

                // Try to get memory info using permissions that allow querying memory counters
                HANDLE hProcMem = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pi.pid);
                if (hProcMem)
                {
                    PROCESS_MEMORY_COUNTERS pmc = {0};
                    if (GetProcessMemoryInfo(hProcMem, &pmc, sizeof(pmc)))
                        pi.memKB = pmc.WorkingSetSize / 1024;
                    CloseHandle(hProcMem);
                }

                results->push_back(pi);
            } while (Process32Next(hSnap, &pe));
        }
        CloseHandle(hSnap);
    }

    // If the dialog window is still valid, post the results. Otherwise
    // free the results to avoid leaking memory when the UI has been destroyed.
    HWND hwnd = dlg->GetSafeHwnd();
    if (hwnd != NULL && IsValidWindow(hwnd))
    {
        // If posting the message fails for any reason, free the results to avoid leaking.
        if (!::PostMessage(hwnd, CMFCApplication1Dlg::WM_REFRESH_PROCESSES_DONE, (WPARAM)results, 0))
        {
            delete results;
        }
    }
    else
    {
        delete results;
    }
    return 0;
}

static BOOL CALLBACK EnumWindowsCloseCallback(HWND hWnd, LPARAM lParam)
{
    DWORD pid = 0; GetWindowThreadProcessId(hWnd, &pid);
    if (pid == (DWORD)lParam)
    {
        ::PostMessage(hWnd, WM_CLOSE, 0, 0);
    }
    return TRUE;
}

// ========== 进程排序与过滤 ==========

void CMFCApplication1Dlg::PopulateProcessList()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST1);
    if (!pList) return;

    pList->DeleteAllItems();

    for (int i = 0; i < (int)m_processes.size(); i++)
    {
        auto& pi = m_processes[i];
        CString pidStr;
        pidStr.Format(_T("%u"), pi.pid);
        CString memStr;
        memStr.Format(_T("%llu"), (unsigned long long)pi.memKB);

        LVITEM li = {0};
        li.mask = LVIF_TEXT;
        li.iItem = i;
        li.pszText = const_cast<LPTSTR>((LPCTSTR)pi.name);
        pList->InsertItem(&li);
        pList->SetItemText(i, 1, pidStr);
        pList->SetItemText(i, 2, pi.path);
        pList->SetItemText(i, 3, memStr);
    }
}

void CMFCApplication1Dlg::SortProcessList()
{
    if (m_nSortColumn < 0 || m_nSortColumn > 3) return;

    std::sort(m_processes.begin(), m_processes.end(),
        [this](const ProcInfo& a, const ProcInfo& b) -> bool
        {
            int cmp = 0;
            switch (m_nSortColumn)
            {
            case 0: cmp = a.name.CompareNoCase(b.name); break;
            case 1: cmp = (a.pid > b.pid) ? 1 : (a.pid < b.pid) ? -1 : 0; break;
            case 2: cmp = a.path.CompareNoCase(b.path); break;
            case 3: cmp = (a.memKB > b.memKB) ? 1 : (a.memKB < b.memKB) ? -1 : 0; break;
            }
            return m_bSortAscending ? (cmp < 0) : (cmp > 0);
        });
}

void CMFCApplication1Dlg::ApplyProcessFilter()
{
    CListCtrl* pList = (CListCtrl*)GetDlgItem(IDC_LIST1);
    if (!pList) return;

    // 只有 m_processes 里有数据时才处理
    if (m_processes.empty()) return;

    // 获取过滤文本
    CString filterText;
    CEdit* pEditFilter = (CEdit*)GetDlgItem(IDC_EDIT_PROCESS_FILTER);
    if (pEditFilter)
        pEditFilter->GetWindowText(filterText);

    bool useRegex = false;
    CButton* pBtnRegex = (CButton*)GetDlgItem(IDC_CHECK_PROCESS_REGEX);
    if (pBtnRegex)
        useRegex = (pBtnRegex->GetCheck() == BST_CHECKED);

    // 应用排序
    SortProcessList();

    // 如果有过滤文本，进行过滤
    if (!filterText.IsEmpty())
    {
        std::vector<ProcInfo> filtered;
        try
        {
            if (useRegex)
            {
                std::wregex re(filterText.GetString(), std::regex_constants::icase);
                for (auto& pi : m_processes)
                {
                    if (std::regex_search(std::wstring(pi.name), re) ||
                        std::regex_search(std::wstring(pi.path), re))
                    {
                        filtered.push_back(pi);
                    }
                }
            }
            else
            {
                CString lower = filterText;
                lower.MakeLower();
                for (auto& pi : m_processes)
                {
                    CString name = pi.name; name.MakeLower();
                    CString path = pi.path; path.MakeLower();
                    if (name.Find(lower) >= 0 || path.Find(lower) >= 0)
                        filtered.push_back(pi);
                }
            }
        }
        catch (...)
        {
            // 正则表达式无效时，忽略过滤
            filtered = m_processes;
        }

        // 用过滤后的数据填充列表
        pList->DeleteAllItems();
        for (int i = 0; i < (int)filtered.size(); i++)
        {
            auto& pi = filtered[i];
            CString pidStr;
            pidStr.Format(_T("%u"), pi.pid);
            CString memStr;
            memStr.Format(_T("%llu"), (unsigned long long)pi.memKB);

            LVITEM li = {0};
            li.mask = LVIF_TEXT;
            li.iItem = i;
            li.pszText = const_cast<LPTSTR>((LPCTSTR)pi.name);
            pList->InsertItem(&li);
            pList->SetItemText(i, 1, pidStr);
            pList->SetItemText(i, 2, pi.path);
            pList->SetItemText(i, 3, memStr);
        }
    }
    else
    {
        // 无过滤文本，直接填充全部
        PopulateProcessList();
    }

    // 更新表头排序箭头
    if (pList->GetHeaderCtrl())
    {
        for (int i = 0; i < 4; i++)
        {
            HDITEM hdi = {0};
            hdi.mask = HDI_TEXT;
            TCHAR buf[128];
            hdi.pszText = buf;
            hdi.cchTextMax = 128;
            pList->GetHeaderCtrl()->GetItem(i, &hdi);

            // 去掉已有的箭头符号
            CString text = hdi.pszText;
            if (text.GetLength() >= 2 &&
                (text[0] == _T('\x25B2') || text[0] == _T('\x25BC') || text[0] == _T(' ')))
            {
                text = text.Mid(2);  // 去掉 "▲ " 或 "▼ "
            }

            if (i == m_nSortColumn)
            {
                text = (m_bSortAscending ? _T("▲ ") : _T("▼ ")) + text;
            }

            hdi.mask = HDI_TEXT;
            hdi.pszText = const_cast<LPTSTR>((LPCTSTR)text);
            pList->GetHeaderCtrl()->SetItem(i, &hdi);
        }
    }
}

void CMFCApplication1Dlg::OnProcessColumnClick(NMHDR* pNMHDR, LRESULT* pResult)
{
    NM_LISTVIEW* pNMListView = (NM_LISTVIEW*)pNMHDR;
    int nCol = pNMListView->iSubItem;

    if (nCol == m_nSortColumn)
        m_bSortAscending = !m_bSortAscending;
    else
    {
        m_nSortColumn = nCol;
        m_bSortAscending = true;
    }

    ApplyProcessFilter();
    *pResult = 0;
}

void CMFCApplication1Dlg::OnProcessFilterChange()
{
    ApplyProcessFilter();
}