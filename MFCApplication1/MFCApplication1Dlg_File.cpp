#include "pch.h"
#include "framework.h"
#include "MFCApplication1Dlg.h"
#include "BatchRenameDlg.h"
#include "resource.h"
#include "Utils.h"

// File management: handle files dropped onto the dialog
void CMFCApplication1Dlg::OnDropFiles(HDROP hDropInfo)
{
    TCHAR szFilePath[MAX_PATH] = {0};
    if (DragQueryFile(hDropInfo, 0, szFilePath, MAX_PATH) == 0)
    {
        DragFinish(hDropInfo);
        return;
    }

    // 检查是否为文件夹
    DWORD attrs = ::GetFileAttributes(szFilePath);
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY))
    {
        // 拖入的是文件夹，打开文件夹处理窗口
        DragFinish(hDropInfo);
        CBatchRenameDlg dlg(this, szFilePath);
        dlg.DoModal();
        return;
    }

    // 拖入的是文件，继续原有tab5逻辑
    m_strDroppedFilePath = szFilePath;
    // display path
    CWnd* pStatic = GetDlgItem(IDC_STATIC_PATH);
    if (pStatic)
        pStatic->SetWindowText(m_strDroppedFilePath);
    else
        ;
    // restore edit IDC_EDIT4 to default content whenever a new file is dropped
    SetDlgItemText(IDC_EDIT4, AfxGetApp()->GetProfileString(_T("Template"), _T("DefaultReportName"), _T("")));

    // Automatically switch to tab 5 (index 4) when a file is dropped
    CTabCtrl* pTab = (CTabCtrl*)GetDlgItem(IDC_TAB1);
    if (pTab)
    {
        pTab->SetCurSel(4);
        LRESULT res = 0;
        // call handler to update control visibility
        OnTcnSelchangeTab1(NULL, &res);
        // populate rename edits: IDC_EDIT7 (basename without ext), IDC_EDIT8 (extension without dot)
        int nSlash = m_strDroppedFilePath.ReverseFind(_T('\\'));
        int nDot = m_strDroppedFilePath.ReverseFind(_T('.'));
        CString base, ext;
        if (nDot != -1 && nDot > nSlash)
        {
            base = m_strDroppedFilePath.Mid(nSlash + 1, nDot - nSlash - 1);
            ext = m_strDroppedFilePath.Mid(nDot + 1); // without dot
        }
        else
        {
            base = m_strDroppedFilePath.Mid(nSlash + 1);
            ext = _T("");
        }
        SetDlgItemText(IDC_EDIT7, base);
        SetDlgItemText(IDC_EDIT8, ext);
    }

    DragFinish(hDropInfo);
    CDialogEx::OnDropFiles(hDropInfo);
}

