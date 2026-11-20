#pragma once

#include <windows.h>
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>
#include <wrl/client.h>
#include <bitset>
#include <cstddef>
#include <string>
#include <vector>

namespace ShaderToggler {

class ControllerInput {
public:
    static constexpr size_t ButtonCount = 128;
    static constexpr size_t ControlCount = ButtonCount + 4;
    using Controls = std::bitset<ControlCount>;

    struct Device {
        GUID guid;
        std::string id;
        std::string name;
        Microsoft::WRL::ComPtr<IDirectInputDevice8W> input;
        Controls down;
        Controls pressed;
        bool armed = false;
    };

    void Poll(HWND window, bool needed);
    bool Pressed(const std::string& device, unsigned control) const;
    const std::vector<Device>& Devices() const { return devices; }
    void LearnFor(int group);
    bool Learning() const { return learnUntil > GetTickCount64(); }
    void StopLearning() { learnUntil = 0; WaitForRelease(); }
    void WaitForRelease();
    std::string selectedDevice;
    static std::string ControlName(unsigned control);

private:
    static BOOL CALLBACK Enumerate(const DIDEVICEINSTANCEW* instance, void* context);
    Microsoft::WRL::ComPtr<IDirectInput8W> input;
    std::vector<Device> devices;
    HWND window = nullptr;
    ULONGLONG nextScan = 0;
    ULONGLONG learnUntil = 0;
    int learnGroup = -1;
};
}
