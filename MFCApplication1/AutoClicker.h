// AutoClicker.h: 连点器功能封装
#pragma once
#include <stop_token>
#include <thread>
#include <atomic>

// 连点器类：通过大写 A 键开始连点，大写 B 键停止
// 使用 std::jthread 和 std::stop_source 实现协作式线程取消
class CAutoClicker
{
public:
    CAutoClicker() = default;
    ~CAutoClicker();

    // 禁止拷贝
    CAutoClicker(const CAutoClicker&) = delete;
    CAutoClicker& operator=(const CAutoClicker&) = delete;

    // 启动连点器（intervalMs: 点击间隔毫秒，hwndOwner: 用于通知的主窗口）
    void Start(int intervalMs, HWND hwndOwner);

    // 停止连点器
    void Stop();

    // 暂停键盘监听（不改变 m_bEnabled 状态，用于 OCR 截图等场景）
    void Pause();

    // 恢复键盘监听（仅在 m_bEnabled 为 true 时有效）
    void Resume();

    // 动态调整点击间隔（毫秒），无需停止连点器
    void SetInterval(int intervalMs);

    // 设置触发键（默认 A=开始, B=停止）
    void SetKeys(char keyStart, char keyStop);

    // 是否正在运行
    [[nodiscard]] bool IsRunning() const { return m_bEnabled.load(); }
    [[nodiscard]] bool IsClicking() const { return m_bClicking.load(); }

    // 获取当前触发键
    [[nodiscard]] char GetKeyStart() const { return m_keyStart.load(); }
    [[nodiscard]] char GetKeyStop() const { return m_keyStop.load(); }

    // 自定义消息：连点已停止，wParam 和 lParam 均为 0
    static constexpr UINT WM_STOPPED = WM_APP + 4;

private:
    // 线程函数
    static void ClickThreadFunc(std::stop_token stoken, int intervalMs);
    void MonitorThreadFunc(std::stop_token stoken);

    // 检测大写按键
    [[nodiscard]] static bool IsUppercaseKeyPressed(char ch) noexcept;

    // 弹出输入框获取间隔
    static int PromptForIntervalMs(HWND hwndParent);

    std::atomic<bool> m_bEnabled{false};
    std::atomic<bool> m_bClicking{false};
    std::atomic<bool> m_bMonitorRunning{false};
    std::atomic<bool> m_bPaused{false};
    std::atomic<int> m_intervalMs{100};
    std::atomic<char> m_keyStart{'A'};
    std::atomic<char> m_keyStop{'B'};

    std::jthread m_clickThread;
    std::jthread m_monitorThread;
    std::stop_source m_clickStopSource;
    std::stop_source m_monitorStopSource;

    HWND m_hwndOwner{nullptr};
};