void CMFCApplication1Dlg::OnBnClickedButton3()
{
    // Read current file path and requested new name
    CString src = m_strDroppedFilePath;
    if (src.IsEmpty())
    {
        MessageBox(_T("请先拖入文件！"), _T("提示"), MB_OK | MB_ICONWARNING);
        return;
    }

    CString newName;
    GetDlgItemText(IDC_EDIT4, newName);
    newName.Trim();
    if (newName.IsEmpty())
    {
        newName = AfxGetApp()->GetProfileString(_T("Template"), _T("DefaultReportName"), _T(""));
        if (newName.IsEmpty())
        {
            MessageBox(_T("请先在 文件→设置→文件命名 中配置默认文件名。"), _T("提示"), MB_OK | MB_ICONWARNING);
            SetDlgItemText(IDC_EDIT4, newName);
            return;
        }
        SetDlgItemText(IDC_EDIT4, newName);
    }

    // build paths
    int nSlash = src.ReverseFind(_T('\\'));
    CString dir = (nSlash != -1) ? src.Left(nSlash + 1) : CString(_T(""));
    int nDot = src.ReverseFind(_T('.'));
    CString ext = (nDot != -1 && nDot > nSlash) ? src.Mid(nDot) : CString(_T(""));

    CString candidate = dir + newName + ext;
    CString baseName = newName;
    // avoid infinite loop: limit attempts
    int attempt = 0;
    while (GetFileAttributes(candidate) != INVALID_FILE_ATTRIBUTES && attempt < 1000)
    {
        attempt++;
        baseName = newName + _T("_副本");
        candidate = dir + baseName + ext;
        newName = baseName; // next iteration will append again if needed
    }

    if (attempt >= 1000)
    {
        MessageBox(_T("无法生成唯一文件名，请检查目标目录权限或手动选择不同名称。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    if (CopyFile(src, candidate, FALSE))
    {
        CString msg; msg.Format(_T("生成副本成功！\n保存路径：%s"), candidate);
        MessageBox(msg, _T("成功"), MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        CString err; err.Format(_T("生成副本失败，错误代码：%u"), GetLastError());
        MessageBox(err, _T("错误"), MB_OK | MB_ICONERROR);
    }
}


// Rename handler for IDC_BUTTON23
void CMFCApplication1Dlg::OnBnClickedButton23()
{
    if (m_strDroppedFilePath.IsEmpty())
    {
        MessageBox(_T("请先拖入文件！"), _T("提示"), MB_OK | MB_ICONWARNING);
        return;
    }

    CString newBase, newExt;
    GetDlgItemText(IDC_EDIT7, newBase);
    GetDlgItemText(IDC_EDIT8, newExt);
    newBase.Trim(); newExt.Trim();
    if (newBase.IsEmpty())
    {
        MessageBox(_T("文件名不能为空。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    // validate characters not allowed in Windows filenames
    CString invalid = _T("\\/:*?\"<>|");
    for (int i = 0; i < invalid.GetLength(); ++i)
    {
        if (newBase.Find(invalid[i]) != -1 || newExt.Find(invalid[i]) != -1)
        {
            MessageBox(_T("文件名或扩展名包含不合法字符。\\ / : * ? \" < > | 不允许。"), _T("错误"), MB_OK | MB_ICONERROR);
            return;
        }
    }

    int nSlash = m_strDroppedFilePath.ReverseFind(_T('\\'));
    int nDot = m_strDroppedFilePath.ReverseFind(_T('.'));
    CString dir = (nSlash != -1) ? m_strDroppedFilePath.Left(nSlash + 1) : CString(_T(""));
    CString srcExt = (nDot != -1 && nDot > nSlash) ? m_strDroppedFilePath.Mid(nDot) : CString(_T(""));

    CString dst;
    if (newExt.IsEmpty())
        dst.Format(_T("%s%s%s"), dir, newBase, srcExt);
    else
        dst.Format(_T("%s%s.%s"), dir, newBase, newExt);

    if (GetFileAttributes(dst) != INVALID_FILE_ATTRIBUTES)
    {
        MessageBox(_T("目标文件已存在，请选择其他名称。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    if (MoveFile(m_strDroppedFilePath, dst))
    {
        CString msg; msg.Format(_T("重命名成功：%s"), dst);
        MessageBox(msg, _T("成功"), MB_OK | MB_ICONINFORMATION);
        m_strDroppedFilePath = dst;
        SetDlgItemText(IDC_STATIC_PATH, m_strDroppedFilePath);
        // do not modify IDC_EDIT4 here; user-managed copy name should remain as-is
    }
    else
    {
        DWORD err = GetLastError();
        CString emsg; emsg.Format(_T("重命名失败：%s (错误代码 %u)"), FormatLastError(err), err);
        MessageBox(emsg, _T("错误"), MB_OK | MB_ICONERROR);
        if (err == ERROR_ACCESS_DENIED)
        {
            if (PromptRestartElevated())
            {
                // Close dialog to allow restarted elevated instance to run
                EndDialog(IDOK);
                return;
            }
        }
    }
}


void CMFCApplication1Dlg::OnStnClickedStaticPath()
{
    // TODO: 在此添加控件通知处理程序代码
}

// Copy selected dropped file to target directory specified in IDC_MFCEDITBROWSE2
void CMFCApplication1Dlg::OnBnClickedButton25()
{
    if (m_strDroppedFilePath.IsEmpty())
    {
        MessageBox(_T("请先拖入文件！"), _T("提示"), MB_OK | MB_ICONWARNING);
        return;
    }

    CString destDir;
    GetDlgItemText(IDC_MFCEDITBROWSE2, destDir);
    destDir.Trim();
    if (destDir.IsEmpty() || GetFileAttributes(destDir) == INVALID_FILE_ATTRIBUTES)
    {
        MessageBox(_T("目标路径无效，请在下方输入或选择有效目录。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    DWORD attr = GetFileAttributes(destDir);
    if (!(attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        MessageBox(_T("目标路径不是目录，请选择文件夹路径。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    int pos = m_strDroppedFilePath.ReverseFind(_T('\\'));
    CString fileName = (pos != -1) ? m_strDroppedFilePath.Mid(pos + 1) : m_strDroppedFilePath;
    CString dst; dst.Format(_T("%s\\%s"), destDir, fileName);

    // If exists, generate unique name
    CString nameOnly = fileName;
    CString ext = _T("");
    int dot = fileName.ReverseFind('.');
    if (dot != -1) { nameOnly = fileName.Left(dot); ext = fileName.Mid(dot); }
    int attempt = 0;
    while (GetFileAttributes(dst) != INVALID_FILE_ATTRIBUTES && attempt < 1000)
    {
        attempt++;
        CString newName; newName.Format(_T("%s_copy%d%s"), nameOnly, attempt, ext);
        dst.Format(_T("%s\\%s"), destDir, newName);
    }

    if (attempt >= 1000)
    {
        MessageBox(_T("目标文件无法生成唯一名称，请检查目录或手动指定不同的名称。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    if (CopyFile(m_strDroppedFilePath, dst, FALSE))
    {
        CString msg; msg.Format(_T("复制成功：%s"), dst);
        MessageBox(msg, _T("完成"), MB_OK | MB_ICONINFORMATION);
    }
    else
    {
        CString err; err.Format(_T("复制失败，错误代码：%u"), GetLastError());
        MessageBox(err, _T("错误"), MB_OK | MB_ICONERROR);
    }
}

// Move selected dropped file to target directory specified in IDC_MFCEDITBROWSE2
void CMFCApplication1Dlg::OnBnClickedButton26()
{
    if (m_strDroppedFilePath.IsEmpty())
    {
        MessageBox(_T("请先拖入文件！"), _T("提示"), MB_OK | MB_ICONWARNING);
        return;
    }

    CString destDir;
    GetDlgItemText(IDC_MFCEDITBROWSE2, destDir);
    destDir.Trim();
    if (destDir.IsEmpty() || GetFileAttributes(destDir) == INVALID_FILE_ATTRIBUTES)
    {
        MessageBox(_T("目标路径无效，请在下方输入或选择有效目录。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    DWORD attr = GetFileAttributes(destDir);
    if (!(attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        MessageBox(_T("目标路径不是目录，请选择文件夹路径。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    int pos = m_strDroppedFilePath.ReverseFind(_T('\\'));
    CString fileName = (pos != -1) ? m_strDroppedFilePath.Mid(pos + 1) : m_strDroppedFilePath;
    CString dst; dst.Format(_T("%s\\%s"), destDir, fileName);

    if (GetFileAttributes(dst) != INVALID_FILE_ATTRIBUTES)
    {
        MessageBox(_T("目标目录中已存在同名文件，请先移除或更改目标路径。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    if (MoveFile(m_strDroppedFilePath, dst))
    {
        CString msg; msg.Format(_T("移动成功：%s"), dst);
        MessageBox(msg, _T("完成"), MB_OK | MB_ICONINFORMATION);
        m_strDroppedFilePath = dst;
        SetDlgItemText(IDC_STATIC_PATH, m_strDroppedFilePath);
    }
    else
    {
        DWORD err = GetLastError();
        CString em; em.Format(_T("移动失败，错误代码：%u"), err);
        MessageBox(em, _T("错误"), MB_OK | MB_ICONERROR);
        if (err == ERROR_ACCESS_DENIED)
        {
            if (PromptRestartElevated())
            {
                EndDialog(IDOK);
                return;
            }
        }
    }
}

// 复选按钮：设置或取消开机自启动（写入或删除当前用户 Run 注册表项）
void CMFCApplication1Dlg::OnBnClickedCheck1()
{
    CButton* pCheck = (CButton*)GetDlgItem(IDC_CHECK1);
    if (!pCheck) return;

    // 获取可执行文件名及路径
    TCHAR exePath[MAX_PATH] = {0};
    if (GetModuleFileName(NULL, exePath, MAX_PATH) == 0)
    {
        MessageBox(_T("无法获取程序路径。"), _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    CString csExePath = exePath;
    int pos = csExePath.ReverseFind(_T('\\'));
    CString keyName = (pos != -1) ? csExePath.Mid(pos + 1) : csExePath; // 使用可执行文件名作为注册表项名

    HKEY hKey = NULL;
    LONG ret = RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, KEY_SET_VALUE | KEY_QUERY_VALUE, &hKey);
    if (ret != ERROR_SUCCESS)
    {
        DWORD err = GetLastError();
        CString msg;
        msg.Format(_T("无法打开注册表键，请检查权限。错误：%s"), FormatLastError(err));
        if (err == ERROR_ACCESS_DENIED)
        {
            if (PromptRestartElevated())
                return;
        }
        MessageBox(msg, _T("错误"), MB_OK | MB_ICONERROR);
        return;
    }

    // 查询当前注册表中是否已有自启动项，以实现复选按钮的开关切换效果
    DWORD type = 0;
    TCHAR buf[MAX_PATH] = {0};
    DWORD bufSize = sizeof(buf);
    LONG checkRet = RegQueryValueEx(hKey, keyName, NULL, &type, (LPBYTE)buf, &bufSize);
    BOOL isAlreadyAutostart = (checkRet == ERROR_SUCCESS && type == REG_SZ);

    if (isAlreadyAutostart)
    {
        // 取消自启动：删除注册表值
        ret = RegDeleteValue(hKey, keyName);
        if (ret == ERROR_SUCCESS || ret == ERROR_FILE_NOT_FOUND)
        {
            MessageBox(_T("已取消开机自启动"), _T("提示"), MB_OK | MB_ICONINFORMATION);
            pCheck->SetCheck(BST_UNCHECKED);
            AfxGetApp()->WriteProfileInt(_T("Settings"), _T("AutoStart"), 0);
        }
        else
        {
            DWORD err = GetLastError();
            CString msg;
            msg.Format(_T("删除启动项失败，请检查权限。错误：%s"), FormatLastError(err));
            if (err == ERROR_ACCESS_DENIED)
            {
                if (PromptRestartElevated()) { RegCloseKey(hKey); return; }
            }
            MessageBox(msg, _T("错误"), MB_OK | MB_ICONERROR);
            pCheck->SetCheck(BST_CHECKED);
        }
    }
    else
    {
        // 设置自启动：写入注册表，值为可执行完整路径并附加我们约定的启动标记 --elevate
        CString runValue;
        // Quote path in case it contains spaces
        runValue.Format(_T("\"%s\" --elevate"), csExePath);
        LONG setRet = RegSetValueEx(hKey, keyName, 0, REG_SZ, (const BYTE*)(LPCTSTR)runValue, (runValue.GetLength() + 1) * sizeof(TCHAR));
        if (setRet == ERROR_SUCCESS)
        {
            MessageBox(_T("已设置为开机自启动。"), _T("提示"), MB_OK | MB_ICONINFORMATION);
            pCheck->SetCheck(BST_CHECKED);
            AfxGetApp()->WriteProfileInt(_T("Settings"), _T("AutoStart"), 1);
        }
        else
        {
            CString msg;
            msg.Format(_T("添加启动项失败，请检查权限。错误：%s"), FormatLastError(GetLastError()));
            if (GetLastError() == ERROR_ACCESS_DENIED)
            {
                if (PromptRestartElevated()) { RegCloseKey(hKey); return; }
            }
            MessageBox(msg, _T("错误"), MB_OK | MB_ICONERROR);
            // 恢复为未选中
            pCheck->SetCheck(BST_UNCHECKED);
        }
    }

    RegCloseKey(hKey);
}

void CMFCApplication1Dlg::OnBnClickedCheck2()
{
    CButton* pCheck = (CButton*)GetDlgItem(IDC_CHECK2);
    if (!pCheck) return;

    // 更新内部标志
    m_bMinimizeOnClose = (pCheck->GetCheck() == BST_CHECKED);
}