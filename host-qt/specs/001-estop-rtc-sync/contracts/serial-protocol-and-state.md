# Contracts: STM32 Serial Protocol & HMI State Module

**Feature**: `001-estop-rtc-sync`  
**Date**: 2026-11-22

## 1. Wire Protocol Contract (STM32 USART1 ASCII, 115200 8N1)

All frames are UTF-8/ASCII strings terminated by `\n` (inbound frames also tolerate `\r\n`).

### Outbound Commands (HMI -> STM32)

| Command | Format | Trigger | Guard Conditions |
| --- | --- | --- | --- |
| LED Control | `$CMD:LED:<0\|1>\n` | Connect (`1`), Disconnect/Exit (`0`) | Port open |
| Servo Angle | `$CMD:SRV:<0..180>\n` | Manual Slider/SpinBox, Sweep tick, Startup init | `connected && !isLocked()` (MUST NOT emit when locked) |
| System Unlock | `$CMD:SYS:UNLOCK\n` | Operator clicks Unlock | `connected && isLocked()` |
| Sync RTC (GMT+7) | `$CMD:RTC:HH:MM:SS\n` | Connect init (120ms delay) or "Sync PC Time (GMT+7)" click | `connected` (allowed even when locked); `HH:MM:SS` is always UTC+07:00 |

### Inbound Messages (STM32 -> HMI)

| Message Pattern | Category | Meaning | Required HMI Action |
| --- | --- | --- | --- |
| `$ALM:<HHMMSS>:BTN:PRESSED` | Alarm | Physical E-Stop (PA0) triggered | Enter `Locked`, stop sweep, sync angle `0`, show banner, disable servo controls, log Alarm |
| `$ALM:<HHMMSS>:SYS:LOCKED` | Alarm | Controller safety lock active / command rejected | Enter `Locked`, stop sweep, sync angle `0`, show banner, disable servo controls, log Warn/Alarm |
| `$ALM:<HHMMSS>:SYS:BOOT` | Alarm | Controller hardware reset | Exit `Locked`, stop sweep, sync angle `0`, hide banner, set tag `BOOT`, log Warn |
| `$TLM:<HHMMSS>:SYS:UNLOCKED` | Telemetry | Controller confirmed unlock | Exit `Locked`, hide banner, re-enable servo controls (if connected), stop unlock timer, log Info |
| `$TLM:<HHMMSS>:RTC:OK` | Telemetry | DS1307 RTC update succeeded | Set RTC status `Synced`, stop RTC timer, log Info |
| `$ALM:<HHMMSS>:RTC:ERR` | Alarm | DS1307 I2C RTC error | Set RTC status `Error`, stop RTC timer, log Warn/Alarm |
| `$TLM:<HHMMSS>:SRV:<0..180>` | Telemetry | Current servo position telemetry | Update Current Angle label (and SliderPage if sweep not running) |

## 2. Internal C++ Module Contract (`ServoProtocol` library, `Qt::Core` only)

```cpp
// Header: servo_protocol.h (zero QtWidgets / QtSerialPort includes)
namespace ServoProtocol {

enum class FrameKind { None, Telemetry, Alarm };

enum class MessageType {
    Invalid,
    AlarmButtonPressed,
    AlarmSystemLocked,
    AlarmSystemBoot,
    AlarmRtcError,
    TelemetrySystemUnlocked,
    TelemetryRtcOk,
    TelemetryServoAngle
};

struct ProtocolMessage {
    MessageType type{MessageType::Invalid};
    FrameKind frameKind{FrameKind::None};
    QString timestamp;
    int servoAngle{-1};
    QString rawLine;
    bool isValid() const { return type != MessageType::Invalid; }
};

class SerialLineFramer {
public:
    static constexpr int kMaxLineLength = 512;
    QStringList appendBytes(const QByteArray &chunk);
    void reset();
};

ProtocolMessage parseProtocolLine(const QString &rawLine);
QString formatGmt7RtcCommand(const QDateTime &utcNow = QDateTime::currentDateTimeUtc());
QString formatUnlockCommand();
QString formatServoCommand(int angle);

} // namespace ServoProtocol
```

Plus a widget-free `HmiSessionState` class/struct inside `servo_protocol.h` exposing `applyMessage(const ProtocolMessage &)`, `onConnected()`, `onDisconnected()`, `onUnlockRequested()`, `onUnlockTimeout()`, `onRtcSyncRequested()`, `onRtcTimeout()`, and read-only state accessors (`isLocked()`, `canSendServoCommand()`, `shouldSendStartupServo()`, `canUnlock()`, `canSyncRtc()`, `unlockRequestState()`, `rtcSyncState()`).

## 3. UI Component Contracts

- **`MainWindow`**:
  - Persistent Top Bar: preserves Port combo (with `/dev/scada_rs232`), Refresh, Connect, `"Sync PC Time (GMT+7)"` button (`m_btnSyncRtc`), inline RTC status indicator (`m_lblRtcStatus`), connection status, and current angle label.
  - Safety Locked Banner (`QFrame`/`QWidget` above `QStackedWidget`, hidden unless `isLocked()`): displays prominent red alert message, inline unlock status (`Waiting for controller unlock...` / `No unlock response yet — click Unlock to retry`), and `Unlock` button (`enabled = canUnlock()`).
  - Non-blocking serial reception strictly via `QSerialPort::readyRead` -> `SerialLineFramer::appendBytes` -> `parseProtocolLine` -> `HmiSessionState::applyMessage` -> UI sync.
- **`SliderPage`**:
  - Adds `setMotionEnabled(bool enabled)` (or `setLocked(bool locked)`) disabling slider, spinbox, and preset buttons while still permitting `syncAngleFromHardware(0)` to update the display without emitting `angleChanged`.
- **`SweepPage`**:
  - Adds `setLocked(bool locked)` stopping any active sweep and disabling `Start/Stop` and sweep parameter inputs while locked.
