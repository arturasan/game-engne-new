#pragma once

#include <array>
#include <cstdint>

#include "engine/platform/types.hpp"

namespace engine {

enum class Key : std::uint16_t {
    Unknown = 0,
    A = 4,
    B = 5,
    C = 6,
    D = 7,
    E = 8,
    F = 9,
    G = 10,
    H = 11,
    I = 12,
    J = 13,
    K = 14,
    L = 15,
    M = 16,
    N = 17,
    O = 18,
    P = 19,
    Q = 20,
    R = 21,
    S = 22,
    T = 23,
    U = 24,
    V = 25,
    W = 26,
    X = 27,
    Y = 28,
    Z = 29,
    Num1 = 30,
    Num2 = 31,
    Num3 = 32,
    Num4 = 33,
    Num5 = 34,
    Num6 = 35,
    Num7 = 36,
    Num8 = 37,
    Num9 = 38,
    Num0 = 39,
    Return = 40,
    Escape = 41,
    Backspace = 42,
    Tab = 43,
    Space = 44,
    Minus = 45,
    Equals = 46,
    LeftBracket = 47,
    RightBracket = 48,
    Backslash = 49,
    NonUsHash = 50,
    Semicolon = 51,
    Apostrophe = 52,
    Grave = 53,
    Comma = 54,
    Period = 55,
    Slash = 56,
    CapsLock = 57,
    F1 = 58,
    F2 = 59,
    F3 = 60,
    F4 = 61,
    F5 = 62,
    F6 = 63,
    F7 = 64,
    F8 = 65,
    F9 = 66,
    F10 = 67,
    F11 = 68,
    F12 = 69,
    PrintScreen = 70,
    ScrollLock = 71,
    Pause = 72,
    Insert = 73,
    Home = 74,
    PageUp = 75,
    Delete = 76,
    End = 77,
    PageDown = 78,
    Right = 79,
    Left = 80,
    Down = 81,
    Up = 82,
    NumLockClear = 83,
    KeypadDivide = 84,
    KeypadMultiply = 85,
    KeypadMinus = 86,
    KeypadPlus = 87,
    KeypadEnter = 88,
    Keypad1 = 89,
    Keypad2 = 90,
    Keypad3 = 91,
    Keypad4 = 92,
    Keypad5 = 93,
    Keypad6 = 94,
    Keypad7 = 95,
    Keypad8 = 96,
    Keypad9 = 97,
    Keypad0 = 98,
    KeypadPeriod = 99,
    NonUsBackslash = 100,
    Application = 101,
    Power = 102,
    KeypadEquals = 103,
    F13 = 104,
    F14 = 105,
    F15 = 106,
    F16 = 107,
    F17 = 108,
    F18 = 109,
    F19 = 110,
    F20 = 111,
    F21 = 112,
    F22 = 113,
    F23 = 114,
    F24 = 115,
    Execute = 116,
    Help = 117,
    Menu = 118,
    Select = 119,
    Stop = 120,
    Again = 121,
    Undo = 122,
    Cut = 123,
    Copy = 124,
    Paste = 125,
    Find = 126,
    Mute = 127,
    VolumeUp = 128,
    VolumeDown = 129,
    KeypadComma = 133,
    KeypadEqualsAs400 = 134,
    International1 = 135,
    International2 = 136,
    International3 = 137,
    International4 = 138,
    International5 = 139,
    International6 = 140,
    International7 = 141,
    International8 = 142,
    International9 = 143,
    Lang1 = 144,
    Lang2 = 145,
    Lang3 = 146,
    Lang4 = 147,
    Lang5 = 148,
    Lang6 = 149,
    Lang7 = 150,
    Lang8 = 151,
    Lang9 = 152,
    AltErase = 153,
    SysReq = 154,
    Cancel = 155,
    Clear = 156,
    Prior = 157,
    Return2 = 158,
    Separator = 159,
    Out = 160,
    Oper = 161,
    ClearAgain = 162,
    CrSel = 163,
    ExSel = 164,
    Keypad00 = 176,
    Keypad000 = 177,
    ThousandsSeparator = 178,
    DecimalSeparator = 179,
    CurrencyUnit = 180,
    CurrencySubUnit = 181,
    KeypadLeftParen = 182,
    KeypadRightParen = 183,
    KeypadLeftBrace = 184,
    KeypadRightBrace = 185,
    KeypadTab = 186,
    KeypadBackspace = 187,
    KeypadA = 188,
    KeypadB = 189,
    KeypadC = 190,
    KeypadD = 191,
    KeypadE = 192,
    KeypadF = 193,
    KeypadXor = 194,
    KeypadPower = 195,
    KeypadPercent = 196,
    KeypadLess = 197,
    KeypadGreater = 198,
    KeypadAmpersand = 199,
    KeypadDblAmpersand = 200,
    KeypadVerticalBar = 201,
    KeypadDblVerticalBar = 202,
    KeypadColon = 203,
    KeypadHash = 204,
    KeypadSpace = 205,
    KeypadAt = 206,
    KeypadExclam = 207,
    KeypadMemStore = 208,
    KeypadMemRecall = 209,
    KeypadMemClear = 210,
    KeypadMemAdd = 211,
    KeypadMemSubtract = 212,
    KeypadMemMultiply = 213,
    KeypadMemDivide = 214,
    KeypadPlusMinus = 215,
    KeypadClear = 216,
    KeypadClearEntry = 217,
    KeypadBinary = 218,
    KeypadOctal = 219,
    KeypadDecimal = 220,
    KeypadHexadecimal = 221,
    LeftCtrl = 224,
    LeftShift = 225,
    LeftAlt = 226,
    LeftGui = 227,
    RightCtrl = 228,
    RightShift = 229,
    RightAlt = 230,
    RightGui = 231,
    Mode = 257,
    Sleep = 258,
    Wake = 259,
    ChannelIncrement = 260,
    ChannelDecrement = 261,
    MediaPlay = 262,
    MediaPause = 263,
    MediaRecord = 264,
    MediaFastForward = 265,
    MediaRewind = 266,
    MediaNextTrack = 267,
    MediaPreviousTrack = 268,
    MediaStop = 269,
    MediaEject = 270,
    MediaPlayPause = 271,
    MediaSelect = 272,
    AcNew = 273,
    AcOpen = 274,
    AcClose = 275,
    AcExit = 276,
    AcSave = 277,
    AcPrint = 278,
    AcProperties = 279,
    AcSearch = 280,
    AcHome = 281,
    AcBack = 282,
    AcForward = 283,
    AcStop = 284,
    AcRefresh = 285,
    AcBookmarks = 286,
    SoftLeft = 287,
    SoftRight = 288,
    Call = 289,
    EndCall = 290,
    Count = 512,
};

enum class MouseButton : std::uint8_t {
    Left = 0,
    Right = 1,
    Middle = 2,
    X1 = 3,
    X2 = 4,
    Count = 5,
};

namespace detail {

struct InputAccess;

}

class Input {
public:
    [[nodiscard]] bool key_pressed(Key key) const noexcept;
    [[nodiscard]] bool key_just_pressed(Key key) const noexcept;
    [[nodiscard]] bool key_just_released(Key key) const noexcept;
    [[nodiscard]] bool mouse_pressed(MouseButton button) const noexcept;
    [[nodiscard]] bool mouse_just_pressed(MouseButton button) const noexcept;
    [[nodiscard]] bool mouse_just_released(MouseButton button) const noexcept;
    [[nodiscard]] vec2 mouse_position() const noexcept;
    [[nodiscard]] vec2 mouse_delta() const noexcept;
    [[nodiscard]] vec2 wheel_delta() const noexcept;

private:
    void clear_frame_state() noexcept;
    void set_key(Key key, bool pressed) noexcept;
    void set_mouse_button(MouseButton button, bool pressed) noexcept;
    void set_mouse_position(vec2 position) noexcept;
    void add_mouse_delta(vec2 delta) noexcept;
    void add_wheel_delta(vec2 delta) noexcept;

