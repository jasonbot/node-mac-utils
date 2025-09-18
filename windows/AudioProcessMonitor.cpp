// AudioProcessMonitor.cpp
//

// Include windows.h FIRST, before any other Windows headers
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX  // Prevent Windows from defining min/max macros
#include <windows.h>

// Now include other Windows headers
#include <psapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propkey.h>
#include <devpkey.h>

// Standard library includes
#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>
#include <chrono>
#include <unordered_map>
#include <algorithm>
#include <cctype>

// Project includes
#include "AudioProcessMonitor.h"

#pragma comment(lib, "Ole32.lib")

// Function to get process executable path from PID
static std::string GetProcessExecutablePath(DWORD processID) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processID);
    if (!hProcess) return "Unknown";

    WCHAR path[MAX_PATH];
    DWORD size = MAX_PATH;
    
    if (QueryFullProcessImageNameW(hProcess, 0, path, &size)) {
        CloseHandle(hProcess);
        // Convert wide string to regular string
        int strSize = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
        std::string result(strSize, 0);
        WideCharToMultiByte(CP_UTF8, 0, path, -1, &result[0], strSize, nullptr, nullptr);
        if (!result.empty() && result.back() == 0) {
            result.pop_back();
        }
        return result;
    }

    CloseHandle(hProcess);
    return "Unknown";
}

// Enhanced state caching for Bluetooth device debouncing and power management
struct BluetoothDeviceState {
    bool lastActiveState;
    bool lastReportedState;  // What we last reported to the caller
    std::chrono::steady_clock::time_point lastStateChange;
    std::chrono::steady_clock::time_point lastActivityTime;  // Last time any activity was detected
    std::chrono::steady_clock::time_point lastReportTime;    // Last time we reported a state
    int consecutiveActiveChecks;
    int consecutiveInactiveChecks;
    int rapidStateChangeCount;  // Track rapid flapping
    std::chrono::steady_clock::time_point rapidStateChangeWindow;

    BluetoothDeviceState() :
        lastActiveState(false),
        lastReportedState(false),
        lastStateChange(std::chrono::steady_clock::now()),
        lastActivityTime(std::chrono::steady_clock::now()),
        lastReportTime(std::chrono::steady_clock::now()),
        consecutiveActiveChecks(0),
        consecutiveInactiveChecks(0),
        rapidStateChangeCount(0),
        rapidStateChangeWindow(std::chrono::steady_clock::now()) {}
};

static std::unordered_map<std::wstring, BluetoothDeviceState> bluetoothStateCache;

// Enhanced debouncing constants for power management scenarios
static const int BLUETOOTH_DEBOUNCE_MS = 3000; // Increased to 3 seconds for power management
static const int BLUETOOTH_ACTIVE_HOLD_MS = 5000; // Hold active state for 5 seconds after last activity
static const int REQUIRED_CONSECUTIVE_ACTIVE_CHECKS = 2; // Reduced for faster response
static const int REQUIRED_CONSECUTIVE_INACTIVE_CHECKS = 4; // More checks needed to go inactive
static const int RAPID_CHANGE_WINDOW_MS = 10000; // 10 second window for flapping detection
static const int MAX_RAPID_CHANGES = 5; // Max state changes in window before extending debounce
static const int EXTENDED_DEBOUNCE_MS = 8000; // Extended debounce for flapping devices

// Helper function to get device ID for state tracking
static std::wstring GetDeviceId(IMMDevice* pDevice) {
    LPWSTR deviceId = nullptr;
    HRESULT hr = pDevice->GetId(&deviceId);
    if (FAILED(hr)) return L"";

    std::wstring id(deviceId);
    CoTaskMemFree(deviceId);
    return id;
}

