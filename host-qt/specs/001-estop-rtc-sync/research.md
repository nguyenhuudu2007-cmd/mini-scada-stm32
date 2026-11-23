# Research: E-Stop Safety Handling & RTC Time Synchronization

**Feature**: `001-estop-rtc-sync`  
**Date**: 2026-11-22  
**Status**: Complete

## 1. Non-GUI Protocol & State Module Architecture

- **Decision**: Extract serial line framing, `$TLM`/`$ALM` frame parsing, GMT+7 `$CMD:RTC` formatting, and safety/RTC HMI state transitions into a standalone CMake static library target `ServoProtocol` (`servo_protocol.h` / `servo_protocol.cpp`) that links solely against `Qt6::Core` (or `Qt5::Core` fallback).
- **Rationale**: Linking `ServoProtocol` strictly to `Qt::Core` enforces Constitution Principle III at compile and link time—no `QWidget`, `QMainWindow`, or `QSerialPort` dependency can accidentally enter the parsing or state-transition layer. Both `ServoHMI` and the `QtTest` executable share the exact same compiled logic.
- **Alternatives considered**:
  - *Pure STL (`std::string`) without `QtCore`*: Rejected because the codebase already uses `QString`, `QByteArray`, and `QDateTime` across all interfaces; using `Qt::Core` avoids redundant string conversions while remaining 100% headless and widget-free.
  - *Keeping a header-only parser inside `main_window.cpp`*: Rejected because it cannot be independently compiled or tested cleanly with CTest/QtTest.

## 2. Chunk-to-Line Serial Framing & Buffer Safety

- **Decision**: Implement a stateful `SerialLineFramer` inside `ServoProtocol` with a 512-byte maximum line length cap, normalizing `\r\n` and `\n`, trimming whitespace, skipping empty lines, and discarding oversized unterminated chunks until the next `\n`. Call `reset()` on serial connect and disconnect.
- **Rationale**: `QSerialPort::readyRead()` delivers arbitrary byte chunks that frequently split frames or combine multiple frames. Putting `SerialLineFramer` in `ServoProtocol` enables direct unit testing of split-chunk reassembly, `\r\n` handling, overflow recovery, and buffer cleanup without mocking `QSerialPort`.
- **Alternatives considered**:
  - *Leaving `m_rxBuffer` ad-hoc inside `MainWindow::onSerialReadyRead()`*: Rejected because partial-chunk and overflow edge cases would remain untested in unit tests.
  - *Using `QSerialPort::canReadLine()`*: Rejected because it couples framing directly to `QIODevice` and lacks explicit overflow discard protection for unterminated noisy streams.

## 3. Strict `$TLM` / `$ALM` Frame Validation

- **Decision**: Validate incoming lines strictly against `<prefix>:<HHMMSS>:<body...>` where:
  - Prefix is strictly `$TLM` or `$ALM` matching the allowed event family (`$TLM` for `SYS:UNLOCKED`, `RTC:OK`, `SRV:<angle>`; `$ALM` for `BTN:PRESSED`, `SYS:LOCKED`, `SYS:BOOT`, `RTC:ERR`).
  - Timestamp is strictly 6 ASCII digits (`0`–`9`).
  - `SRV:<angle>` requires a clean base-10 integer within `0..180` (no trailing characters or negative/overflow values).
  - Unrecognized or malformed lines return `MessageType::Invalid` with `rawLine` preserved for RX logging and trigger zero state changes.
- **Rationale**: Replaces fragile `QString::contains(...)` substring matching in the current `main_window.cpp`, preventing false lockouts, spurious unlocks, or invalid angles from corrupted serial traffic.
- **Alternatives considered**:
  - *Substring matching (`line.contains(...)`)*: Rejected because corrupted frames or log echoes could inadvertently match tokens and cause unsafe state changes.

## 4. GMT+7 (`UTC+07:00`) Time Formatting

- **Decision**: Format `$CMD:RTC:HH:MM:SS\n` using `QDateTime::currentDateTimeUtc().toOffsetFromUtc(7 * 3600).toString("HH:mm:ss")` inside a testable helper accepting an explicit `QDateTime` parameter (defaulting to current UTC time).
- **Rationale**: Guarantees GMT+7 time output regardless of the Fedora host's local timezone configuration (e.g., UTC or other zones), and allows deterministic unit testing with fixed UTC timestamps.
- **Alternatives considered**:
  - *`QDateTime::currentDateTime()`*: Rejected because it uses the OS local timezone instead of fixed GMT+7.
  - *`QTimeZone("Asia/Ho_Chi_Minh")`*: Equivalent for modern dates, while `toOffsetFromUtc(7 * 3600)` has zero external tzdata lookup failure modes and directly models fixed GMT+7.

## 5. Headless CTest + QtTest Execution & Warning-Clean Ninja Build

- **Decision**: Enable `include(CTest)` and `find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets SerialPort Test)` in `CMakeLists.txt`. Build unit test binary `test_servo_protocol` using `QTEST_GUILESS_MAIN` (no GUI/X11/Wayland session needed), register it with `add_test()`, and enable `-Wall -Wextra` on `ServoProtocol`, `ServoHMI`, and `test_servo_protocol`.
- **Rationale**: Verified on this Fedora environment that `Qt6Test` and `Qt6Core` are installed (`/usr/lib64/cmake/Qt6Test`), CMake 4.3.0 + Ninja 1.13.2 + GCC 16.2.1 are present, and `QTEST_GUILESS_MAIN` runs completely headlessly inside `ctest --test-dir build-ninja --output-on-failure`.
- **Alternatives considered**:
  - *`QTEST_MAIN`*: Rejected because it initializes `QApplication` and requires a display server or `QT_QPA_PLATFORM=offscreen`, whereas `QTEST_GUILESS_MAIN` proves zero GUI dependency.
