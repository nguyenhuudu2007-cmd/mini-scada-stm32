# Implementation Plan: E-Stop Safety Handling & RTC Time Synchronization

**Branch**: `001-estop-rtc-sync` | **Date**: 2026-11-22 | **Spec**: [spec.md](file:///home/nguyenhuudu2007/QT_Creator/01_servo_hmi/specs/001-estop-rtc-sync/spec.md)  
**Input**: Feature specification from `/specs/001-estop-rtc-sync/spec.md`

## Summary

Deliver complete E-Stop safety lockout/recovery handling and one-click GMT+7 RTC synchronization for ServoHMI while isolating serial line framing, strict `$TLM`/`$ALM` frame parsing, GMT+7 command formatting, and safety/RTC state logic into a widget-free C++17/QtCore library (`ServoProtocol`). `MainWindow`, `SliderPage`, and `SweepPage` consume `ServoProtocol` via `QSerialPort` asynchronous `readyRead` signals/slots, enforce UI lockouts with a prominent non-modal banner and Unlock workflow, show inline RTC sync status (`Pending`, `Synced`, `Error`, `TimedOut`), and run headless CTest/QtTest unit tests under a warning-clean CMake + Ninja build in `build-ninja/`.

## Technical Context

**Language/Version**: C++17 (`CMAKE_CXX_STANDARD 17`, `CMAKE_CXX_STANDARD_REQUIRED ON`)  
**Primary Dependencies**: Qt6 (`Widgets`, `SerialPort`, `Core`, `Test`; preserving Qt6/Qt5 `CMakeLists.txt` discovery)  
**Storage**: Daily persistent audit log file (`logs/scada_YYYY-MM-DD.log` via existing `LogPage`)  
**Testing**: QtTest (`QTEST_GUILESS_MAIN`) registered with CTest (`ctest --test-dir build-ninja --output-on-failure`)  
**Target Platform**: Fedora Linux desktop HMI (GCC 16.2.1, CMake 4.3.0, Ninja 1.13.2)  
**Project Type**: Desktop HMI application + decoupled core protocol library + headless unit test suite  
**Performance Goals**: <100 ms UI state refresh on incoming `$ALM`/`$TLM` frames; zero GUI thread blocking; preserve 40 ms slider throttling and >=45 ms sweep interval  
**Constraints**: Never call `waitForReadyRead()` on GUI thread; preserve `$CMD:LED`, `$CMD:SRV`, `$CMD:SYS:UNLOCK`, `$CMD:RTC`, `$TLM`, `$ALM`, and `/dev/scada_rs232`; warning-clean `-Wall -Wextra` Ninja build  
**Scale/Scope**: Single desktop window with 4 mode pages, 7 valid `$TLM`/`$ALM` message types, 512-byte max line framer, and comprehensive headless unit test suite

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

- **I. Non-Blocking Event-Driven Serial I/O (NON-NEGOTIABLE)**: PASS — Incoming serial bytes continue to flow strictly through `QSerialPort::readyRead` and `errorOccurred` signals/slots; timeout feedback for Unlock and RTC uses non-blocking single-shot `QTimer`s (2000 ms). No `waitForReadyRead()` calls exist or will be introduced.
- **II. STM32 Protocol & Device Backward Compatibility**: PASS — Preserves `$CMD:LED`, `$CMD:SRV`, `$CMD:SYS:UNLOCK`, `$CMD:RTC:HH:MM:SS`, `$TLM`, `$ALM`, 115200 8N1 settings, and `/dev/scada_rs232` udev symlink discovery.
- **III. Separation of Protocol & State Logic from Qt Widgets**: PASS — `ServoProtocol` is built as a separate static library target linking only `Qt::Core` (zero `QtWidgets` or `QSerialPort` includes), holding `SerialLineFramer`, `parseProtocolLine`, `formatGmt7RtcCommand`, and `HmiSessionState`.
- **IV. Automated Unit Testing with CTest & QtTest**: PASS — `test_servo_protocol` uses `Qt::Test` (`QTEST_GUILESS_MAIN`) and is registered via `add_test()` in `CMakeLists.txt` for headless `ctest` execution.
- **V. Warning-Clean CMake + Ninja Builds**: PASS — `-Wall -Wextra` enabled on all targets in `CMakeLists.txt` and verified via `cmake --build build-ninja`.

*Post-Design Re-check*: All Phase 1 design artifacts (`data-model.md`, `contracts/serial-protocol-and-state.md`, `quickstart.md`) satisfy Principles I–V with zero violations or exceptions required.

## Project Structure

### Documentation (this feature)

```text
specs/001-estop-rtc-sync/
├── plan.md              # This file (/speckit-plan command output)
├── research.md          # Phase 0 output (/speckit-plan command)
├── data-model.md        # Phase 1 output (/speckit-plan command)
├── quickstart.md        # Phase 1 output (/speckit-plan command)
├── contracts/           # Phase 1 output (/speckit-plan command)
│   └── serial-protocol-and-state.md
├── checklists/
│   └── requirements.md
└── spec.md
```

### Source Code (repository root)

```text
CMakeLists.txt               # Adds ServoProtocol static lib, -Wall -Wextra, CTest + test_servo_protocol
main.cpp                     # Application entry point (unchanged)
servo_protocol.h             # Non-GUI protocol framer, strict parser, GMT+7 formatter, HmiSessionState
servo_protocol.cpp           # Implementation linking only Qt::Core
main_window.h                # Adds safety banner, Unlock button, inline RTC label, 2s timers, state wiring
main_window.cpp              # Integrates SerialLineFramer, parseProtocolLine, HmiSessionState, and UI lockouts
slider_page.h                # Adds setLocked(bool) / motion control enablement API
slider_page.cpp              # Disables slider, spinbox, and presets when locked while preserving hardware sync
sweep_page.h                 # Adds setLocked(bool) API
sweep_page.cpp               # Stops sweep and disables Start/Stop & sweep params when locked
log_page.h / log_page.cpp    # Existing daily audit logger (unchanged)
tests/
└── test_servo_protocol.cpp  # Headless QtTest suite for framer, parser, GMT+7 formatting, and state transitions
```

**Structure Decision**: Retain the existing flat repository layout for application sources while introducing a dedicated `ServoProtocol` CMake static library (`servo_protocol.h` / `servo_protocol.cpp`) and a `tests/` directory (`tests/test_servo_protocol.cpp`). Keeping `ServoProtocol` linked only to `Qt::Core` provides compile-time enforcement of widget separation with minimal disruption to the existing Qt Creator project layout.

## Complexity Tracking

> No Constitution Check violations; table intentionally empty.