// Enhanced function to check if device is Bluetooth using reliable Windows property keys
static bool IsBluetoothDevice(IMMDevice* pDevice) {
    IPropertyStore* pProps = nullptr;
    HRESULT hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
    if (FAILED(hr)) return false;

    bool isBluetooth = false;

    // Method 1: Check device instance ID for Bluetooth hardware patterns (most reliable)
    PROPVARIANT varInstanceId;
    PropVariantInit(&varInstanceId);
    hr = pProps->GetValue(PKEY_Device_InstanceId, &varInstanceId);
    if (SUCCEEDED(hr) && varInstanceId.vt == VT_LPWSTR) {
        std::wstring instanceId = varInstanceId.pwszVal;
        // Convert to uppercase for case-insensitive matching
        std::transform(instanceId.begin(), instanceId.end(), instanceId.begin(), ::towupper);
        // Bluetooth devices have specific patterns in their instance IDs
        isBluetooth = (instanceId.find(L"BTHENUM") != std::wstring::npos ||
                      instanceId.find(L"BTH\\") != std::wstring::npos ||
                      instanceId.find(L"BLUETOOTH") != std::wstring::npos);
    }
    PropVariantClear(&varInstanceId);

    // Method 2: Check device hardware IDs for Bluetooth vendor patterns
    if (!isBluetooth) {
        PROPVARIANT varHardwareIds;
        PropVariantInit(&varHardwareIds);
        hr = pProps->GetValue(PKEY_Device_HardwareIds, &varHardwareIds);
        if (SUCCEEDED(hr) && varHardwareIds.vt == (VT_VECTOR | VT_LPWSTR)) {
            for (ULONG i = 0; i < varHardwareIds.calpwstr.cElems; i++) {
                std::wstring hardwareId = varHardwareIds.calpwstr.pElems[i];
                std::transform(hardwareId.begin(), hardwareId.end(), hardwareId.begin(), ::towupper);
                if (hardwareId.find(L"BLUETOOTH") != std::wstring::npos ||
                    hardwareId.find(L"BTHENUM") != std::wstring::npos ||
                    hardwareId.find(L"BTH\\") != std::wstring::npos) {
                    isBluetooth = true;
                    break;
                }
            }
        }
        PropVariantClear(&varHardwareIds);
    }

    // Method 3: Check container ID against Bluetooth service class (when available)
    if (!isBluetooth) {
        PROPVARIANT varContainerId;
        PropVariantInit(&varContainerId);
        hr = pProps->GetValue(PKEY_Device_ContainerId, &varContainerId);
        if (SUCCEEDED(hr) && varContainerId.vt == VT_CLSID) {
            // If we have a container ID, we could potentially check if it's associated
            // with a Bluetooth service, but this requires additional APIs
            // For now, this is a placeholder for future enhancement
        }
        PropVariantClear(&varContainerId);
    }

    // Method 4: Check parent device ID for Bluetooth radio patterns
    if (!isBluetooth) {
        PROPVARIANT varParent;
        PropVariantInit(&varParent);
        hr = pProps->GetValue(PKEY_Device_Parent, &varParent);
        if (SUCCEEDED(hr) && varParent.vt == VT_LPWSTR) {
            std::wstring parentId = varParent.pwszVal;
            std::transform(parentId.begin(), parentId.end(), parentId.begin(), ::towupper);
            isBluetooth = (parentId.find(L"BLUETOOTH") != std::wstring::npos ||
                          parentId.find(L"BTHENUM") != std::wstring::npos);
        }
        PropVariantClear(&varParent);
    }

    // Method 5: Check device class GUID for Bluetooth audio classes
    if (!isBluetooth) {
        PROPVARIANT varClassGuid;
        PropVariantInit(&varClassGuid);
        hr = pProps->GetValue(PKEY_Device_ClassGuid, &varClassGuid);
        if (SUCCEEDED(hr) && varClassGuid.vt == VT_LPWSTR) {
            std::wstring classGuid = varClassGuid.pwszVal;
            std::transform(classGuid.begin(), classGuid.end(), classGuid.begin(), ::towupper);

            // Check for Bluetooth-specific device class GUIDs
            // {e0cbf06c-cd8b-4647-bb8a-263b43f0f974} - Bluetooth devices
            // {4d36e96c-e325-11ce-bfc1-08002be10318} - Sound, video and game controllers (may include BT audio)
            if (classGuid.find(L"E0CBF06C-CD8B-4647-BB8A-263B43F0F974") != std::wstring::npos) {
                isBluetooth = true;
            }
        }
        PropVariantClear(&varClassGuid);
    }

    // Method 6: Check bus type reported by the device
    if (!isBluetooth) {
        PROPVARIANT varBusType;
        PropVariantInit(&varBusType);
        hr = pProps->GetValue(PKEY_Device_BusTypeGuid, &varBusType);
        if (SUCCEEDED(hr) && varBusType.vt == VT_LPWSTR) {
            std::wstring busType = varBusType.pwszVal;
            std::transform(busType.begin(), busType.end(), busType.begin(), ::towupper);

            // Bluetooth bus type GUID: {2bd67d8b-8beb-48d5-87e0-6cda3428040a}
            if (busType.find(L"2BD67D8B-8BEB-48D5-87E0-6CDA3428040A") != std::wstring::npos) {
                isBluetooth = true;
            }
        }
        PropVariantClear(&varBusType);
    }

    // Method 7: Fallback to device friendly name patterns (least reliable, but catches edge cases)
    if (!isBluetooth) {
        PROPVARIANT varFriendlyName;
        PropVariantInit(&varFriendlyName);
        hr = pProps->GetValue(PKEY_DeviceInterface_FriendlyName, &varFriendlyName);
        if (FAILED(hr)) {
            // Try device description if friendly name isn't available
            hr = pProps->GetValue(PKEY_Device_DeviceDesc, &varFriendlyName);
        }
        if (SUCCEEDED(hr) && varFriendlyName.vt == VT_LPWSTR) {
            std::wstring deviceName = varFriendlyName.pwszVal;
            std::transform(deviceName.begin(), deviceName.end(), deviceName.begin(), ::towupper);
            // More comprehensive pattern matching for Bluetooth indicators
            isBluetooth = (deviceName.find(L"BLUETOOTH") != std::wstring::npos ||
                          deviceName.find(L"HANDS-FREE") != std::wstring::npos ||
                          deviceName.find(L"A2DP") != std::wstring::npos ||
                          deviceName.find(L"HFP") != std::wstring::npos ||
                          deviceName.find(L"HSP") != std::wstring::npos ||
                          deviceName.find(L"AVRCP") != std::wstring::npos ||
                          deviceName.find(L"AIRPODS") != std::wstring::npos ||
                          deviceName.find(L"WIRELESS HEADSET") != std::wstring::npos ||
                          deviceName.find(L"BT ") != std::wstring::npos ||
                          (deviceName.find(L"WIRELESS") != std::wstring::npos &&
                           deviceName.find(L"AUDIO") != std::wstring::npos));
        }
        PropVariantClear(&varFriendlyName);
    }

    pProps->Release();
    return isBluetooth;
}

