#include "pch.h"
#include "framework.h"
#include "AutoClickerSpeedDlg.h"
#include "resource.h"

CAutoClickerSpeedDlg::CAutoClickerSpeedDlg(CAutoClicker* pClicker, CWnd* pParent /*= nullptr*/)
    : CDialogEx(IDD_CLICK_SPEED_DIALOG, pParent), m_pClicker(pClicker) {}

void CAutoClickerSpeedDlg::SetInterval(int ms)
{
    m_interval = ms;
    if (GetSafeHwnd())
    {
        SetDlgItemInt(IDC_EDIT_CLICK_SPEED, ms);
        CSliderCtrl* pSlider = static_cast<CSliderCtrl*>(GetDlgItem(IDC_SLIDER_CLICK_SPEED));
        if (pSlider) pSlider->SetPos(ms);
    }
}

int CAutoClickerSpeedDlg::GetInterval() const { return m_interval; }

BOOL CAutoClickerSpeedDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    CSliderCtrl* pSlider = static_cast<CSliderCtrl*>(GetDlgItem(IDC_SLIDER_CLICK_SPEED));
    if (pSlider)
    {
        pSlider->SetRange(10, 1000);
        pSlider->SetPos(m_interval);
        pSlider->SetTicFreq(100);
    }
    SetDlgItemInt(IDC_EDIT_CLICK_SPEED, m_interval);
    UpdateStatus();
    SetTimer(1, 200, nullptr);  // 每200ms刷新状态
    return TRUE;
}

void CAutoClickerSpeedDlg::OnOK() { /* 不关闭，仅应用 */ }

void CAutoClickerSpeedDlg::OnCancel() { OnClose(); }

void CAutoClickerSpeedDlg::OnClose()
{
    // 停止连点器
    if (m_pClicker)
        m_pClicker->Stop();

    // 通知主窗口清空指针
    if (GetParent())
        ::PostMessage(GetParent()->m_hWnd, WM_SPEED_DLG_CLOSED, 0, 0);

    KillTimer(1);
    DestroyWindow();
}

void CAutoClickerSpeedDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    CSliderCtrl* pSlider = static_cast<CSliderCtrl*>(GetDlgItem(IDC_SLIDER_CLICK_SPEED));
    if (pSlider && pScrollBar && pSlider->m_hWnd == pScrollBar->m_hWnd)
    {
        m_interval = pSlider->GetPos();
        SetDlgItemInt(IDC_EDIT_CLICK_SPEED, m_interval);
        if (m_pClicker) m_pClicker->SetInterval(m_interval);
    }
}

void CAutoClickerSpeedDlg::OnEnChangeClickSpeed()
{
    int val = GetDlgItemInt(IDC_EDIT_CLICK_SPEED);
    if (val >= 10 && val <= 1000)
    {
        m_interval = val;
        CSliderCtrl* pSlider = static_cast<CSliderCtrl*>(GetDlgItem(IDC_SLIDER_CLICK_SPEED));
        if (pSlider) pSlider->SetPos(val);
        if (m_pClicker) m_pClicker->SetInterval(val);
    }
}

void CAutoClickerSpeedDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == 1)
        UpdateStatus();
}

void CAutoClickerSpeedDlg::UpdateStatus()
{
    if (!m_pClicker) return;
    CWnd* pStatus = GetDlgItem(IDC_STATIC_CLICK_STATUS);
    if (!pStatus) return;

    if (!m_pClicker->IsRunning())
    {
        pStatus->SetWindowText(_T("已停止"));
        // 自销毁：连点器已停止，关闭窗口
        KillTimer(1);
        if (GetParent())
            ::PostMessage(GetParent()->m_hWnd, WM_SPEED_DLG_CLOSED, 0, 0);
        DestroyWindow();
    }
    else if (m_pClicker->IsClicking())
    {
        pStatus->SetWindowText(_T("正在点击"));
    }
    else
    {
        pStatus->SetWindowText(_T("等待触发 (按 A 开始)"));
    }
}

BEGIN_MESSAGE_MAP(CAutoClickerSpeedDlg, CDialogEx)
    ON_WM_HSCROLL()
    ON_WM_CLOSE()
    ON_WM_TIMER()
    ON_EN_CHANGE(IDC_EDIT_CLICK_SPEED, &CAutoClickerSpeedDlg::OnEnChangeClickSpeed)
END_MESSAGE_MAP()