---

description: "Task list for E-Stop Safety Handling & RTC Time Synchronization"
---

# Tasks: E-Stop Safety Handling & RTC Time Synchronization

**Input**: Design documents from `/specs/001-estop-rtc-sync/`  
**Prerequisites**: [plan.md](file:///home/nguyenhuudu2007/QT_Creator/01_servo_hmi/specs/001-estop-rtc-sync/plan.md) (required), [spec.md](file:///home/nguyenhuudu2007/QT_Creator/01_servo_hmi/specs/001-estop-rtc-sync/spec.md) (required), [research.md](file:///home/nguyenhuudu2007/QT_Creator/01_servo_hmi/specs/001-estop-rtc-sync/research.md), [data-model.md](file:///home/nguyenhuudu2007/QT_Creator/01_servo_hmi/specs/001-estop-rtc-sync/data-model.md), [serial-protocol-and-state.md](file:///home/nguyenhuudu2007/QT_Creator/01_servo_hmi/specs/001-estop-rtc-sync/contracts/serial-protocol-and-state.md), [quickstart.md](file:///home/nguyenhuudu2007/QT_Creator/01_servo_hmi/specs/001-estop-rtc-sync/quickstart.md)

**Tests**: Unit tests using QtTest (`QTEST_GUILESS_MAIN`) and CTest are explicitly required by `FR-010` in [spec.md](file:///home/nguyenhuudu2007/QT_Creator/01_servo_hmi/specs/001-estop-rtc-sync/spec.md) and Principle IV of [constitution.md](file:///home/nguyenhuudu2007/QT_Creator/01_servo_hmi/.specify/memory/constitution.md).

**Organization**: Tasks are grouped by user story (`US1`, `US2`, `US3`) to enable independent implementation and testing of each increment.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: Which user story this task belongs to (`[US1]`, `[US2]`, `[US3]`)
- Every task includes exact repository file paths

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Configure CMake + Ninja build, strict compiler warnings, widget-free `ServoProtocol` target, and headless CTest/QtTest scaffolding.

- [X] T001 Create unit test directory and initial QtTest harness skeleton (`QTEST_GUILESS_MAIN`) in `tests/test_servo_protocol.cpp`
- [X] T002 Create initial `ServoProtocol` static library skeleton files in `servo_protocol.h` and `servo_protocol.cpp`
- [X] T003 Update `CMakeLists.txt` to add `ServoProtocol` static library (linking only `Qt::Core`), enable `-Wall -Wextra` on all targets, find `Qt::Test`, enable `CTest`, and register `test_servo_protocol` via `add_test()`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Define the shared widget-free protocol types, framer interface, and HMI session state contract required by all user stories.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

- [X] T004 Define `ProtocolMessage` (`ProtocolMessage.type`, `ProtocolMessage.frameKind`, `ProtocolMessage.timestamp`, `ProtocolMessage.servoAngle`, `ProtocolMessage.rawLine`) and `SerialLineFramer` (`SerialLineFramer.kMaxLineLength`, `SerialLineFramer.m_buffer`, `SerialLineFramer.m_discardUntilNewline`) in `servo_protocol.h` per [data-model.md](file:///home/nguyenhuudu2007/QT_Creator/01_servo_hmi/specs/001-estop-rtc-sync/data-model.md) and [serial-protocol-and-state.md](file:///home/nguyenhuudu2007/QT_Creator/01_servo_hmi/specs/001-estop-rtc-sync/contracts/serial-protocol-and-state.md)
- [X] T005 Define `HmiSessionState` (`HmiSessionState.connected`, `HmiSessionState.lockState`, `HmiSessionState.lockReason`, `HmiSessionState.unlockRequestState`, `HmiSessionState.rtcSyncState`, `HmiSessionState.rtcLastTimestamp`, `HmiSessionState.currentAngle`) and command formatter declarations in `servo_protocol.h` per [data-model.md](file:///home/nguyenhuudu2007/QT_Creator/01_servo_hmi/specs/001-estop-rtc-sync/data-model.md)

**Checkpoint**: Foundation ready — user story implementation can now begin.

---

## Phase 3: User Story 1 - E-Stop Lockout & Safe Recovery (Priority: P1) 🎯 MVP

**Goal**: Detect STM32 E-Stop lockouts (`$ALM:HHMMSS:BTN:PRESSED`, `$ALM:HHMMSS:SYS:LOCKED`), display a prominent non-modal locked-state banner, stop sweeps, force displayed angle to `0`, disable all servo motion controls across disconnect/reconnect, and unlock only upon `$TLM:...:SYS:UNLOCKED` (or `$ALM:...:SYS:BOOT`).

**Independent Test**: Simulate incoming `$ALM:HHMMSS:BTN:PRESSED` and `$ALM:HHMMSS:SYS:LOCKED` lines, verify banner visibility and disabled servo controls with zero `$CMD:SRV` emissions (including after disconnect/reconnect), trigger Unlock to emit `$CMD:SYS:UNLOCK` with 2s timeout feedback, and verify unlock only upon `$TLM:HHMMSS:SYS:UNLOCKED` or `$ALM:HHMMSS:SYS:BOOT`.

### Tests for User Story 1

> **NOTE: Write these tests FIRST and verify they FAIL before implementing US1 logic.**

- [X] T006 [US1] Add QtTest cases in `tests/test_servo_protocol.cpp` covering E-Stop/system frame parsing (`BTN:PRESSED`, `SYS:LOCKED`, `SYS:BOOT`, `SYS:UNLOCKED`), `formatUnlockCommand()`/`formatServoCommand()`, `HmiSessionState` lockout/unlock transitions, reconnect-while-locked startup `$CMD:SRV` suppression, and 2s unlock timeout state

### Implementation for User Story 1

- [X] T007 [US1] Implement E-Stop/system frame parsing (`ProtocolMessage.type`, `ProtocolMessage.frameKind`, `ProtocolMessage.timestamp`), `formatUnlockCommand()`, `formatServoCommand()`, and safety lock/unlock/boot/reconnect state transitions (`HmiSessionState.connected`, `HmiSessionState.lockState`, `HmiSessionState.lockReason`, `HmiSessionState.unlockRequestState`, `HmiSessionState.currentAngle`) in `servo_protocol.cpp` per [data-model.md](file:///home/nguyenhuudu2007/QT_Creator/01_servo_hmi/specs/001-estop-rtc-sync/data-model.md)
- [X] T008 [P] [US1] Add `setLocked(bool locked)` motion-control disablement (preserving `syncAngleFromHardware(0)`) in `slider_page.h` and `slider_page.cpp`
- [X] T009 [P] [US1] Add `setLocked(bool locked)` sweep stop and control disablement in `sweep_page.h` and `sweep_page.cpp`
- [X] T010 [US1] Declare safety banner widgets, Unlock button, `HmiSessionState`, `SerialLineFramer`, and 2s unlock timeout `QTimer` members/slots in `main_window.h`
- [X] T011 [US1] Implement prominent non-modal locked-state banner, Unlock `$CMD:SYS:UNLOCK` action with `"Waiting for controller unlock..."`/2s retry notice, `SliderPage`/`SweepPage` lockouts, `$CMD:SRV` send guards (`canSendServoCommand()`), and disconnect/reconnect lock preservation with startup `$CMD:SRV` suppression in `main_window.cpp`

**Checkpoint**: User Story 1 is independently functional and passes US1 unit tests.

---

## Phase 4: User Story 2 - One-Click GMT+7 RTC Time Synchronization (Priority: P2)

**Goal**: Provide a one-click `"Sync PC Time (GMT+7)"` action that always sends UTC+07:00 `$CMD:RTC:HH:MM:SS` and displays non-modal inline status feedback (`Pending`, `Synced` on `$TLM:...:RTC:OK`, `Error` on `$ALM:...:RTC:ERR`, or `TimedOut` after 2s).

**Independent Test**: Click `"Sync PC Time (GMT+7)"` while connected (in any host timezone), verify `$CMD:RTC:HH:MM:SS` matches UTC+07:00, verify inline status transitions from `Pending` to `Synced`, `Error`, or 2s `TimedOut` without modal dialogs, and verify button is disabled when disconnected.

### Tests for User Story 2

> **NOTE: Write these tests FIRST and verify they FAIL before implementing US2 logic.**

- [X] T012 [US2] Add QtTest cases in `tests/test_servo_protocol.cpp` covering `RTC:OK`/`RTC:ERR` parsing, deterministic fixed UTC+07:00 `formatGmt7RtcCommand(utcNow)` across non-GMT+7 `QDateTime` inputs, and `HmiSessionState` RTC sync transitions (`Idle`, `Pending`, `Synced`, `Error`, `TimedOut`)

### Implementation for User Story 2

- [X] T013 [US2] Implement `RTC:OK`/`RTC:ERR` parsing, `formatGmt7RtcCommand(utcNow)` (UTC+07:00 offset), and `HmiSessionState` RTC sync state transitions (`HmiSessionState.rtcSyncState`, `HmiSessionState.rtcLastTimestamp`) in `servo_protocol.cpp` per [data-model.md](file:///home/nguyenhuudu2007/QT_Creator/01_servo_hmi/specs/001-estop-rtc-sync/data-model.md)
- [X] T014 [US2] Add inline `m_lblRtcStatus` and 2s `m_rtcTimeoutTimer` declarations to `main_window.h`
- [X] T015 [US2] Update `m_btnSyncRtc` text to `"Sync PC Time (GMT+7)"`, send `formatGmt7RtcCommand()` in `syncRtcNow()`, wire inline `m_lblRtcStatus` updates (`Pending`/`Synced`/`Error`/`TimedOut`) + 2s timeout, and log RTC outcomes in `main_window.cpp`

**Checkpoint**: User Stories 1 and 2 both work independently and pass US1/US2 unit tests.

---

## Phase 5: User Story 3 - Decoupled Protocol Line Parsing & Verification (Priority: P3)

**Goal**: Complete strict `$TLM`/`$ALM` line validation (`SRV:<angle>` `0..180`, 6-digit `HHMMSS`, prefix-to-event strictness, `Invalid` classification) and bounded chunk-to-line serial framing (`SerialLineFramer` with 512-byte overflow cap and disconnect/connect reset) inside `ServoProtocol`, integrated cleanly into `MainWindow` and verified by headless unit tests.

**Independent Test**: Run `ctest --test-dir build-ninja --output-on-failure` in a headless environment with zero GUI/hardware dependencies and verify all valid, split-chunk, `\r\n`, overflow-recovery, out-of-range, wrong-prefix, bad-timestamp, and malformed-line tests pass.

### Tests for User Story 3

> **NOTE: Write these tests FIRST and verify they FAIL before implementing US3 logic.**

- [X] T016 [US3] Add comprehensive QtTest cases in `tests/test_servo_protocol.cpp` covering `SRV:<angle>` boundary values (`0`, `90`, `180`, `-1`, `181`, non-numeric, extra colons), wrong `$TLM`/`$ALM` prefix rejection, malformed/non-6-digit timestamps, empty/whitespace lines, `SerialLineFramer` split multi-chunk reassembly, `\r\n` normalization, 512-byte overflow discard + newline recovery, and `reset()`

### Implementation for User Story 3

- [X] T017 [US3] Implement `SerialLineFramer::appendBytes()` / `SerialLineFramer::reset()` (`SerialLineFramer.kMaxLineLength`, `SerialLineFramer.m_buffer`, `SerialLineFramer.m_discardUntilNewline`) and strict `parseProtocolLine()` (`ProtocolMessage.timestamp`, `ProtocolMessage.servoAngle`, `ProtocolMessage.rawLine`, `SRV:0..180` validation, `Invalid` fallback) in `servo_protocol.cpp` per [data-model.md](file:///home/nguyenhuudu2007/QT_Creator/01_servo_hmi/specs/001-estop-rtc-sync/data-model.md)
- [X] T018 [US3] Replace remaining `m_rxBuffer`/substring `line.contains()` parsing in `MainWindow::onSerialReadyRead()`, `MainWindow::handleIncomingLine()`, and `MainWindow::toggleConnection()` with `SerialLineFramer` (`reset()` on connect/disconnect) + `parseProtocolLine()` + `HmiSessionState::applyMessage()` in `main_window.h` and `main_window.cpp`

**Checkpoint**: All user stories (US1, US2, US3) are independently functional and fully covered by headless unit tests.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Final constitution compliance verification, warning-clean build, headless CTest execution, and end-to-end quickstart checks.

- [X] T019 [P] Audit `servo_protocol.h` and `servo_protocol.cpp` to confirm zero `QtWidgets`/`QSerialPort` includes and audit `main_window.cpp` to confirm zero `waitForReadyRead()` calls and `/dev/scada_rs232` preservation
- [X] T020 Clean configure and build all targets (`ServoProtocol`, `ServoHMI`, `test_servo_protocol`) with Ninja in `build-ninja/` and confirm zero compiler warnings (`-Wall -Wextra`)
- [X] T021 Run headless unit tests via `ctest --test-dir build-ninja --output-on-failure` and verify 100% pass rate
- [X] T022 Validate end-to-end scenarios against [quickstart.md](file:///home/nguyenhuudu2007/QT_Creator/01_servo_hmi/specs/001-estop-rtc-sync/quickstart.md) (virtual PTY `socat` or binary launch check)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — starts immediately.
- **Foundational (Phase 2)**: Depends on Phase 1 (`T001`–`T003`) — BLOCKS all user stories.
- **User Story 1 (Phase 3)**: Depends on Phase 2 (`T004`–`T005`).
- **User Story 2 (Phase 4)**: Depends on Phase 2 (`T004`–`T005`); modifies `servo_protocol.cpp`, `tests/test_servo_protocol.cpp`, `main_window.h`, `main_window.cpp` sequentially after US1 edits on those shared files.
- **User Story 3 (Phase 5)**: Depends on Phase 2 (`T004`–`T005`); finalizes `SerialLineFramer` and `parseProtocolLine()` integration in `main_window.cpp` after US1/US2.
- **Polish (Phase 6)**: Depends on Phases 3, 4, and 5 completion.

### User Story Dependencies

- **User Story 1 (P1)**: Independent MVP slice after Foundational phase (`T006`–`T011`).
- **User Story 2 (P2)**: Independent functional slice after Foundational phase (`T012`–`T015`); integrates cleanly into `MainWindow` top bar without depending on US1 lockout UI.
- **User Story 3 (P3)**: Independent verification/hardening slice (`T016`–`T018`) completing strict validation and bounded framing across all message types.

### Within Each User Story

1. Write failing QtTest unit tests in `tests/test_servo_protocol.cpp` first.
2. Implement non-GUI protocol/state logic in `servo_protocol.cpp`.
3. Update widget pages (`slider_page.*`, `sweep_page.*`) in parallel where independent.
4. Wire `MainWindow` (`main_window.h`, `main_window.cpp`).

### Parallel Opportunities

- Within **US1**: `T008` (`slider_page.h`/`.cpp`) and `T009` (`sweep_page.h`/`.cpp`) can execute in parallel with each other and with `T007` (`servo_protocol.cpp`).
- Within **Polish**: `T019` (header/blocking-call audit) can run in parallel before or alongside `T020` build verification.

---

## Parallel Example: User Story 1

```bash
# After T006 (US1 tests written and failing):
# Run in parallel across separate files:
Task T007: "Implement E-Stop/system parsing and HmiSessionState lock transitions in servo_protocol.cpp"
Task T008: "Add setLocked(bool locked) to slider_page.h and slider_page.cpp"
Task T009: "Add setLocked(bool locked) to sweep_page.h and sweep_page.cpp"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (`T001`–`T003`).
2. Complete Phase 2: Foundational (`T004`–`T005`).
3. Complete Phase 3: User Story 1 (`T006`–`T011`).
4. **STOP and VALIDATE**: Build in `build-ninja/`, run `ctest --test-dir build-ninja --output-on-failure`, and verify E-Stop lockout + Unlock recovery works end-to-end.

### Incremental Delivery

1. Setup + Foundational → `ServoProtocol` and `CTest` scaffold ready.
2. Add User Story 1 → Safety lockout, locked banner, motion disablement, and Unlock flow verified (MVP).
3. Add User Story 2 → Fixed GMT+7 RTC sync and inline status indicators verified.
4. Add User Story 3 → Full strict `$TLM`/`$ALM` validation, `SerialLineFramer` overflow/split-chunk handling, and complete headless regression suite verified.
5. Polish → Warning-clean Ninja build, full CTest pass, and `quickstart.md` validation.