// Helper function to check if device has any sessions
static bool HasAnySessions(IMMDevice* pDevice) {
    IAudioSessionManager2* pSessionManager = nullptr;
    HRESULT hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&pSessionManager);
    if (FAILED(hr)) return false;

    IAudioSessionEnumerator* pSessionEnum = nullptr;
    hr = pSessionManager->GetSessionEnumerator(&pSessionEnum);
    if (FAILED(hr)) {
        pSessionManager->Release();
        return false;
    }

    int sessionCount = 0;
    pSessionEnum->GetCount(&sessionCount);

    pSessionEnum->Release();
    pSessionManager->Release();

    return sessionCount > 0;
}

// Helper function to check session activity with Bluetooth-specific logic
static bool CheckSessionsForActivity(IMMDevice* pDevice, bool isBluetooth = false) {
    IAudioSessionManager2* pSessionManager = nullptr;
    HRESULT hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&pSessionManager);
    if (FAILED(hr)) return false;

    IAudioSessionEnumerator* pSessionEnum = nullptr;
    hr = pSessionManager->GetSessionEnumerator(&pSessionEnum);
    if (FAILED(hr)) {
        pSessionManager->Release();
        return false;
    }

    int sessionCount = 0;
    pSessionEnum->GetCount(&sessionCount);

    bool hasActiveSessions = false;
    for (int i = 0; i < sessionCount; i++) {
        IAudioSessionControl* pSessionControl = nullptr;
        hr = pSessionEnum->GetSession(i, &pSessionControl);
        if (FAILED(hr)) continue;

        // Check session state
        AudioSessionState state;
        pSessionControl->GetState(&state);

        // Check session volume
        ISimpleAudioVolume* pVolume = nullptr;
        hr = pSessionControl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&pVolume);
        if (SUCCEEDED(hr)) {
            float sessionVolume = 0.0f;
            pVolume->GetMasterVolume(&sessionVolume);
            BOOL isMuted = FALSE;
            pVolume->GetMute(&isMuted);

            if (isBluetooth) {
                // For Bluetooth devices, be more permissive but still apply some filtering
                // Accept sessions that are active and have some volume (even very low) and not muted
                if (state == AudioSessionStateActive && sessionVolume > 0.001f && !isMuted) {
                    hasActiveSessions = true;
                }
                // Also accept sessions that are active even with zero volume if not muted
                // (some Bluetooth drivers report zero volume initially)
                else if (state == AudioSessionStateActive && !isMuted) {
                    hasActiveSessions = true;
                }
            } else {
                // Standard logic for non-Bluetooth devices
                if (state == AudioSessionStateActive && sessionVolume > 0.0f && !isMuted) {
                    hasActiveSessions = true;
                }
            }
            pVolume->Release();
        }
        pSessionControl->Release();

        if (hasActiveSessions) break;
    }

    pSessionEnum->Release();
    pSessionManager->Release();
    return hasActiveSessions;
}

