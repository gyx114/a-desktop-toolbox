#pragma once
#include "AutoClicker.h"

// 自定义消息：速度窗口关闭，通知主窗口清空指针
static constexpr UINT WM_SPEED_DLG_CLOSED = WM_APP + 6;

class CAutoClickerSpeedDlg : public CDialogEx
{
public:
    CAutoClickerSpeedDlg(CAutoClicker* pClicker, CWnd* pParent = nullptr);
    void SetInterval(int ms);
    [[nodiscard]] int GetInterval() const;

protected:
    virtual BOOL OnInitDialog();
    virtual void OnOK();
    virtual void OnCancel();
    afx_msg void OnClose();
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnEnChangeClickSpeed();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    void UpdateStatus();
    DECLARE_MESSAGE_MAP()

private:
    CAutoClicker* m_pClicker;
    int m_interval{100};
};