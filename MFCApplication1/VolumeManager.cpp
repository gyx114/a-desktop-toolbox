// VolumeManager.cpp: 系统音量管理实现
#include "pch.h"
#include "framework.h"
#include "VolumeManager.h"
#include <functiondiscoverykeys_devpkey.h>

[[nodiscard]] int CVolumeManager::GetMasterVolumePercent()
{
    int pct = 100;
    HRESULT hr = CoInitialize(nullptr);
    bool bCoInit = SUCCEEDED(hr);

    IMMDeviceEnumerator* pEnum = nullptr;
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&pEnum))) && pEnum)
    {
        IMMDevice* pDevice = nullptr;
        if (SUCCEEDED(pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice)) && pDevice)
        {
            IAudioEndpointVolume* pVolume = nullptr;
            if (SUCCEEDED(pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&pVolume))) && pVolume)
            {
                float fVol = 0.0f;
                if (SUCCEEDED(pVolume->GetMasterVolumeLevelScalar(&fVol)))
                    pct = static_cast<int>(fVol * 100.0f + 0.5f);
                pVolume->Release();
            }
            pDevice->Release();
        }
        pEnum->Release();
    }

    if (bCoInit) CoUninitialize();
    return pct;
}

bool CVolumeManager::SetMasterVolumePercent(int percent)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    bool ok = false;
    HRESULT hr = CoInitialize(nullptr);
    bool bCoInit = SUCCEEDED(hr);

    IMMDeviceEnumerator* pEnum = nullptr;
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&pEnum))) && pEnum)
    {
        IMMDevice* pDevice = nullptr;
        if (SUCCEEDED(pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice)) && pDevice)
        {
            IAudioEndpointVolume* pVolume = nullptr;
            if (SUCCEEDED(pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&pVolume))) && pVolume)
            {
                float fVol = percent / 100.0f;
                ok = SUCCEEDED(pVolume->SetMasterVolumeLevelScalar(fVol, nullptr));
                pVolume->Release();
            }
            pDevice->Release();
        }
        pEnum->Release();
    }

    if (bCoInit) CoUninitialize();
    return ok;
}

void CVolumeManager::FetchVolumeAsync(HWND hwndNotify)
{
    std::jthread([hwndNotify]() {
        int pct = 100;
        HRESULT hr = CoInitialize(nullptr);
        bool bCoInit = SUCCEEDED(hr);

        IMMDeviceEnumerator* pEnum = nullptr;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&pEnum))) && pEnum)
        {
            IMMDevice* pDevice = nullptr;
            if (SUCCEEDED(pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice)) && pDevice)
            {
                IAudioEndpointVolume* pVolume = nullptr;
                if (SUCCEEDED(pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&pVolume))) && pVolume)
                {
                    float fVol = 0.0f;
                    if (SUCCEEDED(pVolume->GetMasterVolumeLevelScalar(&fVol)))
                        pct = static_cast<int>(fVol * 100.0f + 0.5f);
                    pVolume->Release();
                }
                pDevice->Release();
            }
            pEnum->Release();
        }

        if (bCoInit) CoUninitialize();
        ::PostMessage(hwndNotify, WM_VOLUME_UPDATED, static_cast<WPARAM>(pct), 0);
    }).detach();
}