// Enhanced function to check if device has active audio with debouncing
bool HasActiveAudio(IMMDevice* pDevice) {
    bool isBluetooth = IsBluetoothDevice(pDevice);
    std::wstring deviceId = GetDeviceId(pDevice);
    bool hasActiveAudio = false;
    HRESULT hr;

    // Method 1: Check peak value
    IAudioMeterInformation* pMeter = nullptr;
    hr = pDevice->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, nullptr, (void**)&pMeter);
    if (SUCCEEDED(hr)) {
        float peakValue = 0.0f;
        pMeter->GetPeakValue(&peakValue);
        if (peakValue > 0.0f) hasActiveAudio = true;
        pMeter->Release();
    }

    // Method 2: Check padding (buffer activity)
    if (!hasActiveAudio) {
        IAudioClient* pAudioClient = nullptr;
        hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
        if (SUCCEEDED(hr)) {
            UINT32 padding = 0;
            hr = pAudioClient->GetCurrentPadding(&padding);
            if (SUCCEEDED(hr) && padding > 0) hasActiveAudio = true;
            pAudioClient->Release();
        }
    }

    // Method 3: Check active sessions with device-specific logic
    if (!hasActiveAudio) {
        hasActiveAudio = CheckSessionsForActivity(pDevice, isBluetooth);
    }

    // Method 4: Enhanced Bluetooth-specific debouncing and power management
    if (isBluetooth && !deviceId.empty()) {
        auto& state = bluetoothStateCache[deviceId];
        auto now = std::chrono::steady_clock::now();
        auto timeSinceLastChange = std::chrono::duration_cast<std::chrono::milliseconds>(now - state.lastStateChange).count();
        auto timeSinceLastActivity = std::chrono::duration_cast<std::chrono::milliseconds>(now - state.lastActivityTime).count();
        auto timeSinceRapidWindow = std::chrono::duration_cast<std::chrono::milliseconds>(now - state.rapidStateChangeWindow).count();

        // Reset rapid change counter if outside window
        if (timeSinceRapidWindow > RAPID_CHANGE_WINDOW_MS) {
            state.rapidStateChangeCount = 0;
            state.rapidStateChangeWindow = now;
        }

        // Update activity timestamp if any activity detected
        if (hasActiveAudio) {
            state.lastActivityTime = now;
        }

        // Determine current effective debounce time (extended if device is flapping)
        int effectiveDebounceMs = (state.rapidStateChangeCount >= MAX_RAPID_CHANGES) ?
                                 EXTENDED_DEBOUNCE_MS : BLUETOOTH_DEBOUNCE_MS;

        if (hasActiveAudio != state.lastActiveState) {
            // State change detected - increment rapid change counter
            state.rapidStateChangeCount++;
            if (hasActiveAudio) {
                // Going from inactive to active
                state.consecutiveActiveChecks++;
                state.consecutiveInactiveChecks = 0;
                // Require fewer consecutive checks for faster response to genuine activity
                if (state.consecutiveActiveChecks >= REQUIRED_CONSECUTIVE_ACTIVE_CHECKS) {
                    state.lastActiveState = true;
                    state.lastReportedState = true;
                    state.lastStateChange = now;
                    state.lastReportTime = now;
                    return true;
                }
                // Still building confidence - return last reported state during probation
                return state.lastReportedState;
            } else {
                // Going from active to inactive
                state.consecutiveInactiveChecks++;
                state.consecutiveActiveChecks = 0;
                // Apply power management holdoff - stay active if recent activity
                if (timeSinceLastActivity < BLUETOOTH_ACTIVE_HOLD_MS) {
                    return true; // Keep reporting active due to recent activity
                }
                // Apply debouncing with flapping detection
                if (timeSinceLastChange < effectiveDebounceMs) {
                    return state.lastReportedState; // Stay in previous reported state during debounce
                }
                // Require more consecutive inactive checks to confirm device is truly idle
                if (state.consecutiveInactiveChecks >= REQUIRED_CONSECUTIVE_INACTIVE_CHECKS) {
                    state.lastActiveState = false;
                    state.lastReportedState = false;
                    state.lastStateChange = now;
                    state.lastReportTime = now;
                    return false;
                }
                // Still building confidence for inactive state
                return state.lastReportedState;
            }
        } else {
            // State consistent with previous check
            if (hasActiveAudio) {
                // Consistently active - reset inactive counter
                state.consecutiveInactiveChecks = 0;
                state.consecutiveActiveChecks = std::min(state.consecutiveActiveChecks + 1, REQUIRED_CONSECUTIVE_ACTIVE_CHECKS + 1);

                // Ensure we report active if consistently seeing activity
                if (state.consecutiveActiveChecks >= REQUIRED_CONSECUTIVE_ACTIVE_CHECKS && !state.lastReportedState) {
                    state.lastReportedState = true;
                    state.lastReportTime = now;
                }
                return state.lastReportedState;
            } else {
                // Consistently inactive - but check power management holdoff
                if (timeSinceLastActivity < BLUETOOTH_ACTIVE_HOLD_MS) {
                    return true; // Keep active due to recent activity
                }
                state.consecutiveActiveChecks = 0;
                state.consecutiveInactiveChecks = std::min(state.consecutiveInactiveChecks + 1, REQUIRED_CONSECUTIVE_INACTIVE_CHECKS + 1);

                return state.lastReportedState;
            }
        }
    }

    return hasActiveAudio;
}

