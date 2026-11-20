#include "ControllerInput.h"
#include <algorithm>
#include <objbase.h>
#include <utility>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "ole32.lib")

namespace ShaderToggler {

static std::string Utf8(const wchar_t* text) {
    const int length = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1)
        return {};
    std::string result(length, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), length, nullptr, nullptr);
    result.pop_back();
    return result;
}

BOOL CALLBACK ControllerInput::Enumerate(const DIDEVICEINSTANCEW* instance, void* context) {
    auto& owner = *static_cast<ControllerInput*>(context);
    if (std::any_of(owner.devices.begin(), owner.devices.end(), [&](const Device& device) {
        return IsEqualGUID(device.guid, instance->guidInstance);
    }))
        return DIENUM_CONTINUE;

    Device device{};
    device.guid = instance->guidInstance;
    if (FAILED(owner.input->CreateDevice(device.guid, device.input.GetAddressOf(), nullptr)) ||
        FAILED(device.input->SetDataFormat(&c_dfDIJoystick2)) ||
        FAILED(device.input->SetCooperativeLevel(owner.window, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND)))
        return DIENUM_CONTINUE;

    wchar_t guid[39]{};
    if (!StringFromGUID2(device.guid, guid, ARRAYSIZE(guid)))
        return DIENUM_CONTINUE;
    device.id = Utf8(guid);
    device.name = Utf8(instance->tszInstanceName);
    owner.devices.push_back(std::move(device));
    return DIENUM_CONTINUE;
}

void ControllerInput::Poll(HWND target, bool needed) {
    target = GetAncestor(target, GA_ROOT);
    if (window != target || !needed || !target) {
        devices.clear();
        input.Reset();
        nextScan = 0;
        window = target;
        if (!needed || !target)
            return;
    }

    if (GetForegroundWindow() != window) {
        for (auto& device : devices) {
            device.input->Unacquire();
        }
        WaitForRelease();
        return;
    }

    const auto now = GetTickCount64();
    if (now >= nextScan) {
        nextScan = now + 2000;
        if (!input) {
            const HRESULT result = DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION,
                IID_IDirectInput8W, reinterpret_cast<void**>(input.GetAddressOf()), nullptr);
            if (FAILED(result))
                return;
        }
        input->EnumDevices(DI8DEVCLASS_GAMECTRL, Enumerate, this, DIEDFL_ATTACHEDONLY);
    }

    std::erase_if(devices, [](Device& device) {
        device.pressed.reset();
        DIJOYSTATE2 state{};
        HRESULT result = device.input->Poll();
        if (SUCCEEDED(result))
            result = device.input->GetDeviceState(sizeof(state), &state);
        if (result == DIERR_NOTACQUIRED || result == DIERR_INPUTLOST) {
            device.armed = false;
            result = device.input->Acquire();
            if (SUCCEEDED(result)) {
                result = device.input->Poll();
                if (SUCCEEDED(result))
                    result = device.input->GetDeviceState(sizeof(state), &state);
            }
        }
        if (FAILED(result)) {
            device.armed = false;
            return result == DIERR_UNPLUGGED || result == DIERR_DEVICENOTREG;
        }

        Controls down;
        for (size_t index = 0; index < ButtonCount; ++index)
            down[index] = (state.rgbButtons[index] & 0x80) != 0;
        const DWORD pov = state.rgdwPOV[0];
        if (pov < 36000) {
            down[ButtonCount] = pov >= 31500 || pov <= 4500;
            down[ButtonCount + 1] = pov >= 4500 && pov <= 13500;
            down[ButtonCount + 2] = pov >= 13500 && pov <= 22500;
            down[ButtonCount + 3] = pov >= 22500 && pov <= 31500;
        }

        // After reconnect or focus changes, wait for release before firing.
        if (device.armed)
            device.pressed = down & ~device.down;
        else
            device.armed = down.none();
        device.down = down;
        return false;
    });
}

bool ControllerInput::Pressed(const std::string& id, unsigned control) const {
    if (control >= ControlCount)
        return false;
    return std::any_of(devices.begin(), devices.end(), [&](const Device& device) {
        return device.id == id && device.pressed[control];
    });
}

void ControllerInput::WaitForRelease() {
    for (auto& device : devices) {
        device.pressed.reset();
        device.armed = false;
    }
}

void ControllerInput::LearnFor(int group) {
    if (learnGroup != group || !Learning())
        WaitForRelease();
    learnGroup = group;
    learnUntil = GetTickCount64() + 1000;
}

std::string ControllerInput::ControlName(unsigned control) {
    if (control < ButtonCount)
        return "Button " + std::to_string(control + 1);
    constexpr const char* directions[] = {"D-pad Up", "D-pad Right", "D-pad Down", "D-pad Left"};
    return control < ControlCount ? directions[control - ButtonCount] : "Unassigned";
}
}
