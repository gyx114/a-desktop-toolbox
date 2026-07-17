#include "pch.h"
#include "framework.h"
#include "RegexGuideDlg.h"
#include "resource.h"

CRegexGuideDlg::CRegexGuideDlg(CWnd* pParent) : CDialogEx(IDD_REGEX_GUIDE_DLG, pParent)
{
}

void CRegexGuideDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CRegexGuideDlg, CDialogEx)
END_MESSAGE_MAP()

BOOL CRegexGuideDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    CString guide =
        _T("========== 正则表达式规则指南 ==========\r\n\r\n")
        _T("本工具使用 C++ std::regex，默认 ECMAScript 语法（与 JavaScript 正则兼容）。\r\n\r\n")
        _T("【基本语法】\r\n")
        _T("  .        匹配任意单个字符（换行符除外）\r\n")
        _T("  \\d       匹配任意数字 [0-9]\r\n")
        _T("  \\D       匹配非数字\r\n")
        _T("  \\w       匹配字母、数字、下划线 [a-zA-Z0-9_]\r\n")
        _T("  \\W       匹配非单词字符\r\n")
        _T("  \\s       匹配空白字符（空格、制表符等）\r\n")
        _T("  \\S       匹配非空白字符\r\n")
        _T("  [abc]    匹配 a、b 或 c 中的任意一个\r\n")
        _T("  [^abc]   匹配除了 a、b、c 之外的任意字符\r\n")
        _T("  [a-z]    匹配小写字母 a 到 z\r\n\r\n")
        _T("【量词】\r\n")
        _T("  *        匹配前面的表达式 0 次或多次\r\n")
        _T("  +        匹配前面的表达式 1 次或多次\r\n")
        _T("  ?        匹配前面的表达式 0 次或 1 次\r\n")
        _T("  {n}      匹配前面的表达式恰好 n 次\r\n")
        _T("  {n,}     匹配前面的表达式至少 n 次\r\n")
        _T("  {n,m}    匹配前面的表达式 n 到 m 次\r\n\r\n")
        _T("【位置锚定】\r\n")
        _T("  ^        匹配字符串开头\r\n")
        _T("  $        匹配字符串结尾\r\n")
        _T("  \\b       匹配单词边界\r\n\r\n")
        _T("【捕获组与替换】\r\n")
        _T("  (pattern)   捕获组，匹配的内容可在替换中引用\r\n")
        _T("  (?:pattern) 非捕获组，不保存匹配内容\r\n")
        _T("  $1, $2, ... 在替换文本中引用第 1、第 2 个捕获组\r\n")
        _T("  $&           在替换文本中引用整个匹配\r\n\r\n")
        _T("【常用示例】\r\n")
        _T("  删除所有数字:\r\n")
        _T("    查找: \\d+    替换: (空)\r\n\r\n")
        _T("  删除前导序号（如 \"01_\"）:\r\n")
        _T("    查找: ^\\d+_    替换: (空)\r\n\r\n")
        _T("  将 \"IMG_001\" 改为 \"Photo_001\":\r\n")
        _T("    查找: ^IMG    替换: Photo\r\n\r\n")
        _T("  交换 \"name_date\" 为 \"date_name\":\r\n")
        _T("    查找: ^(\\w+)_(\\w+)$    替换: $2_$1\r\n\r\n")
        _T("  删除括号及内容:\r\n")
        _T("    查找: \\(.*?\\)    替换: (空)\r\n\r\n")
        _T("  删除末尾编号 \"(1)\", \"(2)\":\r\n")
        _T("    查找: \\(\\d+\\)$    替换: (空)\r\n\r\n")
        _T("  将空格替换为下划线:\r\n")
        _T("    查找: \\s+    替换: _\r\n\r\n")
        _T("【注意事项】\r\n")
        _T("  - 替换仅作用于文件名主干（不含扩展名）\r\n")
        _T("  - 扩展名不会被修改\r\n")
        _T("  - 正则无效时，替换规则将被忽略\r\n")
        _T("  - 特殊字符需转义: . * + ? ( ) [ ] { } \\ ^ $ |");

    SetDlgItemText(IDC_STATIC, guide);
    return TRUE;
}