// New function with structured result
AudioProcessResult GetProcessesAccessingMicrophoneWithResult() {
    AudioProcessResult result;
    std::unordered_set<std::string> seen;  // Track unique strings
    HRESULT hr = CoInitialize(nullptr);

    if (FAILED(hr)) {
        result.errorCode = hr;
        result.errorMessage = "Failed to initialize COM";
        result.success = false;
        return result;
    }

    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDevice* pDevice = nullptr;
    IAudioSessionManager2* pSessionManager = nullptr;
    IAudioSessionEnumerator* pSessionEnum = nullptr;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) {
        result.errorCode = hr;
        result.errorMessage = "Failed to create device enumerator";
        result.success = false;
        CoUninitialize();
        return result;
    }

    // Get default capture (microphone) device
    hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eMultimedia, &pDevice);
    if (FAILED(hr)) {
        result.errorCode = hr;
        result.errorMessage = "Failed to get default audio endpoint";
        result.success = false;
        pEnumerator->Release();
        CoUninitialize();
        return result;
    }

    bool isPeakValueActive = false;
    IAudioMeterInformation* pMeter = nullptr;

    // Get the Audio Meter Interface
    hr = pDevice->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, nullptr, (void**)&pMeter);
    if (SUCCEEDED(hr)) {
        float peakValue = 0.0f;
        pMeter->GetPeakValue(&peakValue);
        isPeakValueActive = (peakValue > 0.0f);
        pMeter->Release();
    }

    bool isPaddingActive = false;
    IAudioClient* pAudioClient = nullptr;
    hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
    if (SUCCEEDED(hr)) {
        UINT32 padding = 0;
        hr = pAudioClient->GetCurrentPadding(&padding);
        isPaddingActive = SUCCEEDED(hr) && padding > 0;
        pAudioClient->Release();
    }

    if (!isPeakValueActive && !isPaddingActive) {
        pDevice->Release();
        pEnumerator->Release();
        CoUninitialize();
        // This is not an error - just no active audio
        result.processes = std::vector<std::string>();
        return result;
    }

    // Get session manager
    hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&pSessionManager);
    if (FAILED(hr)) {
        result.errorCode = hr;
        result.errorMessage = "Failed to activate IAudioSessionManager2";
        result.success = false;
        pDevice->Release();
        pEnumerator->Release();
        CoUninitialize();
        return result;
    }

    // Get audio session enumerator
    hr = pSessionManager->GetSessionEnumerator(&pSessionEnum);
    if (FAILED(hr)) {
        result.errorCode = hr;
        result.errorMessage = "Failed to get IAudioSessionEnumerator";
        result.success = false;
        pSessionManager->Release();
        pDevice->Release();
        pEnumerator->Release();
        CoUninitialize();
        return result;
    }

    int sessionCount = 0;
    pSessionEnum->GetCount(&sessionCount);

    for (int i = 0; i < sessionCount; i++) {
        IAudioSessionControl* pSessionControl = nullptr;
        hr = pSessionEnum->GetSession(i, &pSessionControl);
        if (FAILED(hr)) continue;

        IAudioSessionControl2* pSessionControl2 = nullptr;
        hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pSessionControl2);
        if (SUCCEEDED(hr)) {
            DWORD processID = 0;
            pSessionControl2->GetProcessId(&processID);

            AudioSessionState state;
            pSessionControl2->GetState(&state);

            if (processID != 0 && state == AudioSessionStateActive) {
                std::string processPath = GetProcessExecutablePath(processID);
                
                // Only insert if not already seen
                if (seen.insert(processPath).second) {
                    result.processes.push_back(processPath);
                }
            }
            pSessionControl2->Release();
        }
        pSessionControl->Release();
    }

    // Cleanup
    pSessionEnum->Release();
    pSessionManager->Release();
    pDevice->Release();
    pEnumerator->Release();
    CoUninitialize();

    return result;
}

