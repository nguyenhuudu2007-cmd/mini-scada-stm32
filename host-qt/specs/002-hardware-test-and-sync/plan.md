# Implementation Plan: Hardware Test Mode, Serial Disconnect Recovery & STM32 Protocol Alignment

**Spec**: [spec.md](file:///home/nguyenhuudu2007/QT_Creator/01_servo_hmi/specs/002-hardware-test-and-sync/spec.md)

## Technical Context
- **Qt HMI**: C++17, Qt6 Widgets / SerialPort / Core / Test, CMake + Ninja (`build-ninja/`), `-Wall -Wextra`, CTest (`test_servo_protocol`).
- **STM32 Firmware**: STM32F103C8Tx HAL (`stm32_SCADA_w1/Core/Src/main.c`), built with `arm-none-eabi-gcc` + `make` in `stm32_SCADA_w1/Debug/`.

## Constitution Check
- **I. Non-Blocking Event-Driven Serial I/O**: PASS — remove blocking `waitForBytesWritten()` calls in disconnect/destructor paths; use `QTimer` for self-test sequence; handle `QSerialPort::errorOccurred` cleanly.
- **II. STM32 Protocol & Device Backward Compatibility**: PASS — coordinate `$ALM:...:BTN:PRESSED`, `$ALM:...:SYS:BOOT`, persistent lock until `$CMD:SYS:UNLOCK`, and `$TLM:...:LED:<0|1>` across both firmware and HMI.
- **III. Separation of Protocol & State Logic from Qt Widgets**: PASS — `TelemetryLedState`, `formatLedCommand()`, and `isServoMotionCommand()` remain in `ServoProtocol` (`Qt::Core` only).
- **IV. Automated Unit Testing with CTest & QtTest**: PASS — add TDD tests in `tests/test_servo_protocol.cpp` before `servo_protocol.*` changes.
- **V. Warning-Clean CMake + Ninja Builds**: PASS — verify clean build in `build-ninja/` with zero warnings.
