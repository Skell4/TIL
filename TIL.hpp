#ifndef TIL_HPP_INCLUDED
#define TIL_HPP_INCLUDED

#if defined(min)
#pragma push_macro("min")
#undef min
#define TIL_DETAIL_RESTORE_CALLER_MIN
#endif
#if defined(max)
#pragma push_macro("max")
#undef max
#define TIL_DETAIL_RESTORE_CALLER_MAX
#endif
#include <atomic>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <csignal>
#include <cstdlib>
#include <exception>
#include <limits>
#include <new>
#include <string>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#if defined(min)
#pragma push_macro("min")
#undef min
#define TIL_DETAIL_POP_MIN
#endif
#if defined(max)
#pragma push_macro("max")
#undef max
#define TIL_DETAIL_POP_MAX
#endif
#include <algorithm>
#if defined(TIL_DETAIL_POP_MIN)
#pragma pop_macro("min")
#undef TIL_DETAIL_POP_MIN
#endif
#if defined(TIL_DETAIL_POP_MAX)
#pragma pop_macro("max")
#undef TIL_DETAIL_POP_MAX
#endif
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <asm/bitsperlong.h>
#include <linux/types.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>
#else
#error "TIL.hpp supports Windows consoles and Linux terminals only"
#endif

#if defined(TIL_DETAIL_RESTORE_CALLER_MIN)
#pragma pop_macro("min")
#undef TIL_DETAIL_RESTORE_CALLER_MIN
#endif
#if defined(TIL_DETAIL_RESTORE_CALLER_MAX)
#pragma pop_macro("max")
#undef TIL_DETAIL_RESTORE_CALLER_MAX
#endif
namespace til {

enum class Key {
    Unknown = -1,
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Space, Enter, Escape, Tab, ArrowUp, ArrowDown, ArrowLeft, ArrowRight,
    Shift, Ctrl, Alt, Backspace, Delete, Home, End, PageUp, PageDown, Insert,
    Count
};

enum class MouseButton { Unknown = -1, Left, Right, Middle, Count };
struct Vec2 { int x, y; };

struct InputModifiers {
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
};
struct InputMouseEvent {
    enum class Action { Move, Press, Release, Scroll };
    Action action = Action::Move;
    MouseButton button = MouseButton::Unknown;
    Vec2 position = {0, 0};
    Vec2 delta = {0, 0};
    Vec2 wheel = {0, 0};
};
struct InputEvent {
    enum class Type { Text, Key, UnknownSequence, Mouse, Focus };
    Type type = Type::Text;

    std::string text;
    Key key = Key::Unknown;
    InputModifiers modifiers;
    InputMouseEvent mouse;
    bool pasted = false;
    bool focused = true;
    bool truncated = false;
};
enum class MouseTracking { Buttons, Drag, AllMotion };
enum class KeyboardBackend { Unavailable, WindowsConsole, Kitty, LinuxConsole };
struct InputOptions {
    std::size_t queueCapacity = 4096;
    int escapeTimeoutMs = 30;

    bool enableMouse = false;
    MouseTracking mouseTracking = MouseTracking::AllMotion;
    bool coalesceMouseMoves = false;
    bool prioritizeInput = true;
    std::size_t lineCapacity = 65536;
    bool discardPendingInputOnShutdown = false;
    bool preserveSignals = false;
    bool enableBracketedPaste = false;
    bool enableFocusEvents = false;
    bool enableKeyboardState = true;
    int keyboardProtocolTimeoutMs = 200;
    bool automaticRestore = false;
    int sequenceTimeoutMs = 1000;
};
struct InputCapabilities {
    bool text = false;
    bool specialKeys = false;
    bool keyState = false;
    bool keyStatePending = false;
    KeyboardBackend keyboardBackend = KeyboardBackend::Unavailable;
    bool mouse = false;

    unsigned mouseButtons = 0;
    bool mouseVerticalWheel = false;
    bool mouseHorizontalWheel = false;
};
enum class InputError {
    NoError, NotInitialized, InvalidOptions, NotTerminal, ConfigureFailed,
    ReadFailed, RestoreFailed, QueueFull, InvalidTerminator, AlreadyInUse,
    LineTooLong, Interrupted
};

namespace input_detail {
inline bool validButton(MouseButton button) {
    return static_cast<int>(button) >= 0 && button < MouseButton::Count;
}
inline int saturatedAdd(int a, int b) {
    const long long sum = static_cast<long long>(a) + b;
    if (sum > (std::numeric_limits<int>::max)()) return (std::numeric_limits<int>::max)();
    if (sum < (std::numeric_limits<int>::min)()) return (std::numeric_limits<int>::min)();
    return static_cast<int>(sum);
}
inline bool validKey(Key key) {
    return static_cast<int>(key) >= 0 && key < Key::Count;
}
inline Key asciiKey(unsigned char c) {
    if (c >= 'a' && c <= 'z') return static_cast<Key>(int(Key::A) + c - 'a');
    if (c >= 'A' && c <= 'Z') return static_cast<Key>(int(Key::A) + c - 'A');
    if (c >= '0' && c <= '9') return static_cast<Key>(int(Key::Num0) + c - '0');
    return c == ' ' ? Key::Space : Key::Unknown;
}
inline std::string utf8(std::uint32_t cp) {
    if (cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) cp = 0xfffd;
    std::string s;
    if (cp < 0x80) s += static_cast<char>(cp);
    else if (cp < 0x800) {
        s += static_cast<char>(0xc0 | (cp >> 6));
        s += static_cast<char>(0x80 | (cp & 63));
    } else if (cp < 0x10000) {
        s += static_cast<char>(0xe0 | (cp >> 12));
        s += static_cast<char>(0x80 | ((cp >> 6) & 63));
        s += static_cast<char>(0x80 | (cp & 63));
    } else {
        s += static_cast<char>(0xf0 | (cp >> 18));
        s += static_cast<char>(0x80 | ((cp >> 12) & 63));
        s += static_cast<char>(0x80 | ((cp >> 6) & 63));
        s += static_cast<char>(0x80 | (cp & 63));
    }
    return s;
}

inline bool validTextTerminator(const std::string& s) {
    if (s.empty() || s.size() > 4) return false;
    const unsigned char first = static_cast<unsigned char>(s[0]);
    if (first < 0x80) return s.size() == 1 && first >= 32 && first != 127;
    std::size_t length = 0;
    std::uint32_t cp = 0, minimum = 0;
    if (first >= 0xc2 && first <= 0xdf) { length = 2; cp = first & 31; minimum = 0x80; }
    else if (first >= 0xe0 && first <= 0xef) { length = 3; cp = first & 15; minimum = 0x800; }
    else if (first >= 0xf0 && first <= 0xf4) { length = 4; cp = first & 7; minimum = 0x10000; }
    else return false;
    if (s.size() != length) return false;
    for (std::size_t i = 1; i < length; ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if ((c & 0xc0) != 0x80) return false;
        cp = (cp << 6) | (c & 63);
    }
    return cp >= minimum && cp <= 0x10ffff && !(cp >= 0xd800 && cp <= 0xdfff);
}

class EventBuffer {
public:
    void reset(std::size_t capacity, std::size_t lineCapacity = 65536,
               bool coalesce = false, bool prioritize = true) {
        events_.clear(); partialLine_.clear(); capacity_ = capacity; dropped_ = 0;
        lineCapacity_ = lineCapacity; lineOverflow_ = false;
        coalesce_ = coalesce; prioritize_ = prioritize;
        inferPress_ = true;
        down_.fill(false); previous_.fill(false);
        pressed_.fill(false); released_.fill(false);
        mouseDown_.fill(false); mousePrevious_.fill(false);
        mousePressed_.fill(false); mouseReleased_.fill(false);
        position_ = delta_ = wheel_ = Vec2{0, 0};
        positionKnown_ = verticalWheel_ = horizontalWheel_ = false; buttons_ = 0;
    }
    void beginFrame() {
        previous_ = down_; pressed_.fill(false); released_.fill(false);
        mousePrevious_ = mouseDown_; mousePressed_.fill(false); mouseReleased_.fill(false);
        delta_ = wheel_ = Vec2{0, 0};
    }
    void setKey(Key key, bool down) {
        if (!validKey(key)) return;
        const std::size_t i = static_cast<std::size_t>(key);
        if (down && !down_[i]) pressed_[i] = true;
        if (!down && down_[i]) released_[i] = true;
        down_[i] = down;
    }
    void clearDown() {
        for (int i = 0; i < int(Key::Count); ++i) setKey(static_cast<Key>(i), false);
    }
    void focus(bool focused) {
        if (!focused) {
            clearDown();
            for (std::size_t i = 0; i < mouseDown_.size(); ++i) {
                if (mouseDown_[i]) mouseReleased_[i] = true;
                mouseDown_[i] = false;
            }
        }
        InputEvent event; event.type = InputEvent::Type::Focus;
        event.focused = focused; push(std::move(event));
    }
    void press(Key key) {
        if (inferPress_ && validKey(key)) pressed_[static_cast<std::size_t>(key)] = true;
    }
    void inferPress(bool enabled) { inferPress_ = enabled; }
    void push(InputEvent event) {
        const bool move = isMove(event);
        if (coalesce_ && move && !events_.empty() && isMove(events_.back()) &&
            events_.back().mouse.button == event.mouse.button &&
            events_.back().modifiers.shift == event.modifiers.shift &&
            events_.back().modifiers.ctrl == event.modifiers.ctrl &&
            events_.back().modifiers.alt == event.modifiers.alt) {
            event.mouse.delta.x = saturatedAdd(events_.back().mouse.delta.x, event.mouse.delta.x);
            event.mouse.delta.y = saturatedAdd(events_.back().mouse.delta.y, event.mouse.delta.y);
            events_.back() = std::move(event); return;
        }
        if (events_.size() >= capacity_) {
            ++dropped_;
            if (!prioritize_ || move) return;
            auto it = events_.begin();
            while (it != events_.end() && !isMove(*it)) ++it;
            if (it == events_.end()) return;
            events_.erase(it);
        }
        events_.push_back(std::move(event));
    }
    void text(std::uint32_t cp, InputModifiers modifiers = InputModifiers(), bool pasted = false) {
        InputEvent event; event.text = utf8(cp); event.modifiers = modifiers;
        event.pasted = pasted;
        push(std::move(event));
    }
    void key(Key key, InputModifiers modifiers = InputModifiers(), bool inferred = true) {
        if (inferred) press(key);
        InputEvent event; event.type = InputEvent::Type::Key;
        event.key = key; event.modifiers = modifiers; push(std::move(event));
    }
    void unknown(const std::string& sequence, bool truncated = false) {
        InputEvent event; event.type = InputEvent::Type::UnknownSequence;
        event.text = sequence; event.truncated = truncated; push(std::move(event));
    }
    Vec2 locate(Vec2 position) {
        Vec2 delta = {0, 0};
        if (positionKnown_) {
            delta.x = saturatedAdd(position.x, -position_.x);
            delta.y = saturatedAdd(position.y, -position_.y);
        }
        position_ = position; positionKnown_ = true;
        delta_.x = saturatedAdd(delta_.x, delta.x);
        delta_.y = saturatedAdd(delta_.y, delta.y);
        return delta;
    }
    void observeButton(MouseButton button) {
        if (validButton(button)) buttons_ |= 1u << static_cast<unsigned>(button);
    }
    void mouse(InputMouseEvent::Action action, MouseButton button, Vec2 position,
               Vec2 delta, Vec2 wheel, InputModifiers modifiers) {
        if (validButton(button)) {
            observeButton(button);
            const std::size_t i = static_cast<std::size_t>(button);
            if (action == InputMouseEvent::Action::Press) {
                if (!mouseDown_[i]) mousePressed_[i] = true;
                mouseDown_[i] = true;
            } else if (action == InputMouseEvent::Action::Release) {

                mouseReleased_[i] = true; mouseDown_[i] = false;
            }
        }
        wheel_.x = saturatedAdd(wheel_.x, wheel.x);
        wheel_.y = saturatedAdd(wheel_.y, wheel.y);
        horizontalWheel_ = horizontalWheel_ || wheel.x != 0;
        verticalWheel_ = verticalWheel_ || wheel.y != 0;
        InputEvent event; event.type = InputEvent::Type::Mouse; event.modifiers = modifiers;
        event.mouse.action = action; event.mouse.button = button;
        event.mouse.position = position; event.mouse.delta = delta; event.mouse.wheel = wheel;
        push(std::move(event));
    }
    bool mouseDown(MouseButton b) const { return validButton(b) && mouseDown_[std::size_t(b)]; }
    bool mousePressed(MouseButton b) const { return validButton(b) && mousePressed_[std::size_t(b)]; }
    bool mouseReleased(MouseButton b) const { return validButton(b) && mouseReleased_[std::size_t(b)]; }
    bool mouseHeld(MouseButton b) const {
        return mouseDown(b) && mousePrevious_[std::size_t(b)];
    }
    bool positionKnown() const { return positionKnown_; }
    Vec2 position() const { return position_; }
    Vec2 delta() const { return delta_; }
    Vec2 wheel() const { return wheel_; }
    unsigned buttons() const { return buttons_; }
    bool verticalWheel() const { return verticalWheel_; }
    bool horizontalWheel() const { return horizontalWheel_; }
    bool pop(InputEvent& event) {
        if (events_.empty()) return false;
        event = std::move(events_.front()); events_.pop_front(); return true;
    }
    bool popType(InputEvent::Type type, InputEvent& event) {
        for (auto it = events_.begin(); it != events_.end(); ++it) {
            if (it->type != type) continue;
            event = std::move(*it); events_.erase(it); return true;
        }
        return false;
    }