std::vector<std::string> GetAudioInputProcesses() {
    std::vector<std::string> results;
    std::unordered_set<std::string> seen;  // Track unique strings
    HRESULT hr = CoInitialize(nullptr);

    if (FAILED(hr)) {
        return results;
    }

    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDevice* pDevice = nullptr;
    IAudioSessionManager2* pSessionManager = nullptr;
    IAudioSessionEnumerator* pSessionEnum = nullptr;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) {
        return results;
    }

    // Get default capture (microphone) device
    hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eMultimedia, &pDevice);
    if (FAILED(hr)) {
        pEnumerator->Release();
        return results;
    }

    bool isActive = false;
    IAudioMeterInformation* pMeter = nullptr;

    // Get the Audio Meter Interface
    hr = pDevice->Activate(__uuidof(IAudioMeterInformation), CLSCTX_ALL, nullptr, (void**)&pMeter);
    if (SUCCEEDED(hr)) {
        float peakValue = 0.0f;
        pMeter->GetPeakValue(&peakValue);
        isActive = (peakValue > 0.0f);
        pMeter->Release();
    }

    if (!isActive) {
        pDevice->Release();
        pEnumerator->Release();
        CoUninitialize();
        return results;
    }

    // Get session manager
    hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&pSessionManager);
    if (FAILED(hr)) {
        std::cerr << "Failed to activate IAudioSessionManager2. HRESULT: " << std::hex << hr << std::endl;
        pDevice->Release();
        pEnumerator->Release();
        CoUninitialize();
        return results;
    }

    // Get audio session enumerator
    hr = pSessionManager->GetSessionEnumerator(&pSessionEnum);
    if (FAILED(hr)) {
        std::cerr << "Failed to get IAudioSessionEnumerator. HRESULT: " << std::hex << hr << std::endl;
        pSessionManager->Release();
        pDevice->Release();
        pEnumerator->Release();
        CoUninitialize();
        return results;
    }

    int sessionCount = 0;
    pSessionEnum->GetCount(&sessionCount);

    for (int i = 0; i < sessionCount; i++) {
        IAudioSessionControl* pSessionControl = nullptr;
        hr = pSessionEnum->GetSession(i, &pSessionControl);
        if (FAILED(hr)) continue;

        IAudioSessionControl2* pSessionControl2 = nullptr;
        hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pSessionControl2);
        if (SUCCEEDED(hr)) {
            DWORD processID = 0;
            pSessionControl2->GetProcessId(&processID);

            AudioSessionState state;
            pSessionControl2->GetState(&state);

            if (processID != 0 && state == AudioSessionStateActive) {
                std::string processPath = GetProcessExecutablePath(processID);

                // Only insert if not already seen
                if (seen.insert(processPath).second) {
                    results.push_back(processPath);
                }
            }
            pSessionControl2->Release();
        }
        pSessionControl->Release();
    }

    // Cleanup
    pSessionEnum->Release();
    pSessionManager->Release();
    pDevice->Release();
    pEnumerator->Release();
    CoUninitialize();

    return results;
}

