# Quickstart & Validation Guide: E-Stop Safety Handling & RTC Time Synchronization

**Feature**: `001-estop-rtc-sync`  
**Date**: 2026-11-22

## Prerequisites

- Fedora Linux with CMake >= 3.16, Ninja, C++17 compiler, and Qt6 (`Widgets`, `SerialPort`, `Test`).
- Working directory: repository root `/home/nguyenhuudu2007/QT_Creator/01_servo_hmi`.

## 1. Configure & Build Warning-Clean (`build-ninja/`)

```bash
cmake -S . -B build-ninja -G Ninja
cmake --build build-ninja
```

**Expected outcome**: Zero compiler warnings (`-Wall -Wextra`) and successful linking of `ServoProtocol`, `ServoHMI`, and `test_servo_protocol`.

## 2. Run Automated Headless Unit Tests

```bash
ctest --test-dir build-ninja --output-on-failure
```

**Expected outcome**: `ServoProtocolTests` passes 100% headlessly (no physical STM32 or display server required), validating:
- All `$TLM` and `$ALM` valid frame variants (`BTN:PRESSED`, `SYS:LOCKED`, `SYS:UNLOCKED`, `SYS:BOOT`, `RTC:OK`, `RTC:ERR`, `SRV:0..180`).
- Strict rejection of wrong prefixes, bad timestamps, out-of-range/non-numeric servo angles, and unrecognized lines.
- `SerialLineFramer` multi-chunk reassembly, `\r\n` normalization, empty line skipping, 512-byte overflow discard/recovery, and `reset()`.
- Fixed GMT+7 (`UTC+07:00`) `$CMD:RTC:HH:MM:SS` formatting independent of host timezone.
- `HmiSessionState` safety transitions including reconnect-while-locked startup `$CMD:SRV` suppression, `SYS:BOOT` recovery, and 2s timeout states.

## 3. Manual End-to-End HMI Validation (Virtual Serial Pair or Hardware)

Optional virtual PTY simulation using `socat`:

```bash
socat -d -d pty,raw,echo=0,link=/tmp/ttyQt pty,raw,echo=0,link=/tmp/ttyStm
```

1. Launch `./build-ninja/ServoHMI`, enter `/tmp/ttyQt` (or select `/dev/scada_rs232` on hardware), and click **Connect**.
2. Verify `$CMD:LED:1` and `$CMD:RTC:HH:MM:SS` (GMT+7) are sent, and **Sync PC Time (GMT+7)** shows `Pending` -> `Synced` upon `$TLM:120000:RTC:OK` or `Error` upon `$ALM:120000:RTC:ERR`.
3. Send `$ALM:120005:BTN:PRESSED` (or `$ALM:120005:SYS:LOCKED`) from `/tmp/ttyStm`. Verify:
   - Any active Auto Sweep stops immediately.
   - Locked-state banner becomes visible and angle resets to `0 deg [E-STOP]` / `[LOCKED]`.
   - Manual Slider controls and Auto Sweep controls are disabled; zero `$CMD:SRV` commands are emitted.
4. Click **Disconnect** then **Connect** again while locked: verify banner stays visible, controls stay disabled, and no automatic startup `$CMD:SRV` is transmitted.
5. Click **Unlock** on the banner: verify `$CMD:SYS:UNLOCK` is transmitted and `"Waiting for controller unlock..."` appears (or retry notice after 2s if unanswered).
6. Send `$TLM:120010:SYS:UNLOCKED` from `/tmp/ttyStm`: verify banner hides and servo controls re-enable.
