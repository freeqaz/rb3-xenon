#pragma once

#include <GLFW/glfw3.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

namespace milo {

struct CrkdGuitarState {
    enum Button : unsigned {
        Green = 1u << 0, Red = 1u << 1, Yellow = 1u << 2, Blue = 1u << 3,
        Orange = 1u << 4, Overdrive = 1u << 5, Start = 1u << 6,
        Up = 1u << 7, Right = 1u << 8, Down = 1u << 9, Left = 1u << 10,
        Tilt = 1u << 11, Select = 1u << 12
    };

    bool connected = false;
    unsigned buttons = 0;
    float whammy = 0.0f; // 0 at rest, 1 fully pressed
};

inline CrkdGuitarState DecodeCrkdGuitar(const unsigned char *buttons, unsigned char hat, float z) {
    using Guitar = CrkdGuitarState;
    Guitar state;
    state.connected = true;
    // CRKD Guitar LM in PS3 mode. Indices are zero-based HID button usages.
    if (buttons[1]) state.buttons |= Guitar::Green;
    if (buttons[2]) state.buttons |= Guitar::Red;
    if (buttons[0]) state.buttons |= Guitar::Yellow;
    if (buttons[3]) state.buttons |= Guitar::Blue;
    if (buttons[4]) state.buttons |= Guitar::Orange;
    if (buttons[5]) state.buttons |= Guitar::Tilt | Guitar::Overdrive;
    if (buttons[8]) state.buttons |= Guitar::Select | Guitar::Overdrive;
    if (buttons[9]) state.buttons |= Guitar::Start;
    if (hat & GLFW_HAT_UP) state.buttons |= Guitar::Up;
    if (hat & GLFW_HAT_RIGHT) state.buttons |= Guitar::Right;
    if (hat & GLFW_HAT_DOWN) state.buttons |= Guitar::Down;
    if (hat & GLFW_HAT_LEFT) state.buttons |= Guitar::Left;
    // Z rests at the axis centre. A stick-2 click (Start) can move it by
    // about 12%, so reserve a small dead zone and remap the remaining range.
    constexpr float deadZone = 0.14f;
    state.whammy = std::clamp((z - deadZone) / (1.0f - deadZone), 0.0f, 1.0f);
    return state;
}

template <size_t PadCount>
class CrkdGuitars {
public:
    CrkdGuitars() { mJoysticks.fill(kUnassigned); }

    void Poll() {
        // Keep a connected guitar in its pad slot when another one is unplugged.
        for (int &id : mJoysticks) {
            if (id != kUnassigned && !IsCrkdGuitar(id))
                id = kUnassigned;
        }
        for (int id = GLFW_JOYSTICK_1; id <= GLFW_JOYSTICK_LAST; ++id) {
            const auto empty = std::find(mJoysticks.begin(), mJoysticks.end(), kUnassigned);
            if (empty == mJoysticks.end()) break;
            if (std::find(mJoysticks.begin(), mJoysticks.end(), id) != mJoysticks.end()) continue;
            if (!IsCrkdGuitar(id)) continue;
            *empty = id;
            std::fprintf(stderr, "CRKD Guitar LM: joystick %d assigned to pad %td\n",
                         id, empty - mJoysticks.begin());
        }

        for (size_t pad = 0; pad < PadCount; ++pad) {
            const int id = mJoysticks[pad];
            mStates[pad] = id == kUnassigned ? CrkdGuitarState{} : Read(id);
        }
    }

    const CrkdGuitarState &operator[](size_t pad) const { return mStates[pad]; }

private:
    static constexpr int kUnassigned = GLFW_JOYSTICK_LAST + 1;

    static bool IsCrkdGuitar(int id) {
        if (!glfwJoystickPresent(id)) return false;
        const char *name = glfwGetJoystickName(id);
        const char *guid = glfwGetJoystickGUID(id);
        // Cocoa GUID: USB, followed by the little-endian vendor and product.
        // 12BA:0100 is shared with older guitars that swap blue and yellow.
        return name && std::strstr(name, "CRKD Guitar LM") && guid &&
               std::strncmp(guid, "03000000ba12000000010000", 24) == 0;
    }

    static CrkdGuitarState Read(int id) {
        int buttonCount = 0, hatCount = 0, axisCount = 0;
        const auto *buttons = glfwGetJoystickButtons(id, &buttonCount);
        const auto *hats = glfwGetJoystickHats(id, &hatCount);
        const auto *axes = glfwGetJoystickAxes(id, &axisCount);
        if (!buttons || buttonCount < 10 || !hats || hatCount < 1 || !axes || axisCount < 3)
            return {};
        // Cocoa enumerates X, Y, Z and Rz for this HID descriptor. Use only Z.
        return DecodeCrkdGuitar(buttons, hats[0], axes[2]);
    }

    std::array<int, PadCount> mJoysticks;
    std::array<CrkdGuitarState, PadCount> mStates{};
};

} // namespace milo