// Speaker/render process detection - separate from microphone monitoring
RenderProcessResult GetRenderProcessesWithResult() {
    RenderProcessResult result;
    HRESULT hr = CoInitialize(nullptr);

    if (FAILED(hr)) {
        result.errorCode = hr;
        result.errorMessage = "Failed to initialize COM";
        result.success = false;
        return result;
    }

    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDeviceCollection* pCollection = nullptr;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) {
        result.errorCode = hr;
        result.errorMessage = "Failed to create device enumerator";
        result.success = false;
        CoUninitialize();
        return result;
    }

    // Get ALL active render (speaker) devices
    hr = pEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr)) {
        result.errorCode = hr;
        result.errorMessage = "Failed to enumerate audio endpoints";
        result.success = false;
        pEnumerator->Release();
        CoUninitialize();
        return result;
    }

    UINT deviceCount = 0;
    pCollection->GetCount(&deviceCount);

    // Check each active render device
    for (UINT deviceIndex = 0; deviceIndex < deviceCount; deviceIndex++) {
        IMMDevice* pDevice = nullptr;
        hr = pCollection->Item(deviceIndex, &pDevice);
        if (FAILED(hr)) continue;

        // Get device name
        std::string deviceName = "Unknown Device";
        IPropertyStore* pProps = nullptr;
        hr = pDevice->OpenPropertyStore(STGM_READ, &pProps);
        if (SUCCEEDED(hr)) {
            PROPVARIANT varName;
            PropVariantInit(&varName);
            hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);

            if (SUCCEEDED(hr) && varName.vt == VT_LPWSTR) {
                std::wstring wDeviceName(varName.pwszVal);
                deviceName.resize(wDeviceName.size() * 4);
                int bytesWritten = WideCharToMultiByte(CP_UTF8, 0, wDeviceName.c_str(), -1,
                                                     &deviceName[0], deviceName.size(), nullptr, nullptr);
                if (bytesWritten > 0) {
                    deviceName.resize(bytesWritten - 1);
                }
            }

            PropVariantClear(&varName);
            pProps->Release();
        }

        IAudioSessionManager2* pSessionManager = nullptr;
        hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&pSessionManager);
        if (SUCCEEDED(hr)) {
            IAudioSessionEnumerator* pSessionEnum = nullptr;
            hr = pSessionManager->GetSessionEnumerator(&pSessionEnum);
            if (SUCCEEDED(hr)) {
                int sessionCount = 0;
                pSessionEnum->GetCount(&sessionCount);

                for (int i = 0; i < sessionCount; i++) {
                    IAudioSessionControl* pSessionControl = nullptr;
                    hr = pSessionEnum->GetSession(i, &pSessionControl);
                    if (FAILED(hr)) continue;

                    IAudioSessionControl2* pSessionControl2 = nullptr;
                    hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pSessionControl2);
                    if (SUCCEEDED(hr)) {
                        DWORD processId = 0;
                        pSessionControl2->GetProcessId(&processId);

                        AudioSessionState state;
                        pSessionControl2->GetState(&state);

                        // Check if session is active and not muted
                        ISimpleAudioVolume* pVolume = nullptr;
                        bool isActiveSession = false;
                        hr = pSessionControl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&pVolume);
                        if (SUCCEEDED(hr)) {
                            BOOL isMuted = FALSE;
                            pVolume->GetMute(&isMuted);

                            // For render sessions, active state + not muted = active
                            isActiveSession = (state == AudioSessionStateActive && !isMuted);
                            pVolume->Release();
                        }

                        if (processId != 0 && isActiveSession) {
                            RenderProcessInfo info;
                            info.processId = processId;
                            info.processName = GetProcessExecutablePath(processId);

                            // Extract filename from path
                            size_t lastSlash = info.processName.find_last_of("\\");
                            if (lastSlash != std::string::npos) {
                                info.processName = info.processName.substr(lastSlash + 1);
                            }

                            info.deviceName = deviceName;
                            info.isActive = true;
                            result.processes.push_back(info);
                        }

                        pSessionControl2->Release();
                    }
                    pSessionControl->Release();
                }
                pSessionEnum->Release();
            }
            pSessionManager->Release();
        }

        pDevice->Release();
    }

    // Cleanup
    pCollection->Release();
    pEnumerator->Release();
    CoUninitialize();

    return result;
}

