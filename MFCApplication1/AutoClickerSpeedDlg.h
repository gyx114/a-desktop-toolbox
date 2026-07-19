#pragma once
#include "AutoClicker.h"

// Custom message: speed dialog closed, notify main window to clear pointer
static constexpr UINT WM_SPEED_DLG_CLOSED = WM_APP + 6;

class CAutoClickerSpeedDlg : public CDialogEx
{
public:
    CAutoClickerSpeedDlg(CAutoClicker* pClicker, CWnd* pParent = nullptr);
    void SetInterval(int ms);
    [[nodiscard]] int GetInterval() const;

protected:
    virtual BOOL OnInitDialog();
    virtual BOOL PreTranslateMessage(MSG* pMsg) override;
    virtual void OnOK();
    virtual void OnCancel();
    afx_msg void OnClose();
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnEnChangeClickSpeed();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    void UpdateStatus();
    DECLARE_MESSAGE_MAP()

private:
    enum class ClickStatus { Stopped, Waiting, Clicking };
    CAutoClicker* m_pClicker;
    int m_interval{100};
    ClickStatus m_lastStatus{ClickStatus::Stopped};
};