// VolumeManager.h: 系统音量管理封装
#pragma once
#include <Mmdeviceapi.h>
#include <Endpointvolume.h>

// 系统主音量管理器（通过 Core Audio EndpointVolume API）
class CVolumeManager
{
public:
    // 获取系统主音量百分比（0-100），失败返回 100
    [[nodiscard]] static int GetMasterVolumePercent();

    // 设置系统主音量百分比（0-100），返回是否成功
    static bool SetMasterVolumePercent(int percent);

    // 异步获取音量并发送 WM_VOLUME_UPDATED 消息到目标窗口
    // wParam 为音量百分比
    static void FetchVolumeAsync(HWND hwndNotify);

    // 自定义消息 ID
    static constexpr UINT WM_VOLUME_UPDATED = WM_APP + 5;
};