    bool line(std::string& result, Key endKey, const std::string& endText, bool* overflow = nullptr) {
        for (auto it = events_.begin(); it != events_.end();) {
            const bool text = it->type == InputEvent::Type::Text;
            const bool key = it->type == InputEvent::Type::Key;
            const bool ending = endText.empty() ? (key && it->key == endKey) :
                                                 (text && !it->pasted && it->text == endText);
            if (ending) {
                if (overflow) *overflow = lineOverflow_;
                lineOverflow_ = false;
                result = std::move(partialLine_); partialLine_.clear();
                events_.erase(it); return true;
            }

            if (lineOverflow_ && (text || (key && it->key == Key::Backspace))) {
                it = events_.erase(it); continue;
            }
            if (text) {
                if (it->text.size() <= lineCapacity_ - partialLine_.size()) partialLine_ += it->text;
                else lineOverflow_ = true;
            }
            else if (key && it->key == Key::Backspace) {
                if (!partialLine_.empty()) {
                    std::size_t start = partialLine_.size() - 1;
                    while (start && (static_cast<unsigned char>(partialLine_[start]) & 0xc0) == 0x80) --start;
                    partialLine_.erase(start);
                }
            } else { ++it; continue; }
            it = events_.erase(it);
        }
        result = partialLine_;
        if (overflow) *overflow = lineOverflow_;
        return false;
    }
    void clearLine() { partialLine_.clear(); lineOverflow_ = false; }
    void clearEvents() { events_.clear(); }
    bool down(Key key) const { return validKey(key) && down_[std::size_t(key)]; }
    bool pressed(Key key) const { return validKey(key) && pressed_[std::size_t(key)]; }
    bool released(Key key) const { return validKey(key) && released_[std::size_t(key)]; }
    bool held(Key key) const {
        return validKey(key) && down_[std::size_t(key)] && previous_[std::size_t(key)];
    }
    std::size_t pending() const { return events_.size(); }
    std::size_t dropped() const { return dropped_; }
private:
    static bool isMove(const InputEvent& event) {
        return event.type == InputEvent::Type::Mouse && event.mouse.action == InputMouseEvent::Action::Move;
    }
    std::deque<InputEvent> events_;
    std::string partialLine_;
    std::size_t capacity_ = 4096, dropped_ = 0;
    std::size_t lineCapacity_ = 65536;
    bool lineOverflow_ = false, coalesce_ = false, prioritize_ = true;
    bool inferPress_ = true;
    std::array<bool, std::size_t(Key::Count)> down_ = {}, previous_ = {};
    std::array<bool, std::size_t(Key::Count)> pressed_ = {}, released_ = {};
    std::array<bool, std::size_t(MouseButton::Count)> mouseDown_ = {}, mousePrevious_ = {};
    std::array<bool, std::size_t(MouseButton::Count)> mousePressed_ = {}, mouseReleased_ = {};
    Vec2 position_ = {0, 0}, delta_ = {0, 0}, wheel_ = {0, 0};
    bool positionKnown_ = false, verticalWheel_ = false, horizontalWheel_ = false;
    unsigned buttons_ = 0;
};

class TerminalParser {
public:
    typedef std::chrono::steady_clock Clock;
    typedef Clock::time_point Time;
    explicit TerminalParser(EventBuffer& events) : events_(events) {}
    void reset(int timeout, bool mouse = false, MouseTracking tracking = MouseTracking::AllMotion,
               int sequenceTimeout = 1000) {
        bytes_.clear(); timeout_ = timeout; waiting_ = false; mouse_ = mouse;
        sequenceTimeout_ = sequenceTimeout; scalarWaiting_ = false;
        tracking_ = tracking;
        paste_ = previousCR_ = false; stringKind_ = 0; control_.clear();
        stringEscape_ = stringTruncated_ = false;
        keyboardEnabled_ = replyExpected_ = confirmExpected_ = replyReady_ = attributesReady_ = attributesExpected_ = false;
        keySources_.fill(0);
    }
    void feed(const char* bytes, std::size_t size, Time now) {

        parse(now);
        if (size) { bytes_.append(bytes, size); lastInput_ = now; }
        parse(now);
    }
    void tick(Time now) { parse(now); }
    void expectKeyboardReply(bool confirmation) {
        replyExpected_ = true; confirmExpected_ = confirmation;
        if (!confirmation) attributesExpected_ = true;
        replyReady_ = attributesReady_ = false;
    }
    void disableKeyboard() {
        keyboardEnabled_ = replyExpected_ = confirmExpected_ = replyReady_ = attributesReady_ = attributesExpected_ = false;
        keySources_.fill(0); events_.clearDown();
    }
    bool keyboardEnabled() const { return keyboardEnabled_; }
    bool takeKeyboardReply(unsigned& flags) {
        if (!replyReady_) return false;
        replyReady_ = false; flags = replyFlags_; return true;
    }
    bool takeAttributesReply() { const bool ready = attributesReady_; attributesReady_ = false; return ready; }
    int waitTime(int requested, Time now) const {
        if (waiting_) requested = boundedWait(requested, now,
            extendedWait() ? lastInput_ : escapeSince_, extendedWait() ? sequenceTimeout_ : timeout_);
        if (stringKind_ || paste_ || scalarWaiting_)
            requested = boundedWait(requested, now, lastInput_, sequenceTimeout_);
        return requested;
    }
private:
    static int boundedWait(int requested, Time now, Time since, int limit) {
        const auto remaining = std::chrono::milliseconds(limit) - (now - since);
        const auto wholeMs = std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count();
        const int timeout = remaining <= Clock::duration::zero() ? 0 :
            (wholeMs >= (std::numeric_limits<int>::max)() ? (std::numeric_limits<int>::max)() :
             static_cast<int>(wholeMs) + 1);
        return requested < 0 || timeout < requested ? timeout : requested;
    }
    EventBuffer& events_;
    std::string bytes_;
    int timeout_ = 30;
    int sequenceTimeout_ = 1000;
    bool scalarWaiting_ = false;
    bool waiting_ = false;
    bool mouse_ = false;
    MouseTracking tracking_ = MouseTracking::AllMotion;
    bool paste_ = false, previousCR_ = false;
    char stringKind_ = 0;
    std::string control_;
    bool stringEscape_ = false, stringTruncated_ = false;
    bool keyboardEnabled_ = false, replyExpected_ = false, confirmExpected_ = false;
    bool replyReady_ = false, attributesReady_ = false, attributesExpected_ = false;
    unsigned replyFlags_ = 0;
    std::array<unsigned, std::size_t(Key::Count)> keySources_ = {};
    Time escapeSince_, lastInput_, previousCRSince_;

    bool extendedWait(std::size_t offset = 0) const {
        return keyboardEnabled_ || replyExpected_ ||
            (bytes_.size() - offset >= 3 && bytes_[offset] == '\x1b' &&
             bytes_[offset + 1] == '[' && bytes_[offset + 2] == '?');
    }

    std::size_t scalar(std::size_t offset, InputModifiers modifiers, Time now, bool literal = false) {
        scalarWaiting_ = false;
        const unsigned char first = static_cast<unsigned char>(bytes_[offset]);
        if (literal && first < 0x80) { events_.text(first, modifiers, true); return 1; }
        if (first < 0x80) {
            const bool pairedLF = first == '\n' && previousCR_;
            previousCR_ = first == '\r';
            if (previousCR_) previousCRSince_ = now;
            if (pairedLF) return 1;
            Key key = Key::Unknown;
            if (first == '\r' || first == '\n') key = Key::Enter;
            else if (first == '\t') key = Key::Tab;
            else if (first == 8 || first == 127) key = Key::Backspace;
            else if (first < 32) {
                modifiers.ctrl = true;
                if (first >= 1 && first <= 26) key = static_cast<Key>(int(Key::A) + first - 1);
            }
            if (key != Key::Unknown) events_.key(key, modifiers);
            else if (first < 32) events_.unknown(std::string(1, static_cast<char>(first)));
            else { events_.press(asciiKey(first)); events_.text(first, modifiers); }
            return 1;
        }
        previousCR_ = false;
        int length = 0;
        std::uint32_t cp = 0, minimum = 0;
        if (first >= 0xc2 && first <= 0xdf) { length = 2; cp = first & 31; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { length = 3; cp = first & 15; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { length = 4; cp = first & 7; minimum = 0x10000; }
        else { events_.text(0xfffd, modifiers, literal); return 1; }
        for (int i = 1; i < length; ++i) {
            if (offset + std::size_t(i) >= bytes_.size()) {
                if (now - lastInput_ < std::chrono::milliseconds(sequenceTimeout_)) {
                    scalarWaiting_ = true; return 0;
                }

                events_.text(0xfffd, modifiers, literal); return bytes_.size() - offset;
            }
            const unsigned char c = static_cast<unsigned char>(bytes_[offset + std::size_t(i)]);
            if ((c & 0xc0) != 0x80) { events_.text(0xfffd, modifiers, literal); return 1; }
            cp = (cp << 6) | (c & 63);
        }
        if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff)) {
            events_.text(0xfffd, modifiers, literal); return 1;
        }
        events_.text(cp, modifiers, literal); return std::size_t(length);
    }

