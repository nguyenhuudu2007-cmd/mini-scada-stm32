---
description: "Task list for Hardware Test Mode, Serial Disconnect Recovery & STM32 Protocol Alignment"
---

# Tasks: Hardware Test Mode, Serial Disconnect Recovery & STM32 Protocol Alignment

## Phase 1: Setup & Ignore Hygiene
- [X] T001 Verify `.gitignore` includes `logs/` and `*.user` in `.gitignore`

## Phase 2: User Story 1 - Protocol Alignment & TDD (`ServoProtocol` + STM32)
- [X] T002 [US1] Write failing QtTest unit tests for `$TLM:<HHMMSS>:LED:<0|1>`, invalid LED frames, `formatLedCommand()`, and `isServoMotionCommand()` in `tests/test_servo_protocol.cpp`
- [X] T003 [US1] Implement `MessageType::TelemetryLedState`, `ProtocolMessage.ledState`, `formatLedCommand()`, and `isServoMotionCommand()` in `servo_protocol.h` and `servo_protocol.cpp`
- [X] T004 [P] [US1] Align STM32 firmware (`$ALM:...:BTN:PRESSED`, remove 1s auto-unlock on `$CMD:SRV`, emit `$ALM:...:SYS:BOOT` at startup) in `/home/nguyenhuudu2007/STM32CubeIDE/workspace_2.2.0/stm32_SCADA_w1/Core/Src/main.c`

## Phase 3: User Story 2 - Hardware Test Mode UI
- [X] T005 [P] [US2] Create `HardwareTestPage` widget (LED `PC13`, quick Servo steps, non-blocking `0->90->180->0` sequence, raw ASCII console) in `hardware_test_page.h` and `hardware_test_page.cpp`
- [X] T006 [US2] Register `hardware_test_page.h` and `hardware_test_page.cpp` in `CMakeLists.txt`
- [X] T007 [US2] Integrate `HardwareTestPage` into `MainWindow`, enforce `canSendServoCommand()` on raw commands, stop `Auto Sweep` when entering `HardwareTest`, stop self-test when leaving `HardwareTest`, and sync LED telemetry in `main_window.h` and `main_window.cpp`

## Phase 4: User Story 3 - Disconnect Recovery & Log Folder Action
- [X] T008 [P] [US3] Add concise filename label and `Open Folder` button in `log_page.h` and `log_page.cpp`
- [X] T009 [US3] Extract shared non-blocking `teardownActiveSession()` helper, handle fatal `QSerialPort::errorOccurred` link-loss disconnects, and remove blocking `waitForBytesWritten()` calls in `main_window.h` and `main_window.cpp`

## Phase 5: Polish & Verification
- [X] T010 Build `build-ninja/` cleanly with `-Wall -Wextra` and run `ctest --test-dir build-ninja --output-on-failure`
- [X] T011 Verify STM32 `Core/Src/main.c` compiles with `arm-none-eabi-gcc -Wall -Wextra -fsyntax-only` (full `.elf` link in `Debug/` deferred to STM32CubeIDE toolchain due to `-fcyclomatic-complexity`)
- [X] T012 Audit header isolation (`Qt::Core` only in `ServoProtocol`) and non-blocking `MainWindow` serial I/O, and run verbose `test_servo_protocol -v2` (`12/12` passed)

## Phase 6: User Story 4 - 4-Step Bring-Up Diagnostic Checklist & RTT Latency
- [X] T013 [US4] Write failing QtTest cases for `HardwareBringUpTracker` (4-step state transitions, PASS/FAIL on `$TLM`/`$ALM`, timeout handling, RTT ms recording) in `tests/test_servo_protocol.cpp`
- [X] T014 [US4] Implement `BringUpStep`, `BringUpStatus`, `BringUpStepResult`, and `HardwareBringUpTracker` in `servo_protocol.h` and `servo_protocol.cpp`
- [X] T015 [US4] Add 4-step Bring-Up Diagnostic Checklist UI, `QElapsedTimer` RTT measurement, and `500 ms` no-ACK warning (`10 s` for PA0 E-Stop press) in `hardware_test_page.h` and `hardware_test_page.cpp`
- [X] T016 [US4] Forward parsed `ProtocolMessage` replies and GMT+7 RTC trigger wiring to `HardwareTestPage` in `main_window.h` and `main_window.cpp`
- [X] T017 [US4] Build `build-ninja/` cleanly (`-Wall -Wextra`) and run `ctest --test-dir build-ninja --output-on-failure`
- [X] T018 [US4] Run `test_servo_protocol -v2` and verify `Qt::Core`-only isolation in `servo_protocol.*`