    friend struct detail::InputAccess;

    static constexpr std::size_t key_count = static_cast<std::size_t>(Key::Count);
    static constexpr std::size_t mouse_button_count = static_cast<std::size_t>(MouseButton::Count);

    std::array<bool, key_count> keys_{};
    std::array<bool, key_count> keys_just_pressed_{};
    std::array<bool, key_count> keys_just_released_{};
    std::array<bool, mouse_button_count> mouse_buttons_{};
    std::array<bool, mouse_button_count> mouse_buttons_just_pressed_{};
    std::array<bool, mouse_button_count> mouse_buttons_just_released_{};
    vec2 mouse_position_{};
    vec2 mouse_delta_{};
    vec2 wheel_delta_{};
};

namespace detail {

struct InputAccess {
    static void clear_frame_state(Input& input) noexcept {
        input.clear_frame_state();
    }

    static void set_key(Input& input, Key key, bool pressed) noexcept {
        input.set_key(key, pressed);
    }

    static void set_mouse_button(Input& input, MouseButton button, bool pressed) noexcept {
        input.set_mouse_button(button, pressed);
    }

    static void set_mouse_position(Input& input, vec2 position) noexcept {
        input.set_mouse_position(position);
    }

    static void add_mouse_delta(Input& input, vec2 delta) noexcept {
        input.add_mouse_delta(delta);
    }

    static void add_wheel_delta(Input& input, vec2 delta) noexcept {
        input.add_wheel_delta(delta);
    }
};

} // namespace detail

[[nodiscard]] bool valid_key(Key key) noexcept;
[[nodiscard]] bool valid_mouse_button(MouseButton button) noexcept;

} // namespace engine