    static bool fields(const std::string& s, unsigned (&values)[3][32], unsigned (&counts)[3],
                       unsigned& fieldCount) {
        fieldCount = 1; counts[0] = 1;
        bool digit = false;
        for (std::size_t i = 2; i + 1 < s.size(); ++i) {
            const char c = s[i];
            if (i == 2 && (c < '0' || c > '9')) return false;
            if (c == ':' || c == ';') {
                if (c == ';') {
                    if (fieldCount == 3) return false;
                    counts[fieldCount++] = 1;
                } else if (++counts[fieldCount - 1] > 32) return false;
                digit = false; continue;
            }
            if (c < '0' || c > '9') return false;
            unsigned& value = values[fieldCount - 1][counts[fieldCount - 1] - 1];
            if (value > (0x10ffffu - static_cast<unsigned>(c - '0')) / 10) return false;
            value = value * 10 + static_cast<unsigned>(c - '0'); digit = true;
        }
        return digit && counts[0] <= 3 && (fieldCount < 2 || counts[1] <= 2);
    }
    static bool scalarCode(unsigned code) { return code <= 0x10ffff && !(code >= 0xd800 && code <= 0xdfff); }
    static Key unicodeKey(unsigned code) {
        if (code < 128) {
            switch (code) {
            case 27: return Key::Escape; case 13: return Key::Enter;
            case 9: return Key::Tab; case 127: return Key::Backspace;
            default: return asciiKey(static_cast<unsigned char>(code));
            }
        }
        if (code >= 57364 && code <= 57375) return static_cast<Key>(int(Key::F1) + int(code - 57364));
        if (code >= 57399 && code <= 57408) return static_cast<Key>(int(Key::Num0) + int(code - 57399));
        switch (code) {
        case 57344: return Key::Escape; case 57345: case 57414: return Key::Enter;
        case 57346: return Key::Tab; case 57347: return Key::Backspace;
        case 57348: case 57425: return Key::Insert; case 57349: case 57426: return Key::Delete;
        case 57350: case 57417: return Key::ArrowLeft; case 57351: case 57418: return Key::ArrowRight;
        case 57352: case 57419: return Key::ArrowUp; case 57353: case 57420: return Key::ArrowDown;
        case 57354: case 57421: return Key::PageUp; case 57355: case 57422: return Key::PageDown;
        case 57356: case 57423: return Key::Home; case 57357: case 57424: return Key::End;
        case 57441: case 57447: return Key::Shift; case 57442: case 57448: return Key::Ctrl;
        case 57443: case 57449: case 57453: return Key::Alt;
        default: return Key::Unknown;
        }
    }
    void keyboardEvent(Key key, unsigned code, unsigned type, InputModifiers modifiers,
                       const unsigned* text, unsigned textCount) {
        if (keyboardEnabled_ && code != 0) {
            events_.setKey(Key::Shift, modifiers.shift);
            events_.setKey(Key::Ctrl, modifiers.ctrl);
            events_.setKey(Key::Alt, modifiers.alt);
            if (validKey(key) && key != Key::Shift && key != Key::Ctrl && key != Key::Alt) {
                unsigned& sources = keySources_[static_cast<std::size_t>(key)];
                const unsigned source = code >= 57399 && code <= 57427 ? 2u : 1u;
                if (type == 3) sources &= ~source; else sources |= source;
                events_.setKey(key, sources != 0);
            }
        }
        if (type == 3) return;
        if (textCount) {
            for (unsigned i = 0; i < textCount; ++i) events_.text(text[i], modifiers);
        } else if (key != Key::Shift && key != Key::Ctrl && key != Key::Alt)
            events_.key(key, modifiers, false);
    }
    bool extendedKey(const std::string& s) {
        const char final = s.back();
        if (final != 'u' && s.find(':') == std::string::npos) return false;
        unsigned values[3][32] = {}, counts[3] = {}, fieldCount = 0;
        if (!fields(s, values, counts, fieldCount)) return false;
        unsigned code = values[0][0];
        const unsigned encoded = fieldCount >= 2 && values[1][0] ? values[1][0] : 1;
        const unsigned type = fieldCount >= 2 && counts[1] >= 2 ? values[1][1] : 1;
        if (encoded > 256 || type < 1 || type > 3) return false;
        InputModifiers modifiers;
        modifiers.shift = ((encoded - 1) & 1) != 0; modifiers.alt = ((encoded - 1) & 2) != 0;
        modifiers.ctrl = ((encoded - 1) & 4) != 0;
        Key key = Key::Unknown;
        if (final == 'u') {
            for (unsigned i = 0; i < counts[0]; ++i) if (!scalarCode(values[0][i])) return false;
            key = unicodeKey(code);
            if (key == Key::Unknown && counts[0] == 3) key = unicodeKey(values[0][2]);
        } else {
            if (counts[0] != 1 || fieldCount > 2) return false;
            if (final == '~') {
                switch (code) {
                case 1: case 7: key = Key::Home; break; case 2: key = Key::Insert; break;
                case 3: key = Key::Delete; break; case 4: case 8: key = Key::End; break;
                case 5: key = Key::PageUp; break; case 6: key = Key::PageDown; break;
                case 11: key = Key::F1; break; case 12: key = Key::F2; break;
                case 13: key = Key::F3; break; case 14: key = Key::F4; break;
                case 15: key = Key::F5; break; case 17: key = Key::F6; break;
                case 18: key = Key::F7; break; case 19: key = Key::F8; break;
                case 20: key = Key::F9; break; case 21: key = Key::F10; break;
                case 23: key = Key::F11; break; case 24: key = Key::F12; break;
                default: return false;
                }
            } else {
                if (code != 1) return false;
                switch (final) {
                case 'A': key = Key::ArrowUp; break; case 'B': key = Key::ArrowDown; break;
                case 'C': key = Key::ArrowRight; break; case 'D': key = Key::ArrowLeft; break;
                case 'H': key = Key::Home; break; case 'F': key = Key::End; break;
                case 'P': key = Key::F1; break; case 'Q': key = Key::F2; break;
                case 'S': key = Key::F4; break; default: return false;
                }
            }
            code = 128u + static_cast<unsigned>(key);
        }
        const unsigned textCount = fieldCount == 3 ? counts[2] : 0;
        for (unsigned i = 0; i < textCount; ++i) {
            const unsigned cp = values[2][i];
            if (!scalarCode(cp) || cp < 32 || (cp >= 127 && cp <= 159)) return false;
        }
        keyboardEvent(key, code, type, modifiers, values[2], textCount); return true;
    }
    bool sequence(const std::string& s, bool altPrefix = false) {
        if (!altPrefix && s.size() >= 5 && s[2] == '?' && s.back() == 'u') {
            unsigned flags = 0;
            for (std::size_t i = 3; i + 1 < s.size(); ++i) {
                if (s[i] < '0' || s[i] > '9' || flags > 100000) return false;
                flags = flags * 10 + static_cast<unsigned>(s[i] - '0');
            }
            if (!replyExpected_) return false;
            replyExpected_ = false; replyReady_ = true; replyFlags_ = flags;
            if (confirmExpected_) keyboardEnabled_ = (flags & 26u) == 26u;
            return true;
        }
        if (!altPrefix && s.size() >= 4 && s[2] == '?' && s.back() == 'c') {
            if (!attributesExpected_) return false;
            for (std::size_t i = 3; i + 1 < s.size(); ++i)
                if ((s[i] < '0' || s[i] > '9') && s[i] != ';') return false;
            attributesReady_ = true; attributesExpected_ = false; return true;
        }
        if (!altPrefix && s[1] == '[' && (s.back() == 'u' || s.find(':') != std::string::npos))
            return extendedKey(s);
        if (!altPrefix && (s == "\x1b[200~" || s == "\x1b[201~")) {
            paste_ = s == "\x1b[200~"; return true;
        }
        if (!altPrefix && (s == "\x1b[I" || s == "\x1b[O")) {
            if (s.back() == 'O') keySources_.fill(0);
            events_.focus(s.back() == 'I'); return true;
        }
        if (s.size() >= 3 && s[1] == '[' && s[2] == '<')
            return !altPrefix && mouse_ && mouseSequence(s);
        const char final = s.back();
        int params[2] = { 0, 0 }; int field = 0;
        for (std::size_t i = 2; i + 1 < s.size(); ++i) {
            const char c = s[i];
            if (c == ';' && field == 0) { ++field; continue; }
            if (c < '0' || c > '9' || params[field] > 1000) return false;
            params[field] = params[field] * 10 + (c - '0');
        }
        InputModifiers modifiers; modifiers.alt = altPrefix;
        if (field == 1) {
            if (params[1] < 1 || params[1] > 8) return false;
            const int bits = params[1] - 1;
            modifiers.shift = (bits & 1) != 0;
            modifiers.alt = altPrefix || (bits & 2) != 0;
            modifiers.ctrl = (bits & 4) != 0;
        }
        Key key = Key::Unknown;
        if (final != '~' && params[0] != 0 && params[0] != 1) return false;
        switch (final) {
        case 'A': key = Key::ArrowUp; break;
        case 'B': key = Key::ArrowDown; break;
        case 'C': key = Key::ArrowRight; break;
        case 'D': key = Key::ArrowLeft; break;
        case 'H': key = Key::Home; break;
        case 'F': key = Key::End; break;
        case 'P': key = Key::F1; break;
        case 'Q': key = Key::F2; break;
        case 'R': key = Key::F3; break;
        case 'S': key = Key::F4; break;
        case 'Z': key = Key::Tab; modifiers.shift = true; break;
        case '~':
            switch (params[0]) {
            case 1: case 7: key = Key::Home; break;
            case 2: key = Key::Insert; break;
            case 3: key = Key::Delete; break;
            case 4: case 8: key = Key::End; break;
            case 5: key = Key::PageUp; break;
            case 6: key = Key::PageDown; break;
            case 11: key = Key::F1; break;
            case 12: key = Key::F2; break;
            case 13: key = Key::F3; break;
            case 14: key = Key::F4; break;
            case 15: key = Key::F5; break;
            case 17: key = Key::F6; break;
            case 18: key = Key::F7; break;
            case 19: key = Key::F8; break;
            case 20: key = Key::F9; break;
            case 21: key = Key::F10; break;
            case 23: key = Key::F11; break;
            case 24: key = Key::F12; break;
            }
            break;
        }
        if (key == Key::Unknown) return false;
        if (keyboardEnabled_) keyboardEvent(key, 128u + static_cast<unsigned>(key), 1, modifiers, nullptr, 0);
        else events_.key(key, modifiers);
        return true;
    }
    bool mouseSequence(const std::string& s) {
        if (s.back() != 'M' && s.back() != 'm') return false;
        int params[3] = {0, 0, 0}, field = 0;
        bool digit = false;
        for (std::size_t i = 3; i + 1 < s.size(); ++i) {
            const char c = s[i];
            if (c == ';' && field < 2 && digit) { ++field; digit = false; continue; }
            if (c < '0' || c > '9' ||
                params[field] > ((std::numeric_limits<int>::max)() - (c - '0')) / 10) return false;
            params[field] = params[field] * 10 + (c - '0'); digit = true;
        }
        if (field != 2 || !digit || params[0] > 255 || params[1] < 1 || params[2] < 1) return false;
        const int code = params[0], base = code & ~(4 | 8 | 16 | 32);
        const bool motion = (code & 32) != 0, release = s.back() == 'm';
        InputModifiers modifiers;
        modifiers.shift = (code & 4) != 0; modifiers.alt = (code & 8) != 0;
        modifiers.ctrl = (code & 16) != 0;
        MouseButton button = MouseButton::Unknown;
        Vec2 wheel = {0, 0};
        InputMouseEvent::Action action;
        if (base >= 64 && base <= 67) {
            if (release || motion) return false;
            action = InputMouseEvent::Action::Scroll;
            if (base == 64) wheel.y = 120;
            if (base == 65) wheel.y = -120;
            if (base == 66) wheel.x = -120;
            if (base == 67) wheel.x = 120;
        } else {
            switch (base) {
            case 0: button = MouseButton::Left; break;
            case 1: button = MouseButton::Middle; break;
            case 2: button = MouseButton::Right; break;
            case 3: if (!motion || release) return false; break;
            default: return false;
            }
            if (motion && release) return false;
            action = motion ? InputMouseEvent::Action::Move :
                (release ? InputMouseEvent::Action::Release : InputMouseEvent::Action::Press);
        }
        const Vec2 position = {params[1] - 1, params[2] - 1};

        events_.observeButton(button);
        const Vec2 delta = events_.locate(position);
        if (motion && (tracking_ == MouseTracking::Buttons ||
            (tracking_ == MouseTracking::Drag && button == MouseButton::Unknown))) return true;
        events_.mouse(action, button, position, delta, wheel, modifiers);
        return true;
    }
    void parse(Time now) {
        if (previousCR_ && now - previousCRSince_ > std::chrono::milliseconds(timeout_)) previousCR_ = false;
        if (now - lastInput_ >= std::chrono::milliseconds(sequenceTimeout_)) {
            if (stringKind_) {
                events_.unknown(control_, true); control_.clear(); stringKind_ = 0;
                stringEscape_ = stringTruncated_ = false;
            }
            if (paste_) {

                if (!bytes_.empty() && bytes_[0] != '\x1b') {
                    events_.text(0xfffd, InputModifiers(), true); bytes_.clear();
                }
                events_.unknown(std::string("\x1b[200~") + bytes_, true);
                bytes_.clear(); paste_ = scalarWaiting_ = previousCR_ = false;
            }
        }
        std::size_t offset = 0;
        while (offset < bytes_.size()) {
            if (stringKind_) {
                const char c = bytes_[offset++];
                if (control_.size() < 4096) control_ += c;
                else stringTruncated_ = true;
                const bool ended = (stringKind_ == ']' && c == '\a') ||
                    (stringEscape_ && c == '\\') || c == '\x18' || c == '\x1a';
                stringEscape_ = c == '\x1b';
                if (ended) {
                    events_.unknown(control_, stringTruncated_); control_.clear(); stringKind_ = 0;
                    stringEscape_ = stringTruncated_ = false;
                }
                continue;
            }
            if (paste_) {
                const std::string ending = "\x1b[201~";
                const std::size_t available = bytes_.size() - offset;
                const std::size_t compare = available < ending.size() ? available : ending.size();
                if (bytes_.compare(offset, compare, ending, 0, compare) == 0) {
                    if (available < ending.size()) break;
                    offset += ending.size(); paste_ = false; previousCR_ = false; continue;
                }
                const std::size_t size = scalar(offset, InputModifiers(), now, true);
                if (!size) break;
                offset += size; continue;
            }
            if (bytes_[offset] != '\x1b') {
                waiting_ = false;
                const std::size_t size = scalar(offset, InputModifiers(), now);
                if (!size) break;
                offset += size; continue;
            }
            previousCR_ = false;
            if (!waiting_) { waiting_ = true; escapeSince_ = now; }
            const bool extended = extendedWait(offset);
            const bool expired = now - (extended ? lastInput_ : escapeSince_) >=
                std::chrono::milliseconds(extended ? sequenceTimeout_ : timeout_);
            const std::size_t available = bytes_.size() - offset;
            if (extended && expired && available > 1) {
                events_.unknown(bytes_.substr(offset), true);
                offset = bytes_.size(); waiting_ = scalarWaiting_ = false; continue;
            }
            if (available >= 2 &&
                (bytes_[offset + 1] == ']' || bytes_[offset + 1] == 'P' ||
                 bytes_[offset + 1] == '_' || bytes_[offset + 1] == '^' || bytes_[offset + 1] == 'X')) {
                if (available == 2) {
                    if (!expired) break;
                    InputModifiers modifiers; modifiers.alt = true;
                    scalar(offset + 1, modifiers, now); offset += 2; waiting_ = false; continue;
                }
                stringKind_ = bytes_[offset + 1]; control_ = bytes_.substr(offset, 2);
                stringEscape_ = stringTruncated_ = waiting_ = false; offset += 2; continue;
            }

            if (available == 2 && bytes_[offset + 1] == '\x1b' && !expired) break;

            if (available >= 4 && bytes_[offset + 1] == '\x1b' && bytes_[offset + 2] == '[' && bytes_[offset + 3] == '?') {
                events_.key(Key::Escape); ++offset; waiting_ = false; continue;
            }
            const bool altPrefix = available >= 3 && bytes_[offset + 1] == '\x1b' &&
                (bytes_[offset + 2] == '[' || bytes_[offset + 2] == 'O');
            const std::size_t start = offset + (altPrefix ? 1 : 0);
            const std::size_t sequenceSize = bytes_.size() - start;
            const std::size_t maximumSequenceLength = 512;
            if (sequenceSize >= 2 && (bytes_[start + 1] == '[' || bytes_[start + 1] == 'O')) {

                if (!altPrefix && sequenceSize == 2 && expired) {
                    InputModifiers modifiers; modifiers.alt = true;
                    scalar(start + 1, modifiers, now);
                    offset += 2; waiting_ = false; continue;
                }

                if (sequenceSize >= 3 && bytes_[start + 1] == '[' && bytes_[start + 2] == '[') {
                    if (sequenceSize >= 4 && bytes_[start + 3] >= 'A' && bytes_[start + 3] <= 'E') {
                        InputModifiers modifiers; modifiers.alt = altPrefix;
                        events_.key(static_cast<Key>(int(Key::F1) + bytes_[start + 3] - 'A'), modifiers);
                        offset = start + 4; waiting_ = false; continue;
                    }
                    if (sequenceSize < 4 && !expired) break;
                } else {
                    std::size_t end = start + 2;
                    while (end < bytes_.size() && end - start < maximumSequenceLength &&
                           static_cast<unsigned char>(bytes_[end]) >= 0x20 &&
                           static_cast<unsigned char>(bytes_[end]) <= 0x3f) ++end;
                    if (end < bytes_.size() && end - start < maximumSequenceLength &&
                        static_cast<unsigned char>(bytes_[end]) >= 0x40 &&
                        static_cast<unsigned char>(bytes_[end]) <= 0x7e) {
                        const std::string s = bytes_.substr(start, end - start + 1);
                        if (!sequence(s, altPrefix)) events_.unknown(bytes_.substr(offset, end - offset + 1));
                        offset = end + 1; waiting_ = false; continue;
                    }
                    if (end == bytes_.size() && sequenceSize < maximumSequenceLength && !expired) break;
                }
            } else if (available >= 2 && bytes_[offset + 1] != '\x1b') {
                InputModifiers modifiers; modifiers.alt = true;
                const std::size_t size = scalar(offset + 1, modifiers, now);
                if (!size && !expired) break;
                if (size) { offset += size + 1; waiting_ = false; continue; }
            } else if (available == 1 && !expired) break;
            events_.key(Key::Escape); ++offset; waiting_ = false;
        }
        bytes_.erase(0, offset);
    }
};

class Utf16Decoder {
public:
    explicit Utf16Decoder(EventBuffer& events) : events_(events) {}
    void reset() { high_ = 0; repeats_ = 0; }
    void flush() {
        if (high_) for (unsigned i = 0; i < repeats_; ++i) events_.text(0xfffd, modifiers_);
        reset();
    }
    void feed(std::uint16_t unit, unsigned repeat, InputModifiers modifiers) {
        if (!repeat) repeat = 1;
        if (unit >= 0xdc00 && unit <= 0xdfff && high_) {
            const std::uint32_t cp = 0x10000 + ((std::uint32_t(high_) - 0xd800) << 10) + unit - 0xdc00;
            const unsigned pairs = repeat < repeats_ ? repeat : repeats_;
            for (unsigned i = 0; i < pairs; ++i) events_.text(cp, modifiers_);
            for (unsigned i = pairs; i < repeats_; ++i) events_.text(0xfffd, modifiers_);
            for (unsigned i = pairs; i < repeat; ++i) events_.text(0xfffd, modifiers);
            reset(); return;
        }
        flush();
        if (unit >= 0xd800 && unit <= 0xdbff) { high_ = unit; repeats_ = repeat; modifiers_ = modifiers; }
        else for (unsigned i = 0; i < repeat; ++i) events_.text(unit, modifiers);
    }
private:
    EventBuffer& events_;
    std::uint16_t high_ = 0;
    unsigned repeats_ = 0;
    InputModifiers modifiers_;
};

#if defined(__linux__)

namespace linux_native {
enum : unsigned {
    EventSyn = 0, EventKey = 1, SyncReport = 0, SyncDropped = 3,
    KeyboardModeRequest = 0x4b44, KeyboardMapRequest = 0x4b46, ConsoleStateRequest = 0x5603,
    MapLatin = 0, MapLetter = 11, KeyMax = 0x2ff,
    KeyEsc = 1, Key1 = 2, Key9 = 10, Key0 = 11, KeyBackspace = 14, KeyTab = 15,
    KeyQ = 16, KeyW = 17, KeyE = 18, KeyR = 19, KeyT = 20, KeyY = 21, KeyU = 22,
    KeyI = 23, KeyO = 24, KeyP = 25, KeyEnter = 28, KeyLeftCtrl = 29,
    KeyA = 30, KeyS = 31, KeyD = 32, KeyF = 33, KeyG = 34, KeyH = 35, KeyJ = 36,
    KeyK = 37, KeyL = 38, KeyLeftShift = 42, KeyZ = 44, KeyX = 45, KeyC = 46,
    KeyV = 47, KeyB = 48, KeyN = 49, KeyM = 50, KeyRightShift = 54, KeyLeftAlt = 56,
    KeySpace = 57, KeyF1 = 59, KeyF10 = 68, KeyKp7 = 71, KeyKp8 = 72, KeyKp9 = 73,
    KeyKp4 = 75, KeyKp5 = 76, KeyKp6 = 77, KeyKp1 = 79, KeyKp2 = 80, KeyKp3 = 81,
    KeyKp0 = 82, KeyF11 = 87, KeyF12 = 88, KeyKpEnter = 96, KeyRightCtrl = 97,
    KeyRightAlt = 100, KeyHome = 102, KeyUp = 103, KeyPageUp = 104, KeyLeft = 105,
    KeyRight = 106, KeyEnd = 107, KeyDown = 108, KeyPageDown = 109, KeyInsert = 110, KeyDelete = 111
};
struct InputRecord {

#if __BITS_PER_LONG != 32 || !defined(__USE_TIME_BITS64)
    timeval timestamp;
    std::int64_t seconds() const { return static_cast<std::int64_t>(timestamp.tv_sec); }
    std::int64_t microseconds() const { return static_cast<std::int64_t>(timestamp.tv_usec); }
#else
    __kernel_ulong_t sec;
#if defined(__sparc__) && defined(__arch64__)
    unsigned int usec, padding;
#else
    __kernel_ulong_t usec;
#endif
    std::int64_t seconds() const { return static_cast<std::int64_t>(sec); }
    std::int64_t microseconds() const { return static_cast<std::int64_t>(usec); }
#endif
    std::uint16_t type, code;
    std::int32_t value;
};
struct KeymapEntry { unsigned char table, index; std::uint16_t value; };
struct ConsoleState { std::uint16_t active, signal, state; };
inline unsigned long keyBitsRequest(std::size_t size) {
    return static_cast<unsigned long>(_IOC(_IOC_READ, 'E', 0x20 + EventKey, size));
}
inline unsigned long keyStateRequest(std::size_t size) {
    return static_cast<unsigned long>(_IOC(_IOC_READ, 'E', 0x18, size));
}
}

class ConsoleKeyboard {
public:
    ConsoleKeyboard() { mapping_.fill(Key::Unknown); }
    ~ConsoleKeyboard() { closeDevices(); }
    ConsoleKeyboard(const ConsoleKeyboard&) = delete;
    ConsoleKeyboard& operator=(const ConsoleKeyboard&) = delete;
    void closeDevices() {
        for (auto& device : devices_) {
            if (device.fd >= 0) close(device.fd);
            device.fd = -1; device.keys.fill(0); device.resync = false;
        }
        tty_ = -1; number_ = 0; foreground_ = false;
    }
    bool initialize(int tty, EventBuffer& events) {
        closeDevices();
        unsigned deviceNumber = 0;
        int mode = 0;
        if (ioctl(tty, linux_native::KeyboardModeRequest, &mode) != 0 || ioctl(tty, TIOCGDEV, &deviceNumber) != 0 ||
            major(deviceNumber) != 4 || minor(deviceNumber) < 1 || minor(deviceNumber) > 63) return false;
        tty_ = tty; number_ = minor(deviceNumber);
        for (unsigned code = 0; code <= linux_native::KeyMax; ++code) mapping_[code] = logicalKey(code, tty);
        DIR* directory = opendir("/dev/input");
        if (!directory) { closeDevices(); return false; }
        std::size_t slot = 0;
        while (dirent* entry = readdir(directory)) {
            const std::string name = entry->d_name;
            if (name.size() <= 5 || name.compare(0, 5, "event") != 0 ||
                name.find_first_not_of("0123456789", 5) != std::string::npos) continue;
            if (slot == devices_.size()) break;
            const int fd = open(("/dev/input/" + name).c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (fd < 0) continue;
            Bits supported = {};
            if (ioctl(fd, linux_native::keyBitsRequest(sizeof(supported)), supported.data()) < 0 ||
                (!bit(supported, linux_native::KeyA) && !bit(supported, linux_native::KeyEnter))) { close(fd); continue; }
            Device& device = devices_[slot];
            if (ioctl(fd, linux_native::keyStateRequest(sizeof(device.keys)), device.keys.data()) < 0) { close(fd); continue; }
            device.fd = fd; ++slot;
        }
        closedir(directory);
        update(events);
        return available();
    }
    bool available() const {
        for (const auto& device : devices_) if (device.fd >= 0) return true;
        return false;
    }
    std::size_t appendDescriptors(pollfd* descriptors, std::size_t count) const {
        for (const auto& device : devices_)
            if (device.fd >= 0) descriptors[count++] = pollfd{device.fd, POLLIN, 0};
        return count;
    }
    bool update(EventBuffer& events) {
        if (tty_ < 0) return false;
        linux_native::ConsoleState state = {};
        const bool foreground = ioctl(tty_, linux_native::ConsoleStateRequest, &state) == 0 && state.active == number_;
        const bool resumed = foreground && !foreground_;
        bool activity = foreground != foreground_;
        foreground_ = foreground;
        unsigned budget = 4096;
        struct Pending { linux_native::InputRecord event; std::size_t device; unsigned order; };
        std::array<Pending, 4096> pending;
        unsigned pendingCount = 0;
        for (auto& device : devices_) {
            if (device.fd < 0) continue;
            if (resumed && ioctl(device.fd, linux_native::keyStateRequest(sizeof(device.keys)), device.keys.data()) < 0) {
                closeDevice(device); activity = true; continue;
            }
            while (budget && device.fd >= 0) {
                pollfd descriptor = {device.fd, POLLIN, 0};
                const int ready = poll(&descriptor, 1, 0);
                if (!ready || (ready < 0 && errno == EINTR)) break;
                if (ready < 0 || (descriptor.revents & (POLLERR | POLLNVAL | POLLHUP))) {
                    closeDevice(device); activity = true; break;
                }
                if (!(descriptor.revents & POLLIN)) break;
                linux_native::InputRecord records[64];
                const unsigned requested = budget < 64 ? budget : 64;
                const ssize_t bytes = read(device.fd, records, requested * sizeof(linux_native::InputRecord));
                if (bytes < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) break;
                if (bytes <= 0 || static_cast<std::size_t>(bytes) % sizeof(linux_native::InputRecord) != 0) {
                    closeDevice(device); activity = true; break;
                }
                const unsigned count = static_cast<unsigned>(static_cast<std::size_t>(bytes) / sizeof(linux_native::InputRecord));
                budget -= count; activity = true;
                for (unsigned i = 0; i < count; ++i) {
                    pending[pendingCount].event = records[i];
                    pending[pendingCount].device = static_cast<std::size_t>(&device - devices_.data());
                    pending[pendingCount].order = pendingCount; ++pendingCount;
                }
            }
        }

        std::sort(pending.begin(), pending.begin() + pendingCount, [](const Pending& a, const Pending& b) {
            if (a.event.seconds() != b.event.seconds()) return a.event.seconds() < b.event.seconds();
            if (a.event.microseconds() != b.event.microseconds()) return a.event.microseconds() < b.event.microseconds();
            return a.order < b.order;
        });
        for (unsigned i = 0; i < pendingCount; ++i) {
            Device& device = devices_[pending[i].device];
            if (device.fd < 0) continue;
            const linux_native::InputRecord& record = pending[i].event;
            if (record.type == linux_native::EventSyn && record.code == linux_native::SyncDropped) device.resync = true;
            else if (device.resync) {
                if (record.type == linux_native::EventSyn && record.code == linux_native::SyncReport) {
                    if (ioctl(device.fd, linux_native::keyStateRequest(sizeof(device.keys)), device.keys.data()) < 0) closeDevice(device);
                    device.resync = false;
                }
            } else if (record.type == linux_native::EventKey && record.code <= linux_native::KeyMax && record.value >= 0 && record.value <= 2)
                setBit(device.keys, record.code, record.value != 0);
            publish(events);
        }
        publish(events); return activity;
    }
private:
    typedef std::array<unsigned char, (linux_native::KeyMax + 8) / 8> Bits;
    struct Device { int fd = -1; Bits keys = {}; bool resync = false; };
    std::array<Device, 16> devices_;
    std::array<Key, linux_native::KeyMax + 1> mapping_;
    int tty_ = -1;
    unsigned number_ = 0;
    bool foreground_ = false;
    static bool bit(const Bits& bits, unsigned code) { return (bits[code / 8] & (1u << (code % 8))) != 0; }
    static void setBit(Bits& bits, unsigned code, bool down) {
        const unsigned mask = 1u << (code % 8);
        bits[code / 8] = static_cast<unsigned char>(down ? (bits[code / 8] | mask) : (bits[code / 8] & ~mask));
    }
    static void closeDevice(Device& device) {
        close(device.fd); device.fd = -1; device.keys.fill(0); device.resync = false;
    }
    void publish(EventBuffer& events) const {
        std::array<bool, std::size_t(Key::Count)> down = {};
        if (foreground_) {
            for (const auto& device : devices_) {
                if (device.fd < 0) continue;
                for (unsigned code = 0; code <= linux_native::KeyMax; ++code)
                    if (validKey(mapping_[code]) && bit(device.keys, code))
                        down[static_cast<std::size_t>(mapping_[code])] = true;
            }
        }
        for (std::size_t i = 0; i < down.size(); ++i) events.setKey(static_cast<Key>(i), down[i]);
    }
    static Key logicalKey(unsigned code, int tty) {
        if (code >= linux_native::KeyF1 && code <= linux_native::KeyF10) return static_cast<Key>(int(Key::F1) + int(code - linux_native::KeyF1));
        if (code == linux_native::KeyF11) return Key::F11;
        if (code == linux_native::KeyF12) return Key::F12;
        switch (code) {
        case linux_native::KeyEsc: return Key::Escape; case linux_native::KeyEnter: case linux_native::KeyKpEnter: return Key::Enter;
        case linux_native::KeyTab: return Key::Tab; case linux_native::KeyBackspace: return Key::Backspace;
        case linux_native::KeySpace: return Key::Space; case linux_native::KeyUp: return Key::ArrowUp; case linux_native::KeyDown: return Key::ArrowDown;
        case linux_native::KeyLeft: return Key::ArrowLeft; case linux_native::KeyRight: return Key::ArrowRight;
        case linux_native::KeyHome: return Key::Home; case linux_native::KeyEnd: return Key::End;
        case linux_native::KeyPageUp: return Key::PageUp; case linux_native::KeyPageDown: return Key::PageDown;
        case linux_native::KeyInsert: return Key::Insert; case linux_native::KeyDelete: return Key::Delete;
        case linux_native::KeyLeftShift: case linux_native::KeyRightShift: return Key::Shift;
        case linux_native::KeyLeftCtrl: case linux_native::KeyRightCtrl: return Key::Ctrl;
        case linux_native::KeyLeftAlt: case linux_native::KeyRightAlt: return Key::Alt;
        case linux_native::KeyKp0: return Key::Num0; case linux_native::KeyKp1: return Key::Num1; case linux_native::KeyKp2: return Key::Num2;
        case linux_native::KeyKp3: return Key::Num3; case linux_native::KeyKp4: return Key::Num4; case linux_native::KeyKp5: return Key::Num5;
        case linux_native::KeyKp6: return Key::Num6; case linux_native::KeyKp7: return Key::Num7; case linux_native::KeyKp8: return Key::Num8;
        case linux_native::KeyKp9: return Key::Num9;
        default: break;
        }
        if (code < 256) {
            linux_native::KeymapEntry entry = {}; entry.index = static_cast<unsigned char>(code);
            if (ioctl(tty, linux_native::KeyboardMapRequest, &entry) == 0) {
                if ((entry.value >> 8) == linux_native::MapLatin || (entry.value >> 8) == linux_native::MapLetter)
                    return asciiKey(static_cast<unsigned char>((entry.value & 0xffu)));
                return Key::Unknown;
            }
        }

        const unsigned letters[] = {linux_native::KeyA, linux_native::KeyB, linux_native::KeyC, linux_native::KeyD, linux_native::KeyE, linux_native::KeyF, linux_native::KeyG, linux_native::KeyH, linux_native::KeyI,
            linux_native::KeyJ, linux_native::KeyK, linux_native::KeyL, linux_native::KeyM, linux_native::KeyN, linux_native::KeyO, linux_native::KeyP, linux_native::KeyQ, linux_native::KeyR, linux_native::KeyS, linux_native::KeyT, linux_native::KeyU,
            linux_native::KeyV, linux_native::KeyW, linux_native::KeyX, linux_native::KeyY, linux_native::KeyZ};
        for (unsigned i = 0; i < 26; ++i) if (code == letters[i]) return static_cast<Key>(int(Key::A) + int(i));
        if (code >= linux_native::Key1 && code <= linux_native::Key9) return static_cast<Key>(int(Key::Num1) + int(code - linux_native::Key1));
        return code == linux_native::Key0 ? Key::Num0 : Key::Unknown;
    }
};
#endif

class EmergencyRecovery {
public:
    struct RestoreData {
        bool saveInput = false;
        bool discardInput = false;
#if defined(_WIN32)
        HANDLE input = INVALID_HANDLE_VALUE;
        DWORD mode = 0;
#else
        int input = -1, output = -1;
        termios mode = {};
        char controls[384] = {};
        std::size_t controlSize = 0;
        void append(const char* bytes) {
            for (std::size_t i = 0; bytes[i] && controlSize < sizeof(controls); ++i) controls[controlSize++] = bytes[i];
        }
#endif
    };
    EmergencyRecovery() { registry(); }
    ~EmergencyRecovery() { stop(); }
    EmergencyRecovery(const EmergencyRecovery&) = delete;
    EmergencyRecovery& operator=(const EmergencyRecovery&) = delete;
    bool installed() const { return installed_; }
    unsigned long interruption() const { return installed_ ? registry().interrupted.load() : 0; }
    unsigned long error() const { return error_; }
#if defined(_WIN32)
    HANDLE wakeHandle() const { return installed_ ? wake_ : nullptr; }
#else
    int wakeHandle() const { return installed_ ? wake_[0] : -1; }
#endif
    bool start(const RestoreData& data) {
        if (installed_) return publish(data);
        error_ = 0;
#if defined(__linux__)
        if (alternateStack_) {
            closeWake();
            if (alternateStack_) { error_ = static_cast<unsigned long>(errno); return false; }
        }
#endif
        previousTerminate_ = std::get_terminate();
#if defined(_WIN32)
        wake_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!wake_) { error_ = GetLastError(); return false; }
#else
        if (pipe2(wake_, O_CLOEXEC | O_NONBLOCK) != 0) { error_ = static_cast<unsigned long>(errno); return false; }
        if (sigaltstack(nullptr, &previousStack_) != 0) { error_ = static_cast<unsigned long>(errno); closeWake(); return false; }
        if (previousStack_.ss_flags & SS_DISABLE) {
            const std::size_t requested = static_cast<std::size_t>(SIGSTKSZ);
            alternateSize_ = requested > 65536 ? requested : 65536;
            alternateStack_ = new (std::nothrow) char[alternateSize_];
            if (!alternateStack_) { error_ = allocationError(); closeWake(); return false; }
            stack_t stack = {}; stack.ss_sp = alternateStack_; stack.ss_size = alternateSize_;
            if (sigaltstack(&stack, nullptr) != 0) { error_ = static_cast<unsigned long>(errno); closeWake(); return false; }
            stackInstalled_ = true;
        }
        for (unsigned i = 0; i < SignalCount; ++i) {
            if (sigaction(signalNumber(i), nullptr, &previousSignals_[i]) != 0) {
                error_ = static_cast<unsigned long>(errno); closeWake(); return false;
            }
        }
#endif
        standby_ = makeSnapshot(nullptr);
        if (!standby_) { closeWake(); return false; }
        installed_ = true;
        registry().interrupted.store(0); registry().claimed.store(false);
        if (!publish(data)) { stop(); return false; }
#if defined(__linux__) || defined(_MSC_VER) || (defined(_GLIBCXX_HAVE_AT_QUICK_EXIT) && _GLIBCXX_HAVE_AT_QUICK_EXIT)
        if (!registry().quickRegistered) {
            if (std::at_quick_exit(&onQuickExit) != 0) { error_ = allocationError(); stop(); return false; }
            registry().quickRegistered = true;
        }
#endif
#if defined(_WIN32)
        previousFilter_ = SetUnhandledExceptionFilter(&onException); filterInstalled_ = true;
        for (unsigned i = 0; i < SignalCount; ++i) {
            previousSignals_[i] = std::signal(signalNumber(i), &onCrtSignal);
            if (previousSignals_[i] == SIG_ERR) { error_ = ERROR_INVALID_FUNCTION; stop(); return false; }
            if (previousSignals_[i] == SIG_IGN) std::signal(signalNumber(i), SIG_IGN);
            else signalInstalled_[i] = true;
        }
        if (!SetConsoleCtrlHandler(&onControl, TRUE)) { error_ = GetLastError(); stop(); return false; }
        controlInstalled_ = true;
#else
        for (unsigned i = 0; i < SignalCount; ++i) {
            if (previousSignals_[i].sa_handler == SIG_IGN) continue;
            struct sigaction action = previousSignals_[i];
            action.sa_sigaction = &onSignal;
            action.sa_flags |= SA_SIGINFO | SA_ONSTACK;
            if (sigaction(signalNumber(i), &action, nullptr) != 0) {
                error_ = static_cast<unsigned long>(errno); stop(); return false;
            }
            signalInstalled_[i] = true;
        }
#endif
        previousTerminate_ = std::set_terminate(&onTerminate); terminateInstalled_ = true;

