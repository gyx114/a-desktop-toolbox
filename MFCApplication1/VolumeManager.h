// VolumeManager.h: System volume management encapsulation
#pragma once
#include <Mmdeviceapi.h>
#include <Endpointvolume.h>

// System master volume manager (via Core Audio EndpointVolume API)
class CVolumeManager
{
public:
    // Get system master volume percentage (0-100), returns 100 on failure
    [[nodiscard]] static int GetMasterVolumePercent();

    // Set system master volume percentage (0-100), returns success status
    static bool SetMasterVolumePercent(int percent);

    // Asynchronously fetch volume and send WM_VOLUME_UPDATED message to target window
    // wParam is the volume percentage
    static void FetchVolumeAsync(HWND hwndNotify);

    // Custom message ID
    static constexpr UINT WM_VOLUME_UPDATED = WM_APP + 5;
};