# Data Model: E-Stop Safety Handling & RTC Time Synchronization

**Feature**: `001-estop-rtc-sync`  
**Date**: 2026-11-22

## 1. Parsed Protocol Message (`ProtocolMessage`)

Represents a single validated or rejected incoming ASCII line from the STM32 USART1 link.

| Field | Type | Description | Validation / Constraints |
| --- | --- | --- | --- |
| `type` | `MessageType` (enum class) | Classified protocol frame kind | One of `Invalid`, `AlarmButtonPressed`, `AlarmSystemLocked`, `AlarmSystemBoot`, `AlarmRtcError`, `TelemetrySystemUnlocked`, `TelemetryRtcOk`, `TelemetryServoAngle` |
| `frameKind` | `FrameKind` (enum class) | Frame header prefix | `None`, `Telemetry` (`$TLM`), `Alarm` (`$ALM`) |
| `timestamp` | `QString` | 6-digit controller timestamp (`HHMMSS`) | Present and strictly 6 ASCII digits (`[0-9]{6}`) for all valid messages; empty on `Invalid` |
| `servoAngle` | `int` | Parsed servo angle payload | Valid range `0..180` when `type == TelemetryServoAngle`; `-1` otherwise |
| `rawLine` | `QString` | Original trimmed incoming line | Always preserved for RX logging and diagnostics |

### Message Classification Rules

| Raw Line Pattern | `frameKind` | `type` | Additional Payload |
| --- | --- | --- | --- |
| `$ALM:<HHMMSS>:BTN:PRESSED` | `Alarm` | `AlarmButtonPressed` | None |
| `$ALM:<HHMMSS>:SYS:LOCKED` | `Alarm` | `AlarmSystemLocked` | None |
| `$ALM:<HHMMSS>:SYS:BOOT` | `Alarm` | `AlarmSystemBoot` | None |
| `$ALM:<HHMMSS>:RTC:ERR` | `Alarm` | `AlarmRtcError` | None |
| `$TLM:<HHMMSS>:SYS:UNLOCKED` | `Telemetry` | `TelemetrySystemUnlocked` | None |
| `$TLM:<HHMMSS>:RTC:OK` | `Telemetry` | `TelemetryRtcOk` | None |
| `$TLM:<HHMMSS>:SRV:<angle>` | `Telemetry` | `TelemetryServoAngle` | Integer `angle` in `0..180` |
| Any other line | `None` | `Invalid` | `rawLine` only |

## 2. Serial Line Framer (`SerialLineFramer`)

Stateful byte-to-line accumulator for asynchronous serial reads.

| Field / Config | Type / Value | Description |
| --- | --- | --- |
| `kMaxLineLength` | `512` bytes | Maximum allowed length of an unterminated line before overflow discard |
| `m_buffer` | `QByteArray` | Accumulated bytes awaiting `\n` |
| `m_discardUntilNewline` | `bool` | Set when an unterminated chunk exceeds `kMaxLineLength`; cleared after next `\n` or `reset()` |

## 3. HMI Safety & RTC State (`HmiSessionState`)

Widget-free state model governing UI enablement, safety lockouts, startup commands, and inline status indicators.

### Enumerations

- `SafetyLockState`: `Unlocked`, `Locked`
- `UnlockRequestState`: `Idle`, `Pending`, `TimedOut`
- `RtcSyncState`: `Idle`, `Pending`, `Synced`, `Error`, `TimedOut`

### Attributes

| Field | Type | Initial Value | Description |
| --- | --- | --- | --- |
| `connected` | `bool` | `false` | Whether the serial port is currently open |
| `lockState` | `SafetyLockState` | `Unlocked` | Safety lockout status (`Locked` disables servo controls and blocks `$CMD:SRV`) |
| `lockReason` | `QString` | `""` | Human-readable trigger (`BTN:PRESSED` / `SYS:LOCKED`) |
| `unlockRequestState` | `UnlockRequestState` | `Idle` | Tracks inline feedback after sending `$CMD:SYS:UNLOCK` |
| `rtcSyncState` | `RtcSyncState` | `Idle` | Tracks inline feedback for GMT+7 RTC sync |
| `rtcLastTimestamp` | `QString` | `""` | Timestamp of last synced or attempted RTC update |
| `currentAngle` | `int` | `90` | Last known hardware or UI target angle (`0..180`) |

### Derived Permissions & Queries

- `isLocked() == (lockState == SafetyLockState::Locked)`
- `canSendServoCommand() == (connected && lockState == SafetyLockState::Unlocked)`
- `shouldSendStartupServo() == (connected && lockState == SafetyLockState::Unlocked)`
- `canUnlock() == (connected && lockState == SafetyLockState::Locked)`
- `canSyncRtc() == connected`

### State Transition Summary

| Event / Input | `lockState` | `unlockRequestState` | `rtcSyncState` | Side Effects |
| --- | --- | --- | --- | --- |
| `onConnected()` | *Preserved* | `Idle` (if Unlocked) / keep `Idle` (if Locked) | `Idle` | Suppress startup `$CMD:SRV` if `Locked` |
| `onDisconnected()` | *Preserved* | `Idle` | `Idle` | Stop active sweep & timers; Unlock disabled offline |
| `AlarmButtonPressed` / `AlarmSystemLocked` | `Locked` | `Idle` (or unchanged if already waiting? Reset to `Idle` if fresh E-Stop button press) | Unchanged | Stop sweep, angle -> `0`, show banner, disable servo controls |
| `onUnlockRequested()` (when `canUnlock()`) | `Locked` | `Pending` | Unchanged | Send `$CMD:SYS:UNLOCK\n`, start 2s unlock timeout timer |
| `onUnlockTimeout()` (when `Locked` && `Pending`) | `Locked` | `TimedOut` | Unchanged | Show retry notice in banner; Unlock stays clickable |
| `TelemetrySystemUnlocked` | `Unlocked` | `Idle` | Unchanged | Hide banner, re-enable servo controls |
| `AlarmSystemBoot` | `Unlocked` | `Idle` | Unchanged | Angle -> `0`, hide banner, re-enable controls if connected |
| `onRtcSyncRequested()` (when `connected`) | Unchanged | Unchanged | `Pending` | Send `$CMD:RTC:HH:MM:SS\n` (GMT+7), start 2s RTC timeout timer |
| `TelemetryRtcOk` | Unchanged | Unchanged | `Synced` | Stop RTC timeout timer, display inline success |
| `AlarmRtcError` | Unchanged | Unchanged | `Error` | Stop RTC timeout timer, display inline error |
| `onRtcTimeout()` (when `Pending`) | Unchanged | Unchanged | `TimedOut` | Display inline timeout/retry notice |
| `TelemetryServoAngle(angle)` | Unchanged | Unchanged | Unchanged | Update displayed angle to `angle` |
| `Invalid` | Unchanged | Unchanged | Unchanged | Log raw line only; zero state mutations |