        copyHandlers(*standby_);
        if (!publish(data)) { stop(); return false; }
        return true;
    }
    bool publish(const RestoreData& data) {
        if (!installed_) return true;
        Snapshot* snapshot = makeSnapshot(&data);
        if (!snapshot) return false;
        replace(snapshot); return true;
    }
    void stop() {
        if (!installed_) return;

        copyHandlers(*standby_);
        replace(standby_); standby_ = nullptr;
        if (terminateInstalled_ && std::get_terminate() == &onTerminate) std::set_terminate(previousTerminate_);
        terminateInstalled_ = false;
#if defined(_WIN32)
        if (controlInstalled_) SetConsoleCtrlHandler(&onControl, FALSE);
        controlInstalled_ = false;
        if (filterInstalled_) {
            const auto current = SetUnhandledExceptionFilter(previousFilter_);
            if (current != &onException) SetUnhandledExceptionFilter(current);
        }
        filterInstalled_ = false;
        for (unsigned i = 0; i < SignalCount; ++i) if (signalInstalled_[i]) {
            const auto current = std::signal(signalNumber(i), previousSignals_[i]);
            if (current != &onCrtSignal && current != SIG_ERR) std::signal(signalNumber(i), current);
            signalInstalled_[i] = false;
        }
#else
        for (unsigned i = 0; i < SignalCount; ++i) if (signalInstalled_[i]) {
            struct sigaction current = {};
            if (sigaction(signalNumber(i), nullptr, &current) == 0 &&
                (current.sa_flags & SA_SIGINFO) && current.sa_sigaction == &onSignal)
                sigaction(signalNumber(i), &previousSignals_[i], nullptr);
            signalInstalled_[i] = false;
        }
#endif
        closeWake(); installed_ = false;
    }
private:
    static_assert(ATOMIC_POINTER_LOCK_FREE == 2 && ATOMIC_INT_LOCK_FREE == 2 &&
                  ATOMIC_BOOL_LOCK_FREE == 2 && ATOMIC_LONG_LOCK_FREE == 2,
                  "Emergency recovery requires lock-free native atomics");
#if defined(_WIN32)
    enum { SignalCount = 2 };
    typedef void (*SignalHandler)(int);
    static int signalNumber(unsigned i) { const int signals[] = {SIGABRT, SIGTERM}; return signals[i]; }
#else
    enum { SignalCount = 10 };
    static int signalNumber(unsigned i) {
        const int signals[] = {SIGINT, SIGTERM, SIGHUP, SIGQUIT, SIGABRT, SIGSEGV, SIGILL, SIGFPE, SIGBUS, SIGPIPE};
        return signals[i];
    }
#endif
    struct Snapshot {
        RestoreData data;
        bool active = false;
        std::terminate_handler previousTerminate = nullptr;
#if defined(_WIN32)
        HANDLE wake = nullptr;
        LPTOP_LEVEL_EXCEPTION_FILTER previousFilter = nullptr;
        SignalHandler previousSignals[SignalCount] = {};
#else
        int wake = -1;
        pid_t owner = 0;
        struct sigaction previousSignals[SignalCount] = {};
#endif
        ~Snapshot() {
#if defined(_WIN32)
            if (data.input != INVALID_HANDLE_VALUE) CloseHandle(data.input);
#else
            if (data.input >= 0) close(data.input);
            if (data.output >= 0) close(data.output);
#endif
        }
    };
    struct Registry {
        std::atomic<Snapshot*> current{nullptr};
        std::atomic<unsigned> readers{0};
        std::atomic<unsigned long> interrupted{0};
        std::atomic<bool> claimed{false};
        bool quickRegistered = false;
        ~Registry() { delete current.load(); }
    };
    static Registry& registry() { static Registry state; return state; }
    bool installed_ = false, terminateInstalled_ = false;
    bool signalInstalled_[SignalCount] = {};
    std::terminate_handler previousTerminate_ = nullptr;
    Snapshot* standby_ = nullptr;
    unsigned long error_ = 0;
#if defined(_WIN32)
    HANDLE wake_ = nullptr;
    LPTOP_LEVEL_EXCEPTION_FILTER previousFilter_ = nullptr;
    SignalHandler previousSignals_[SignalCount] = {};
    bool filterInstalled_ = false, controlInstalled_ = false;
#else
    int wake_[2] = {-1, -1};
    struct sigaction previousSignals_[SignalCount] = {};
    stack_t previousStack_ = {};
    char* alternateStack_ = nullptr;
    std::size_t alternateSize_ = 0;
    bool stackInstalled_ = false;
#endif
    static unsigned long allocationError() {
#if defined(_WIN32)
        return ERROR_NOT_ENOUGH_MEMORY;
#else
        return static_cast<unsigned long>(ENOMEM);
#endif
    }
    void closeWake() {
#if defined(_WIN32)
        if (wake_) CloseHandle(wake_);
        wake_ = nullptr;
#else
        if (stackInstalled_) {
            stack_t current = {};
            if (sigaltstack(nullptr, &current) == 0 &&
                (current.ss_sp != alternateStack_ || sigaltstack(&previousStack_, nullptr) == 0))
                stackInstalled_ = false;
        }

        if (!stackInstalled_) { delete[] alternateStack_; alternateStack_ = nullptr; alternateSize_ = 0; }
        if (wake_[0] >= 0) close(wake_[0]);
        if (wake_[1] >= 0) close(wake_[1]);
        wake_[0] = wake_[1] = -1;
#endif
    }
    void copyHandlers(Snapshot& snapshot) const {
        snapshot.previousTerminate = previousTerminate_;
        for (unsigned i = 0; i < SignalCount; ++i) snapshot.previousSignals[i] = previousSignals_[i];
#if defined(_WIN32)
        snapshot.previousFilter = previousFilter_;
#endif
    }
    Snapshot* makeSnapshot(const RestoreData* data) {
        Snapshot* snapshot = new (std::nothrow) Snapshot;
        if (!snapshot) { error_ = allocationError(); return nullptr; }
        copyHandlers(*snapshot);
        if (!data) return snapshot;
        snapshot->data = *data; snapshot->active = true;
#if defined(_WIN32)
        snapshot->data.input = INVALID_HANDLE_VALUE; snapshot->wake = wake_;
        if (data->saveInput && !DuplicateHandle(GetCurrentProcess(), data->input, GetCurrentProcess(),
                &snapshot->data.input, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            error_ = GetLastError(); delete snapshot; return nullptr;
        }
#else
        snapshot->data.input = snapshot->data.output = -1; snapshot->wake = wake_[1]; snapshot->owner = getpid();
        if (data->saveInput) snapshot->data.input = fcntl(data->input, F_DUPFD_CLOEXEC, 0);
        if ((data->saveInput && snapshot->data.input < 0) ||
            (data->controlSize && (snapshot->data.output = fcntl(data->output, F_DUPFD_CLOEXEC, 0)) < 0)) {
            error_ = static_cast<unsigned long>(errno); delete snapshot; return nullptr;
        }
#endif
        return snapshot;
    }
    static void replace(Snapshot* snapshot) {
        Registry& state = registry();
        Snapshot* previous = state.current.exchange(snapshot);
        while (state.readers.load() != 0) {}
        delete previous;
    }
    static Snapshot* acquire() { registry().readers.fetch_add(1); return registry().current.load(); }
    static void release() { registry().readers.fetch_sub(1); }
    static void restore(Snapshot* snapshot, unsigned long reason) noexcept {
        if (!snapshot || !snapshot->active) return;
#if defined(__linux__)
        if (snapshot->owner != getpid()) return;
#endif
        Registry& state = registry();
        unsigned long expected = 0;
        state.interrupted.compare_exchange_strong(expected, reason);
        if (!state.claimed.exchange(true)) {
#if defined(_WIN32)
            if (snapshot->data.discardInput) FlushConsoleInputBuffer(snapshot->data.input);
            if (snapshot->data.saveInput) SetConsoleMode(snapshot->data.input, snapshot->data.mode);
#else
            sigset_t blocked, oldMask;
            sigemptyset(&blocked); sigaddset(&blocked, SIGTTOU);
            const bool maskSaved = sigprocmask(SIG_BLOCK, &blocked, &oldMask) == 0;
            std::size_t offset = 0; unsigned interruptions = 0;
            while (offset < snapshot->data.controlSize) {
                const ssize_t count = write(snapshot->data.output, snapshot->data.controls + offset, snapshot->data.controlSize - offset);
                if (count > 0) offset += static_cast<std::size_t>(count);
                else if (count < 0 && errno == EINTR && ++interruptions < 4) continue;
                else break;
            }
            if (snapshot->data.saveInput) {
                if (snapshot->data.discardInput) tcflush(snapshot->data.input, TCIFLUSH);
                for (unsigned i = 0; i < 4; ++i)
                    if (tcsetattr(snapshot->data.input, TCSANOW, &snapshot->data.mode) == 0 || errno != EINTR) break;
            }
            if (maskSaved) sigprocmask(SIG_SETMASK, &oldMask, nullptr);
#endif
        }
#if defined(_WIN32)
        SetEvent(snapshot->wake);
#else
        const char byte = 1;
        const ssize_t ignored = write(snapshot->wake, &byte, 1); (void)ignored;
#endif
    }
    static void onQuickExit() noexcept {
        Snapshot* snapshot = acquire(); restore(snapshot, static_cast<unsigned long>(SIGABRT)); release();
    }
    static void onTerminate() noexcept {
        Snapshot* snapshot = acquire();
        const auto previous = snapshot ? snapshot->previousTerminate : nullptr;
        restore(snapshot, static_cast<unsigned long>(SIGABRT)); release();
        if (previous && previous != &onTerminate) previous();
        std::abort();
    }
#if defined(_WIN32)
    static BOOL WINAPI onControl(DWORD event) {
        if (event != CTRL_C_EVENT && event != CTRL_BREAK_EVENT && event != CTRL_CLOSE_EVENT &&
            event != CTRL_LOGOFF_EVENT && event != CTRL_SHUTDOWN_EVENT) return FALSE;
        const DWORD saved = GetLastError();
        Snapshot* snapshot = acquire(); restore(snapshot, ERROR_OPERATION_ABORTED); release();
        SetLastError(saved); return FALSE;
    }
    static LONG WINAPI onException(EXCEPTION_POINTERS* exception) {
        Snapshot* snapshot = acquire(); const auto previous = snapshot ? snapshot->previousFilter : nullptr;
        restore(snapshot, exception->ExceptionRecord->ExceptionCode); release();
        return previous && previous != &onException ? previous(exception) : EXCEPTION_CONTINUE_SEARCH;
    }
    static void onCrtSignal(int signal) {
        Snapshot* snapshot = acquire(); SignalHandler previous = SIG_DFL;
        if (snapshot) for (unsigned i = 0; i < SignalCount; ++i)
            if (signalNumber(i) == signal) previous = snapshot->previousSignals[i];
        restore(snapshot, static_cast<unsigned long>(signal)); release();
        if (previous == SIG_IGN) return;
        if (previous != SIG_DFL && previous != SIG_IGN && previous != SIG_ERR && previous != &onCrtSignal) previous(signal);
        else { std::signal(signal, SIG_DFL); std::raise(signal); }
    }
#else
    static void onSignal(int signal, siginfo_t* information, void* context) {
        const int saved = errno;
        Snapshot* snapshot = acquire(); struct sigaction previous = {};
        if (snapshot) for (unsigned i = 0; i < SignalCount; ++i)
            if (signalNumber(i) == signal) previous = snapshot->previousSignals[i];
        restore(snapshot, static_cast<unsigned long>(signal)); release(); errno = saved;
        if (previous.sa_handler == SIG_IGN) return;
        if (previous.sa_handler == SIG_DFL) {
            sigaction(signal, &previous, nullptr);

            if (information && information->si_code > 0 &&
                (signal == SIGSEGV || signal == SIGILL || signal == SIGFPE || signal == SIGBUS)) return;
            if (raise(signal) != 0) _exit(128 + signal);
        } else if (previous.sa_flags & SA_SIGINFO) previous.sa_sigaction(signal, information, context);
        else previous.sa_handler(signal);
    }
#endif
};

class Manager {
public:
    static Manager& instance() { static Manager manager; return manager; }
    Manager(const Manager&) = delete;
    Manager& operator=(const Manager&) = delete;
    ~Manager() { shutdown(); }

