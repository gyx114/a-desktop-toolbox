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
        if (m_lastStatus != 0)
        {
            m_lastStatus = 0;
            pStatus->SetWindowText(_T("已停止"));
            pStatus->Invalidate();
        }
        // 自销毁：连点器已停止，关闭窗口
        KillTimer(1);
        if (GetParent())
            ::PostMessage(GetParent()->m_hWnd, WM_SPEED_DLG_CLOSED, 0, 0);
        DestroyWindow();
    }
    else if (m_pClicker->IsClicking())
    {
        if (m_lastStatus != 2)
        {
            m_lastStatus = 2;
            CString strMsg;
            strMsg.Format(_T("正在点击 (按 %c 停止)"), m_pClicker->GetKeyStop());
            pStatus->SetWindowText(strMsg);
            pStatus->Invalidate();
        }
    }
    else
    {
        if (m_lastStatus != 1)
        {
            m_lastStatus = 1;
            CString strMsg;
            strMsg.Format(_T("等待触发 (按 %c 开始, %c 停止)"),
                m_pClicker->GetKeyStart(), m_pClicker->GetKeyStop());
            pStatus->SetWindowText(strMsg);
            pStatus->Invalidate();
        }
    }
}

HBRUSH CAutoClickerSpeedDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH hbr = CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);

    if (pWnd && pWnd->GetDlgCtrlID() == IDC_STATIC_CLICK_STATUS)
    {
        pDC->SetBkMode(TRANSPARENT);
        switch (m_lastStatus)
        {
        case 2:  // 点击中 → 绿色
            pDC->SetTextColor(RGB(0, 170, 0));
            break;
        case 1:  // 等待触发 → 橙色
            pDC->SetTextColor(RGB(204, 136, 0));
            break;
        default:  // 已停止 → 灰色
            pDC->SetTextColor(RGB(136, 136, 136));
            break;
        }
    }

    return hbr;
}

BEGIN_MESSAGE_MAP(CAutoClickerSpeedDlg, CDialogEx)
    ON_WM_HSCROLL()
    ON_WM_CLOSE()
    ON_WM_TIMER()
    ON_WM_CTLCOLOR()
    ON_EN_CHANGE(IDC_EDIT_CLICK_SPEED, &CAutoClickerSpeedDlg::OnEnChangeClickSpeed)
END_MESSAGE_MAP()