// New debounced function with structured result using enumeration of all devices
AudioProcessResult GetProcessAccessMicrophoneDebouncedWithResult() {
    AudioProcessResult result;
    std::unordered_set<std::string> seen;
    HRESULT hr = CoInitialize(nullptr);

    if (FAILED(hr)) {
        result.errorCode = hr;
        result.errorMessage = "Failed to initialize COM";
        result.success = false;
        return result;
    }

    IMMDeviceEnumerator* pEnumerator = nullptr;
    IMMDeviceCollection* pCollection = nullptr;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) {
        result.errorCode = hr;
        result.errorMessage = "Failed to create device enumerator";
        result.success = false;
        CoUninitialize();
        return result;
    }

    // Get ALL active capture devices instead of just default
    hr = pEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &pCollection);
    if (FAILED(hr)) {
        result.errorCode = hr;
        result.errorMessage = "Failed to enumerate audio endpoints";
        result.success = false;
        pEnumerator->Release();
        CoUninitialize();
        return result;
    }

    UINT deviceCount = 0;
    pCollection->GetCount(&deviceCount);

    // Check each active capture device
    for (UINT deviceIndex = 0; deviceIndex < deviceCount; deviceIndex++) {
        IMMDevice* pDevice = nullptr;
        hr = pCollection->Item(deviceIndex, &pDevice);
        if (FAILED(hr)) continue;

        // Use enhanced activity detection with debouncing
        if (HasActiveAudio(pDevice)) {
            IAudioSessionManager2* pSessionManager = nullptr;
            hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&pSessionManager);
            if (SUCCEEDED(hr)) {
                IAudioSessionEnumerator* pSessionEnum = nullptr;
                hr = pSessionManager->GetSessionEnumerator(&pSessionEnum);
                if (SUCCEEDED(hr)) {
                    int sessionCount = 0;
                    pSessionEnum->GetCount(&sessionCount);

                    for (int i = 0; i < sessionCount; i++) {
                        IAudioSessionControl* pSessionControl = nullptr;
                        hr = pSessionEnum->GetSession(i, &pSessionControl);
                        if (FAILED(hr)) continue;

                        IAudioSessionControl2* pSessionControl2 = nullptr;
                        hr = pSessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pSessionControl2);
                        if (SUCCEEDED(hr)) {
                            DWORD processID = 0;
                            pSessionControl2->GetProcessId(&processID);

                            AudioSessionState state;
                            pSessionControl2->GetState(&state);

                            if (processID != 0 && state == AudioSessionStateActive) {
                                std::string processPath = GetProcessExecutablePath(processID);

                                // Only insert if not already seen
                                if (seen.insert(processPath).second) {
                                    result.processes.push_back(processPath);
                                }
                            }
                            pSessionControl2->Release();
                        }
                        pSessionControl->Release();
                    }
                    pSessionEnum->Release();
                }
                pSessionManager->Release();
            }
        }

        pDevice->Release();
    }

    // Cleanup
    pCollection->Release();
    pEnumerator->Release();
    CoUninitialize();

    return result;
}