    bool initialize(const InputOptions& options) {
        if (recovery_.interruption() && !shutdown()) return false;
        if (active_) return true;
        if (needsRestore() && !shutdown()) return false;
        error_ = InputError::NoError; nativeError_ = 0;
        if (!options.queueCapacity || !options.lineCapacity || options.escapeTimeoutMs < 0 ||
            options.keyboardProtocolTimeoutMs < 1 || options.sequenceTimeoutMs < 1 ||
            (options.mouseTracking != MouseTracking::Buttons && options.mouseTracking != MouseTracking::Drag &&
             options.mouseTracking != MouseTracking::AllMotion)) return fail(InputError::InvalidOptions, 0);
        options_ = options;
        events_.reset(options.queueCapacity, options.lineCapacity, options.coalesceMouseMoves, options.prioritizeInput);
        parser_.reset(options.escapeTimeoutMs, options.enableMouse, options.mouseTracking,
                      options.sequenceTimeoutMs); utf16_.reset();
        mouseEnabled_ = options.enableMouse;
#if defined(_WIN32)
        input_ = GetStdHandle(STD_INPUT_HANDLE);
        if (input_ == INVALID_HANDLE_VALUE || !GetConsoleMode(input_, &originalMode_))
            return fail(InputError::NotTerminal, GetLastError());
        DWORD mode = (originalMode_ | ENABLE_EXTENDED_FLAGS) &
            ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT |
              ENABLE_QUICK_EDIT_MODE | ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT |
              ENABLE_VIRTUAL_TERMINAL_INPUT);
        if (mouseEnabled_) {
            output_ = CreateFileW(L"CONOUT$", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 nullptr, OPEN_EXISTING, 0, nullptr);
            if (output_ == INVALID_HANDLE_VALUE) return fail(InputError::ConfigureFailed, GetLastError());
            mode |= ENABLE_MOUSE_INPUT;
        }
        modeSaved_ = true;
        if (!refreshRecovery()) {
            const unsigned long saved = recovery_.error();
            if (!shutdown()) return false;
            return fail(InputError::ConfigureFailed, saved);
        }
        if (!SetConsoleMode(input_, mode)) {
            const DWORD saved = GetLastError();
            if (!shutdown()) return false;
            return fail(InputError::ConfigureFailed, saved);
        }
#else
        events_.inferPress(false);
        if (!isatty(STDIN_FILENO)) return fail(InputError::NotTerminal, 0);
        input_ = fcntl(STDIN_FILENO, F_DUPFD_CLOEXEC, 0);
        if (input_ < 0) return fail(InputError::ConfigureFailed, static_cast<unsigned long>(errno));
        if (tcgetattr(input_, &originalMode_) != 0) {
            const int saved = errno; close(input_); input_ = -1;
            return fail(InputError::ConfigureFailed, static_cast<unsigned long>(saved));
        }
        termios mode = originalMode_;
        mode.c_iflag &= ~static_cast<tcflag_t>(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON | IXOFF | INPCK);
        mode.c_lflag &= ~static_cast<tcflag_t>(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
        if (options.preserveSignals) mode.c_lflag |= originalMode_.c_lflag & ISIG;
        mode.c_cflag = (mode.c_cflag & ~static_cast<tcflag_t>(CSIZE | PARENB)) | CS8;
        mode.c_cc[VMIN] = 0; mode.c_cc[VTIME] = 0;

        modeSaved_ = true;
        if (!refreshRecovery()) {
            const unsigned long saved = recovery_.error();
            if (!shutdown()) return false;
            return fail(InputError::ConfigureFailed, saved);
        }
        if (tcsetattr(input_, TCSANOW, &mode) != 0) {
            const int saved = errno;
            if (!shutdown()) return false;
            return fail(InputError::ConfigureFailed, static_cast<unsigned long>(saved));
        }
        if (options.enableKeyboardState) consoleKeyboard_.initialize(input_, events_);
        const bool probeKeyboard = options.enableKeyboardState && !consoleKeyboard_.available();
        if (mouseEnabled_ || options.enableBracketedPaste || options.enableFocusEvents || probeKeyboard) {
            const char* path = ttyname(input_);
            output_ = path ? open(path, O_WRONLY | O_NOCTTY | O_NONBLOCK | O_CLOEXEC) : -1;
            if (output_ < 0 && (mouseEnabled_ || options.enableBracketedPaste || options.enableFocusEvents)) {
                const int saved = errno;
                if (!shutdown()) return false;
                return fail(InputError::ConfigureFailed, static_cast<unsigned long>(saved));
            }
        }
        if (mouseEnabled_) {

            if (!writeControl("\x1b[?9;1000;1001;1002;1003;1005;1006;1015;1016s")) {
                const int saved = errno; close(output_); output_ = -1;
                if (!shutdown()) return false;
                return fail(InputError::ConfigureFailed, static_cast<unsigned long>(saved));
            }
            mouseModeSaved_ = true;
            if (!refreshRecovery()) {
                const unsigned long saved = recovery_.error();
                if (!shutdown()) return false;
                return fail(InputError::ConfigureFailed, saved);
            }
            const char* tracking = options.mouseTracking == MouseTracking::Buttons ? "\x1b[?1000;1006h" :
                (options.mouseTracking == MouseTracking::Drag ? "\x1b[?1002;1006h" : "\x1b[?1003;1006h");
            if (!writeControl("\x1b[?9;1000;1001;1002;1003;1005;1006;1015;1016l") || !writeControl(tracking)) {
                const int saved = errno;
                if (!shutdown()) return false;
                return fail(InputError::ConfigureFailed, static_cast<unsigned long>(saved));
            }
        }
        if (options.enableBracketedPaste || options.enableFocusEvents) {

            if (!writeControl(options.enableBracketedPaste ?
                (options.enableFocusEvents ? "\x1b[?2004;1004s" : "\x1b[?2004s") : "\x1b[?1004s")) {
                const int saved = errno;
                if (!shutdown()) return false;
                return fail(InputError::ConfigureFailed, static_cast<unsigned long>(saved));
            }
            extraModesSaved_ = true;
            if (!refreshRecovery()) {
                const unsigned long saved = recovery_.error();
                if (!shutdown()) return false;
                return fail(InputError::ConfigureFailed, saved);
            }
            if (!writeControl(options.enableBracketedPaste ?
                (options.enableFocusEvents ? "\x1b[?2004;1004h" : "\x1b[?2004h") : "\x1b[?1004h")) {
                const int saved = errno;
                if (!shutdown()) return false;
                return fail(InputError::ConfigureFailed, static_cast<unsigned long>(saved));
            }
        }
        if (probeKeyboard && output_ >= 0 && writeControl("\x1b[?u\x1b[c")) {
            keyboardPhase_ = 1; parser_.expectKeyboardReply(false);
            keyboardDeadline_ = TerminalParser::Clock::now() + std::chrono::milliseconds(options.keyboardProtocolTimeoutMs);
        }
#endif
        active_ = true;
        return checkInterruption();
    }
    bool shutdown() {
        if (active_ && options_.discardPendingInputOnShutdown) flushPending_ = true;
        active_ = false; events_.reset(4096); parser_.reset(30); utf16_.reset();
#if defined(_WIN32)
        if (output_ != INVALID_HANDLE_VALUE) { CloseHandle(output_); output_ = INVALID_HANDLE_VALUE; }
        unsigned long restoreError = 0;
        if (flushPending_) {
            if (FlushConsoleInputBuffer(input_)) flushPending_ = false;
            else restoreError = GetLastError();
        }
        if (modeSaved_) {
            if (SetConsoleMode(input_, originalMode_)) modeSaved_ = false;
            else if (!restoreError) restoreError = GetLastError();
        }
        if (!modeSaved_ && !flushPending_) input_ = INVALID_HANDLE_VALUE;
        if (restoreError) return fail(InputError::RestoreFailed, restoreError);
#else
        consoleKeyboard_.closeDevices(); keyboardPhase_ = 0;
        keyboardCommand_ = nullptr; keyboardCommandOffset_ = 0;
        unsigned long restoreError = 0;

        if (mouseModeSaved_) {
            if (writeControl("\x1b[?9;1000;1001;1002;1003;1005;1006;1015;1016l"
                              "\x1b[?9;1000;1001;1002;1003;1005;1006;1015;1016r")) {
                mouseModeSaved_ = false;
            } else restoreError = static_cast<unsigned long>(errno);
        }
        if (extraModesSaved_) {
            if (writeControl(options_.enableBracketedPaste ?
                (options_.enableFocusEvents ? "\x1b[?2004;1004r" : "\x1b[?2004r") : "\x1b[?1004r"))
                extraModesSaved_ = false;
            else if (!restoreError) restoreError = static_cast<unsigned long>(errno);
        }
        if (keyboardModeSaved_) {
            if (writeControl(keyboardRestoreControl_.c_str())) keyboardModeSaved_ = false;
            else if (!restoreError) restoreError = static_cast<unsigned long>(errno);
        }
        if (!mouseModeSaved_ && !extraModesSaved_ && !keyboardModeSaved_ && output_ >= 0) { close(output_); output_ = -1; }
        if (flushPending_) {
            if (tcflush(input_, TCIFLUSH) == 0) flushPending_ = false;
            else if (!restoreError) restoreError = static_cast<unsigned long>(errno);
        }
        if (modeSaved_) {
            if (tcsetattr(input_, TCSANOW, &originalMode_) == 0) {
                modeSaved_ = false;
            } else if (!restoreError) restoreError = static_cast<unsigned long>(errno);
        }
        if (!modeSaved_ && !flushPending_ && input_ >= 0) { close(input_); input_ = -1; }
        if (restoreError) return fail(InputError::RestoreFailed, restoreError);
#endif
        recovery_.stop();
        error_ = InputError::NoError; nativeError_ = 0;
        return true;
    }
    bool update() {
        if (!checkInterruption()) return false;
        events_.beginFrame();
        activity_ = false;
        if (!active()) return fail(InputError::NotInitialized, 0);
        error_ = InputError::NoError; nativeError_ = 0;
        const std::size_t dropped = events_.dropped();
#if defined(_WIN32)
        DWORD available = 0;
        if (!GetNumberOfConsoleInputEvents(input_, &available)) return readFailure(GetLastError());

        if (available > 4096) available = 4096;
        while (available) {
            INPUT_RECORD records[64]; DWORD count = 0;
            const DWORD requested = available < 64 ? available : 64;
            if (!ReadConsoleInputW(input_, records, requested, &count)) return readFailure(GetLastError());
            if (!count) break;
            activity_ = true;
            available -= count;
            for (DWORD i = 0; i < count; ++i) {
                if (records[i].EventType == KEY_EVENT) processKey(records[i].Event.KeyEvent);
                else if (records[i].EventType == MOUSE_EVENT && mouseEnabled_ &&
                         !processMouse(records[i].Event.MouseEvent)) return readFailure(GetLastError());
                else if (records[i].EventType == FOCUS_EVENT) {
                    utf16_.flush(); events_.focus(records[i].Event.FocusEvent.bSetFocus != FALSE);
                }
            }
        }
#else
        activity_ = consoleKeyboard_.update(events_);
        if (!advanceKeyboard(TerminalParser::Clock::now())) return false;
        std::size_t budget = 4096;
        while (budget) {
            pollfd descriptor = { input_, POLLIN, 0 };
            const int ready = poll(&descriptor, 1, 0);
            if (ready < 0) {
                if (errno == EINTR) break;
                return readFailure(static_cast<unsigned long>(errno));
            }
            if (!ready) break;
            if (descriptor.revents & (POLLERR | POLLNVAL | POLLHUP)) return readFailure(0);
            if (!(descriptor.revents & POLLIN)) break;
            char bytes[256];
            const std::size_t requested = budget < sizeof(bytes) ? budget : sizeof(bytes);
            const ssize_t count = read(input_, bytes, requested);
            if (count < 0) {
                if (errno == EINTR || errno == EAGAIN) break;
                return readFailure(static_cast<unsigned long>(errno));
            }
            if (!count) break;
            activity_ = true;
            budget -= static_cast<std::size_t>(count);
            parser_.feed(bytes, static_cast<std::size_t>(count), TerminalParser::Clock::now());
            if (!advanceKeyboard(TerminalParser::Clock::now())) return false;
        }
        parser_.tick(TerminalParser::Clock::now());
        if (!advanceKeyboard(TerminalParser::Clock::now())) return false;
#endif
        if (!checkInterruption()) return false;

        if (events_.dropped() != dropped) error_ = InputError::QueueFull;
        return true;
    }
    bool wait(int timeoutMs) {
        if (timeoutMs < -1) return fail(InputError::InvalidOptions, 0);
        if (!update()) return false;
        if (events_.pending() || activity_ || timeoutMs == 0) return true;
#if defined(_WIN32)
        HANDLE handles[2] = {input_, recovery_.wakeHandle()};
        const DWORD status = WaitForMultipleObjects(handles[1] ? 2 : 1, handles, FALSE,
            timeoutMs < 0 ? INFINITE : static_cast<DWORD>(timeoutMs));
        if (status == WAIT_FAILED) return readFailure(GetLastError());
#else
        pollfd descriptors[19]; descriptors[0] = pollfd{input_, POLLIN, 0};
        std::size_t count = consoleKeyboard_.appendDescriptors(descriptors, 1);
        if (keyboardCommand_) descriptors[count++] = pollfd{output_, POLLOUT, 0};
        if (recovery_.wakeHandle() >= 0) descriptors[count++] = pollfd{recovery_.wakeHandle(), POLLIN, 0};
        const int timeout = keyboardWaitTime(parser_.waitTime(timeoutMs, TerminalParser::Clock::now()));
        if (poll(descriptors, static_cast<nfds_t>(count), timeout) < 0 && errno != EINTR)
            return readFailure(static_cast<unsigned long>(errno));
#endif
        return update();
    }
    bool active() const { return active_ && !recovery_.interruption(); }
    bool needsRestore() const { return modeSaved_ || mouseModeSaved_ || extraModesSaved_ || keyboardModeSaved_ || flushPending_; }
    InputError error() const { return error_; }
    unsigned long nativeError() const { return nativeError_; }
    InputCapabilities capabilities() const {
        InputCapabilities caps;
        caps.text = caps.specialKeys = active();
        caps.mouse = active() && mouseEnabled_;
        if (caps.mouse) {
            caps.mouseButtons = events_.buttons();
            caps.mouseVerticalWheel = events_.verticalWheel();
            caps.mouseHorizontalWheel = events_.horizontalWheel();
        }
#if defined(_WIN32)
        caps.keyState = active() && options_.enableKeyboardState;
        if (caps.keyState) caps.keyboardBackend = KeyboardBackend::WindowsConsole;
#else
        caps.keyState = active() && (consoleKeyboard_.available() || parser_.keyboardEnabled());
        caps.keyStatePending = active() && (keyboardPhase_ == 1 || keyboardPhase_ == 2 || keyboardPhase_ == 4);
        if (caps.keyState) caps.keyboardBackend = consoleKeyboard_.available() ? KeyboardBackend::LinuxConsole : KeyboardBackend::Kitty;
#endif
        return caps;
    }
    EventBuffer& events() { return events_; }
    bool clearLine() {
        if (!active()) return fail(InputError::NotInitialized, 0);
        events_.clearLine(); error_ = InputError::NoError; nativeError_ = 0; return true;
    }
    bool clearEvents() {
        if (!active()) return fail(InputError::NotInitialized, 0);
        events_.clearEvents(); error_ = InputError::NoError; nativeError_ = 0; return true;
    }
    bool getLine(std::string& result, Key terminator) {
        if (!active()) return fail(InputError::NotInitialized, 0);
        if (!validKey(terminator)) return fail(InputError::InvalidTerminator, 0);

        if (terminator == Key::Space) return getLine(result, std::string(" "));
        error_ = InputError::NoError; nativeError_ = 0;
        bool overflow = false;
        const bool complete = events_.line(result, terminator, std::string(), &overflow);
        if (overflow) fail(InputError::LineTooLong, 0);
        return complete;
    }
    bool getLine(std::string& result, const std::string& terminator) {
        if (!active()) return fail(InputError::NotInitialized, 0);

        if (terminator.size() == 1) {
            switch (terminator[0]) {
            case '\r': case '\n': return getLine(result, Key::Enter);
            case '\t': return getLine(result, Key::Tab);
            case '\b': case '\x7f': return getLine(result, Key::Backspace);
            case '\x1b': return getLine(result, Key::Escape);
            }
        }
        if (!validTextTerminator(terminator)) return fail(InputError::InvalidTerminator, 0);
        error_ = InputError::NoError; nativeError_ = 0;
        bool overflow = false;
        const bool complete = events_.line(result, Key::Unknown, terminator, &overflow);
        if (overflow) fail(InputError::LineTooLong, 0);
        return complete;
    }
private:
    Manager() : parser_(events_), utf16_(events_) {}
    EventBuffer events_;
    TerminalParser parser_;
    Utf16Decoder utf16_;
    EmergencyRecovery recovery_;
    bool active_ = false, modeSaved_ = false;
    bool mouseEnabled_ = false, mouseModeSaved_ = false;
    bool extraModesSaved_ = false, flushPending_ = false;
    bool keyboardModeSaved_ = false;
    bool activity_ = false;
    InputOptions options_;
    InputError error_ = InputError::NoError;
    unsigned long nativeError_ = 0;
    bool fail(InputError error, unsigned long native) { error_ = error; nativeError_ = native; return false; }
    bool checkInterruption() {
        const unsigned long reason = recovery_.interruption();
        if (!reason) return true;
        if (!shutdown()) return false;
        return fail(InputError::Interrupted, reason);
    }
    bool refreshRecovery() {
        if (!options_.automaticRestore) return true;
        EmergencyRecovery::RestoreData data;
        data.input = input_; data.mode = originalMode_; data.saveInput = modeSaved_;
        data.discardInput = options_.discardPendingInputOnShutdown;
#if defined(__linux__)
        data.output = output_;
        if (mouseModeSaved_) data.append("\x1b[?9;1000;1001;1002;1003;1005;1006;1015;1016l"
                                        "\x1b[?9;1000;1001;1002;1003;1005;1006;1015;1016r");
        if (extraModesSaved_) data.append(options_.enableBracketedPaste ?
            (options_.enableFocusEvents ? "\x1b[?2004;1004r" : "\x1b[?2004r") : "\x1b[?1004r");
        if (keyboardModeSaved_) data.append(keyboardRestoreControl_.c_str());
#endif
        return recovery_.installed() ? recovery_.publish(data) : recovery_.start(data);
    }
    bool readFailure(unsigned long native) {
        if (!shutdown()) return false;
        return fail(InputError::ReadFailed, native);
    }
#if defined(_WIN32)
    HANDLE input_ = INVALID_HANDLE_VALUE;
    HANDLE output_ = INVALID_HANDLE_VALUE;
    DWORD originalMode_ = 0;
    bool processMouse(const MOUSE_EVENT_RECORD& record) {

        HANDLE currentOutput = CreateFileW(L"CONOUT$", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                          nullptr, OPEN_EXISTING, 0, nullptr);
        if (currentOutput == INVALID_HANDLE_VALUE) return false;
        if (output_ != INVALID_HANDLE_VALUE) CloseHandle(output_);
        output_ = currentOutput;
        CONSOLE_SCREEN_BUFFER_INFO info = {};
        if (!GetConsoleScreenBufferInfo(output_, &info)) return false;
        const Vec2 position = {int(record.dwMousePosition.X) - info.srWindow.Left,
                               int(record.dwMousePosition.Y) - info.srWindow.Top};
        InputModifiers modifiers;
        modifiers.shift = (record.dwControlKeyState & SHIFT_PRESSED) != 0;
        modifiers.ctrl = (record.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
        modifiers.alt = (record.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
        const DWORD masks[] = {FROM_LEFT_1ST_BUTTON_PRESSED, RIGHTMOST_BUTTON_PRESSED,
            FROM_LEFT_2ND_BUTTON_PRESSED};

        if (position.x < 0 || position.y < 0 ||
            record.dwMousePosition.X > info.srWindow.Right ||
            record.dwMousePosition.Y > info.srWindow.Bottom) {
            bool flushed = false;
            for (int i = 0; i < int(MouseButton::Count); ++i) {
                const MouseButton button = static_cast<MouseButton>(i);
                if (!events_.mouseDown(button) || (record.dwButtonState & masks[i])) continue;
                if (!flushed) { utf16_.flush(); flushed = true; }
                events_.mouse(InputMouseEvent::Action::Release, button, events_.position(),
                              Vec2{0, 0}, Vec2{0, 0}, modifiers);
            }
            return true;
        }
        utf16_.flush();
        Vec2 delta = events_.locate(position);
        for (int i = 0; i < int(MouseButton::Count); ++i) {
            const MouseButton button = static_cast<MouseButton>(i);
            const bool down = (record.dwButtonState & masks[i]) != 0;
            if (down) events_.observeButton(button);
            if (down == events_.mouseDown(button)) continue;
            events_.mouse(down ? InputMouseEvent::Action::Press : InputMouseEvent::Action::Release,
                          button, position, delta, Vec2{0, 0}, modifiers);
            delta = Vec2{0, 0};
        }
        if (record.dwEventFlags & (MOUSE_WHEELED | MOUSE_HWHEELED)) {
            const int amount = static_cast<SHORT>(HIWORD(record.dwButtonState));
            const Vec2 wheel = {(record.dwEventFlags & MOUSE_HWHEELED) ? amount : 0,
                                (record.dwEventFlags & MOUSE_WHEELED) ? amount : 0};
            events_.mouse(InputMouseEvent::Action::Scroll, MouseButton::Unknown,
                          position, delta, wheel, modifiers);
        } else if ((record.dwEventFlags & MOUSE_MOVED) && options_.mouseTracking != MouseTracking::Buttons &&
                   (options_.mouseTracking == MouseTracking::AllMotion ||
                    (record.dwButtonState & (FROM_LEFT_1ST_BUTTON_PRESSED | RIGHTMOST_BUTTON_PRESSED |
                                            FROM_LEFT_2ND_BUTTON_PRESSED)))) {
            events_.mouse(InputMouseEvent::Action::Move, MouseButton::Unknown,
                          position, delta, Vec2{0, 0}, modifiers);
        }
        return true;
    }
    static Key virtualKey(WORD code) {
        if (code >= 'A' && code <= 'Z') return static_cast<Key>(int(Key::A) + code - 'A');
        if (code >= '0' && code <= '9') return static_cast<Key>(int(Key::Num0) + code - '0');
        if (code >= VK_NUMPAD0 && code <= VK_NUMPAD9) return static_cast<Key>(int(Key::Num0) + code - VK_NUMPAD0);
        if (code >= VK_F1 && code <= VK_F12) return static_cast<Key>(int(Key::F1) + code - VK_F1);
        switch (code) {
        case VK_SPACE: return Key::Space; case VK_RETURN: return Key::Enter;
        case VK_ESCAPE: return Key::Escape; case VK_TAB: return Key::Tab;
        case VK_UP: return Key::ArrowUp; case VK_DOWN: return Key::ArrowDown;
        case VK_LEFT: return Key::ArrowLeft; case VK_RIGHT: return Key::ArrowRight;
        case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: return Key::Shift;
        case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: return Key::Ctrl;
        case VK_MENU: case VK_LMENU: case VK_RMENU: return Key::Alt;
        case VK_BACK: return Key::Backspace; case VK_DELETE: return Key::Delete;
        case VK_HOME: return Key::Home; case VK_END: return Key::End;
        case VK_PRIOR: return Key::PageUp; case VK_NEXT: return Key::PageDown;
        case VK_INSERT: return Key::Insert;
        default: return Key::Unknown;
        }
    }
    void processKey(const KEY_EVENT_RECORD& record) {
        InputModifiers modifiers;
        modifiers.shift = (record.dwControlKeyState & SHIFT_PRESSED) != 0;
        modifiers.ctrl = (record.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
        modifiers.alt = (record.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
        const Key key = virtualKey(record.wVirtualKeyCode);
        if (key != Key::Shift && key != Key::Ctrl && key != Key::Alt)
            events_.setKey(key, record.bKeyDown != FALSE);
        events_.setKey(Key::Shift, modifiers.shift);
        events_.setKey(Key::Ctrl, modifiers.ctrl);
        events_.setKey(Key::Alt, modifiers.alt);
        const std::uint16_t unit = static_cast<std::uint16_t>(record.uChar.UnicodeChar);

        const bool composedAlt = !record.bKeyDown && key == Key::Alt && unit != 0;
        if (!record.bKeyDown && !composedAlt) return;
        const unsigned repeat = record.wRepeatCount ? record.wRepeatCount : 1;

        if (unit >= 32 && unit != 127) { utf16_.feed(unit, repeat, modifiers); return; }

        if (unit != 0 && (key == Key::Unknown || composedAlt)) {
            Key control = Key::Unknown;
            if (unit == '\r' || unit == '\n') control = Key::Enter;
            else if (unit == '\t') control = Key::Tab;
            else if (unit == 8 || unit == 127) control = Key::Backspace;
            else if (unit == 27) control = Key::Escape;
            else if (unit >= 1 && unit <= 26) {
                control = static_cast<Key>(int(Key::A) + unit - 1);
                modifiers.ctrl = true;
            }
            utf16_.flush();
            for (unsigned i = 0; i < repeat; ++i) {
                if (control != Key::Unknown) events_.key(control, modifiers, false);
                else events_.unknown(std::string(1, static_cast<char>(unit)));
            }
            return;
        }
        if (key == Key::Shift || key == Key::Ctrl || key == Key::Alt) return;

        utf16_.flush();
        for (unsigned i = 0; i < repeat; ++i) events_.key(key, modifiers, false);
    }
#else
    ConsoleKeyboard consoleKeyboard_;
    int keyboardPhase_ = 0;
    TerminalParser::Time keyboardDeadline_;
    const char* keyboardCommand_ = nullptr;
    std::size_t keyboardCommandOffset_ = 0;
    std::string keyboardRestoreControl_;
    int input_ = -1;
    int output_ = -1;
    termios originalMode_ = {};
    int keyboardWaitTime(int requested) const {
        if (keyboardPhase_ != 1 && keyboardPhase_ != 2 && keyboardPhase_ != 4) return requested;
        const auto remaining = keyboardDeadline_ - TerminalParser::Clock::now();
        const int timeout = remaining <= TerminalParser::Clock::duration::zero() ? 0 :
            static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count()) + 1;
        return requested < 0 || timeout < requested ? timeout : requested;
    }
    bool advanceKeyboard(TerminalParser::Time now) {
        if (keyboardCommand_) {
            const std::size_t size = std::char_traits<char>::length(keyboardCommand_);
            while (keyboardCommandOffset_ < size) {
                const ssize_t count = write(output_, keyboardCommand_ + keyboardCommandOffset_, size - keyboardCommandOffset_);
                if (count < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
                    if (now < keyboardDeadline_) return true;
                    errno = ETIMEDOUT;
                }
                if (count <= 0) {
                    const unsigned long saved = count == 0 ? static_cast<unsigned long>(EIO) : static_cast<unsigned long>(errno);
                    const InputError error = keyboardPhase_ == 4 ? InputError::RestoreFailed : InputError::ConfigureFailed;
                    if (!shutdown()) return false;
                    return fail(error, saved);
                }
                keyboardCommandOffset_ += static_cast<std::size_t>(count);
            }
            keyboardCommand_ = nullptr; keyboardCommandOffset_ = 0;
            if (keyboardPhase_ == 4) { keyboardModeSaved_ = false; keyboardPhase_ = 0; }
            else keyboardDeadline_ = now + std::chrono::milliseconds(options_.keyboardProtocolTimeoutMs);
        }
        unsigned flags = 0;
        const bool reply = parser_.takeKeyboardReply(flags);
        const bool attributes = parser_.takeAttributesReply();
        if (keyboardPhase_ == 1 && reply) {

            keyboardRestoreControl_ = "\x1b[=" + std::to_string(flags) + "u";
            keyboardModeSaved_ = true;
            if (!refreshRecovery()) {
                const unsigned long saved = recovery_.error();
                if (!shutdown()) return false;
                return fail(InputError::ConfigureFailed, saved);
            }
            parser_.expectKeyboardReply(true); keyboardPhase_ = 2;
            keyboardDeadline_ = now + std::chrono::milliseconds(options_.keyboardProtocolTimeoutMs);
            keyboardCommand_ = "\x1b[=31u\x1b[?u";
            return advanceKeyboard(now);
        } else if (keyboardPhase_ == 2 && reply && (flags & 26u) == 26u) {
            keyboardPhase_ = 3;
        } else if ((keyboardPhase_ == 1 && attributes) ||
                   (keyboardPhase_ == 2 && reply) ||
                   ((keyboardPhase_ == 1 || keyboardPhase_ == 2) && now >= keyboardDeadline_)) {
            parser_.disableKeyboard(); keyboardPhase_ = 0;
            if (keyboardModeSaved_) {
                keyboardPhase_ = 4; keyboardCommand_ = keyboardRestoreControl_.c_str();
                keyboardDeadline_ = now + std::chrono::milliseconds(options_.keyboardProtocolTimeoutMs);
                return advanceKeyboard(now);
            }
        }
        return true;
    }
    bool writeControl(const char* bytes) {
        const std::size_t size = std::char_traits<char>::length(bytes);
        const auto deadline = TerminalParser::Clock::now() + std::chrono::milliseconds(100);
        std::size_t offset = 0;
        unsigned interruptions = 0;
        while (offset < size) {
            const ssize_t count = write(output_, bytes + offset, size - offset);
            if (count < 0) {
                if (errno == EINTR && ++interruptions < 16) continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    for (;;) {
                        const auto remaining = deadline - TerminalParser::Clock::now();
                        if (remaining <= TerminalParser::Clock::duration::zero()) {
                            errno = ETIMEDOUT; return false;
                        }
                        const int timeout = static_cast<int>(
                            std::chrono::duration_cast<std::chrono::milliseconds>(remaining).count()) + 1;
                        pollfd descriptor = {output_, POLLOUT, 0};
                        const int ready = poll(&descriptor, 1, timeout);
                        if (ready < 0 && errno == EINTR) continue;
                        if (ready < 0) return false;
                        if (!ready) { errno = ETIMEDOUT; return false; }
                        if (descriptor.revents & (POLLERR | POLLNVAL | POLLHUP)) {
                            errno = EIO; return false;
                        }
                        if (descriptor.revents & POLLOUT) break;
                    }
                    continue;
                }
                return false;
            }
            if (!count) { errno = EIO; return false; }
            offset += static_cast<std::size_t>(count);
        }
        return true;
    }
#endif
};
}

class Input {
public:
    explicit Input(const InputOptions& options = InputOptions()) { Initialize(options); }
    ~Input() {
        Shutdown();
        if (owner() == this) owner() = nullptr;
    }
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;
    Input(Input&&) = delete;
    Input& operator=(Input&&) = delete;

    bool Initialize(const InputOptions& options = InputOptions()) {
        if (owner() && owner() != this) return fail(InputError::AlreadyInUse);
        input_detail::Manager& backend = manager();
        owner() = this;
        return remember(backend.initialize(options));
    }
    bool Shutdown() { return ownsSession() ? remember(manager().shutdown()) : true; }
    bool IsInitialized() const { return ownsSession() && manager().active(); }

    bool Update() {
        return ownsSession() ? remember(manager().update()) : fail(InputError::NotInitialized);
    }

    bool Wait(int timeoutMs = -1) {
        return ownsSession() ? remember(manager().wait(timeoutMs)) : fail(InputError::NotInitialized);
    }
    bool ClearLine() {
        return ownsSession() ? remember(manager().clearLine()) : fail(InputError::NotInitialized);
    }
    bool ClearEvents() {
        return ownsSession() ? remember(manager().clearEvents()) : fail(InputError::NotInitialized);
    }
    InputError GetLastError() const { return error_; }
    unsigned long GetNativeError() const { return nativeError_; }
    InputCapabilities GetCapabilities() const {
        return ownsSession() ? manager().capabilities() : InputCapabilities();
    }

    bool GetEvent(InputEvent& event) { return IsInitialized() && manager().events().pop(event); }
    bool GetChar(std::string& text) {
        InputEvent event;
        if (!IsInitialized() || !manager().events().popType(InputEvent::Type::Text, event)) return false;
        text = std::move(event.text); return true;
    }
    std::string GetChar() {
        std::string text; GetChar(text); return text;
    }
    bool GetKey(Key& key) {
        InputEvent event;
        if (!IsInitialized() || !manager().events().popType(InputEvent::Type::Key, event)) return false;
        key = event.key; return true;
    }
    Key GetKey() {
        Key key = Key::Unknown; GetKey(key); return key;
    }

    bool GetLine(std::string& line, Key terminator = Key::Enter) {
        return ownsSession() ? remember(manager().getLine(line, terminator)) : fail(InputError::NotInitialized);
    }
    bool GetLine(std::string& line, const std::string& terminator) {
        return ownsSession() ? remember(manager().getLine(line, terminator)) : fail(InputError::NotInitialized);
    }
    bool GetLine(std::string& line, const char* terminator) {
        return GetLine(line, terminator ? std::string(terminator) : std::string());
    }
    bool GetLine(std::string& line, char terminator) {
        return GetLine(line, std::string(1, terminator));
    }
    bool GetMouseEvent(InputEvent& event) {
        return IsInitialized() && manager().events().popType(InputEvent::Type::Mouse, event);
    }
    bool HasMousePosition() const { return GetCapabilities().mouse && manager().events().positionKnown(); }
    Vec2 GetMousePosition() const { return IsInitialized() ? manager().events().position() : Vec2{0, 0}; }
    Vec2 GetMouseDelta() const { return IsInitialized() ? manager().events().delta() : Vec2{0, 0}; }
    Vec2 GetMouseWheel() const { return IsInitialized() ? manager().events().wheel() : Vec2{0, 0}; }
    bool IsMouseButtonPressed(MouseButton b) const {
        return GetCapabilities().mouse && manager().events().mousePressed(b);
    }

    bool IsMouseButtonDown(MouseButton b) const {
        return GetCapabilities().mouse && manager().events().mouseDown(b);
    }
    bool IsMouseButtonUp(MouseButton b) const {
        return GetCapabilities().mouse && input_detail::validButton(b) &&
            (manager().events().buttons() & (1u << static_cast<unsigned>(b))) != 0 &&
            !manager().events().mouseDown(b);
    }
    bool IsMouseButtonReleased(MouseButton b) const {
        return GetCapabilities().mouse && manager().events().mouseReleased(b);
    }
    bool IsMouseButtonHeld(MouseButton b) const {
        return GetCapabilities().mouse && manager().events().mouseHeld(b);
    }
    std::size_t PendingEvents() const { return IsInitialized() ? manager().events().pending() : 0; }
    std::size_t DroppedEvents() const { return IsInitialized() ? manager().events().dropped() : 0; }
    bool IsKeyPressed(Key key) const { return GetCapabilities().keyState && manager().events().pressed(key); }

    bool IsKeyDown(Key key) const { return GetCapabilities().keyState && manager().events().down(key); }
    bool IsKeyUp(Key key) const {
        return GetCapabilities().keyState && input_detail::validKey(key) && !manager().events().down(key);
    }
    bool IsKeyReleased(Key key) const { return GetCapabilities().keyState && manager().events().released(key); }
    bool IsKeyHeld(Key key) const { return GetCapabilities().keyState && manager().events().held(key); }
private:
    static input_detail::Manager& manager() { return input_detail::Manager::instance(); }
    static Input*& owner() { static Input* current = nullptr; return current; }
    bool ownsSession() const { return owner() == this; }
    bool remember(bool result) {
        error_ = manager().error(); nativeError_ = manager().nativeError();
        if (!manager().active() && !manager().needsRestore()) owner() = nullptr;
        return result;
    }
    bool fail(InputError error) { error_ = error; nativeError_ = 0; return false; }
    InputError error_ = InputError::NoError;
    unsigned long nativeError_ = 0;
};
}
#endif
