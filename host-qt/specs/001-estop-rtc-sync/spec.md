# Feature Specification: E-Stop Safety Handling & RTC Time Synchronization

**Feature Branch**: `001-estop-rtc-sync`

**Created**: 2026-11-22

**Status**: Draft

**Input**: User description: "Add full E-Stop safety handling and RTC time synchronization to ServoHMI using the existing STM32 ASCII serial protocol. When the STM32 reports $ALM:HHMMSS:SYS:LOCKED or a button press, show a visible locked state banner, disable servo motion controls, and provide an Unlock action that sends $CMD:SYS:UNLOCK and unlocks controls upon $TLM:...:SYS:UNLOCKED. Add a one-click "Sync PC Time (GMT+7)" action that sends $CMD:RTC:HH:MM:SS and displays confirmation from $TLM:...:RTC:OK or error from $ALM:...:RTC:ERR. Extract incoming line parsing ($TLM and $ALM) into a testable non-GUI parser module with unit tests."

## Clarifications

### Session 2026-11-22

- Q: What should happen to the safety lock state and initial startup servo command when the HMI disconnects while locked and then reconnects to the controller? → A: Preserve the locked state across disconnect/reconnect, keep servo controls disabled, suppress the automatic startup `$CMD:SRV` command, and exit lock only upon `$TLM:...:SYS:UNLOCKED` or `$ALM:...:SYS:BOOT`.
- Q: How should the standalone incoming line parser handle malformed lines, mismatched `$TLM`/`$ALM` prefixes, non-6-digit timestamps, and out-of-range `SRV:<angle>` values? → A: Enforce strict frame structure: require `$TLM:` or `$ALM:` prefix with matching expected event category, a 6-digit numeric `HHMMSS` timestamp, and integer `0..180` for `SRV:<angle>`; classify anything else as `Invalid`/`Unknown` without triggering state transitions.
- Q: How should partial serial chunks arriving across multiple read events be buffered, bounded against unterminated noise, and cleaned up on disconnect? → A: Handle chunk-to-line buffering inside the testable non-GUI protocol module (supporting `\n` and `\r\n`, ignoring empty lines), discard oversized unterminated chunks beyond a fixed safety limit (512 bytes) until the next newline, and clear the buffer on disconnect/connect.
- Q: How should the HMI display RTC sync outcomes and unacknowledged Unlock requests so operators receive clear feedback without blocking incoming serial alarms? → A: Use non-modal inline status feedback only (an RTC status indicator showing Pending / Synced / Error in the persistent top bar, and a "Waiting for controller unlock..." status in the locked banner) while keeping both actions immediately re-clickable for retry if no reply arrives within a 2-second timeout or on demand.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - E-Stop Lockout & Safe Recovery (Priority: P1)

An operator controlling the servo from the desktop HMI experiences an emergency stop triggered either by the physical hardware button or a controller safety lock report. The HMI immediately alerts the operator with a prominent locked-state banner, halts any automated sweep, locks out all manual and automated servo motion controls, and provides a dedicated Unlock action that restores servo control only after the controller confirms the unlock.

**Why this priority**: Operator and equipment safety is the highest priority in a servo control HMI; preventing unintended motion commands during an emergency stop and requiring explicit acknowledgement before resuming operation is safety-critical.

**Independent Test**: Can be fully tested by simulating incoming `$ALM:HHMMSS:BTN:PRESSED` and `$ALM:HHMMSS:SYS:LOCKED` frames from the controller, verifying that the locked banner appears, servo motion controls become disabled, clicking Unlock transmits `$CMD:SYS:UNLOCK`, and controls remain locked until `$TLM:HHMMSS:SYS:UNLOCKED` is received.

**Acceptance Scenarios**:

1. **Given** the HMI is connected and servo controls are active (either manual slider or auto sweep running), **When** the controller reports `$ALM:HHMMSS:BTN:PRESSED` or `$ALM:HHMMSS:SYS:LOCKED`, **Then** any running auto sweep stops immediately, the displayed angle updates to `0` degrees, a prominent locked-state banner becomes visible, and all servo motion controls are disabled.
2. **Given** the HMI is in the locked state and connected to the controller, **When** the operator activates the Unlock action, **Then** the HMI transmits `$CMD:SYS:UNLOCK` to the controller and keeps servo motion controls disabled until confirmation arrives.
3. **Given** the HMI is in the locked state and an unlock has been requested, **When** the controller reports `$TLM:HHMMSS:SYS:UNLOCKED`, **Then** the locked-state banner clears, servo motion controls become enabled again, and the event is recorded in the HMI status/log view.
4. **Given** the HMI is in the locked state, **When** the operator attempts to adjust manual angle controls or start auto sweep before unlocking, **Then** no `$CMD:SRV` motion commands are transmitted.

---

### User Story 2 - One-Click GMT+7 RTC Time Synchronization (Priority: P2)

An operator wants the embedded controller's real-time clock synchronized to local Vietnam time (GMT+7) with a single click, and wants immediate visual confirmation in the HMI whether the controller successfully updated its clock or encountered a clock hardware error.

**Why this priority**: Accurate controller timestamps on `$TLM` and `$ALM` frames and LCD status require reliable clock synchronization with clear operator feedback on success or I2C clock failure.

**Independent Test**: Can be tested independently by connecting the HMI, triggering the "Sync PC Time (GMT+7)" action, verifying `$CMD:RTC:HH:MM:SS` is transmitted using UTC+07:00 time, and verifying distinct confirmation vs. error feedback upon receiving `$TLM:HHMMSS:RTC:OK` or `$ALM:HHMMSS:RTC:ERR`.

**Acceptance Scenarios**:

1. **Given** the HMI is connected to the controller, **When** the operator triggers the "Sync PC Time (GMT+7)" action, **Then** the HMI transmits `$CMD:RTC:HH:MM:SS` formatted in GMT+7 (UTC+07:00) 24-hour time.
2. **Given** an RTC sync request was sent, **When** the controller responds with `$TLM:HHMMSS:RTC:OK`, **Then** the HMI displays a visible confirmation status indicating RTC synchronization succeeded (including the timestamp or confirmation message).
3. **Given** an RTC sync request was sent, **When** the controller responds with `$ALM:HHMMSS:RTC:ERR`, **Then** the HMI displays a visible warning/error status indicating RTC synchronization failed without disrupting other HMI controls.
4. **Given** the HMI is disconnected from the controller, **When** viewing the "Sync PC Time (GMT+7)" control, **Then** the action is disabled so invalid offline sync attempts cannot occur.

---

### User Story 3 - Decoupled Protocol Line Parsing & Verification (Priority: P3)

A developer or maintainer modifies or extends incoming controller message handling (`$TLM` and `$ALM` frames) and needs confidence that every message type—telemetry angles, E-Stop button presses, system lock/unlock/boot states, RTC OK/ERR responses, and malformed lines—is parsed deterministically outside the visual interface and verified by automated unit tests.

**Why this priority**: Separating protocol parsing from visual interface components enables fast, repeatable, hardware-free verification of safety and synchronization state transitions.

**Independent Test**: Can be tested independently by running the automated unit test suite against the standalone parser module with valid, partial, boundary, and malformed `$TLM` and `$ALM` input lines without launching a graphical window or serial device.

**Acceptance Scenarios**:

1. **Given** valid `$TLM` and `$ALM` ASCII lines (including `BTN:PRESSED`, `SYS:LOCKED`, `SYS:UNLOCKED`, `SYS:BOOT`, `RTC:OK`, `RTC:ERR`, and `SRV:<angle>`), **When** passed to the standalone parser module, **Then** each line yields a structured, typed parse result containing the frame category, timestamp, event/status type, and parsed payload (such as angle).
2. **Given** unrecognized, empty, truncated, or out-of-range lines (such as non-numeric servo angles or missing delimiters), **When** passed to the standalone parser module, **Then** the parser rejects or classifies them safely as invalid/unknown without crashing or producing spurious state transitions.
3. **Given** the automated test suite is executed in a headless environment, **When** all parser unit tests run, **Then** every test passes with no hardware or graphical session dependency.

---

### Edge Cases

- What happens when `$ALM:HHMMSS:SYS:LOCKED` is received multiple times in a row while already locked? The HMI remains in the locked state, keeps servo controls disabled, keeps the locked banner visible, and avoids redundant disruptive state resets.
- What happens when the serial connection is closed while the HMI is in the locked state? Servo controls remain disabled while disconnected and the Unlock action is disabled (since `$CMD:SYS:UNLOCK` cannot be sent offline). Upon reconnecting, the HMI preserves the locked state, keeps servo controls disabled, suppresses the automatic startup `$CMD:SRV` command, enables the Unlock action, and exits the locked state only when `$TLM:...:SYS:UNLOCKED` or `$ALM:...:SYS:BOOT` is received.
- What happens if `$ALM:HHMMSS:SYS:BOOT` arrives while the HMI was locked? Because the controller has physically reset, the HMI transitions out of the prior E-Stop lockout consistent with a controller boot (angle `0`, boot notification shown, controls ready when connected) and logs the reset event.
- What happens if an incoming line contains `\r\n` endings, leading/trailing whitespace, partial chunks arriving across multiple serial read events, or unterminated noise? The non-GUI protocol module buffers incoming byte chunks, normalizes `\r\n` and `\n`, trims whitespace, skips empty lines, only dispatches complete newline-terminated lines, discards any unterminated line exceeding a 512-byte safety cap until the next newline, and clears any residual partial buffer on disconnect and connect.
- What happens if the host PC's operating system timezone is set to something other than GMT+7 (for example, UTC)? The "Sync PC Time (GMT+7)" action computes UTC+07:00 explicitly so the transmitted `HH:MM:SS` is always GMT+7.
- What happens if the operator clicks Unlock repeatedly before `$TLM:...:SYS:UNLOCKED` arrives, or if no acknowledgement arrives for Unlock or RTC sync? The HMI displays a non-modal inline waiting status ("Waiting for controller unlock..." in the locked banner, or `Pending` in the persistent top-bar RTC indicator) without blocking incoming serial updates, keeps both controls immediately re-clickable for safe retry, and transitions the pending indicator to a timed-out/retry-ready notice if no response arrives within 2 seconds.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST recognize incoming `$ALM:<timestamp>:BTN:PRESSED` and `$ALM:<timestamp>:SYS:LOCKED` messages as E-Stop lockout triggers.
- **FR-002**: Upon entering E-Stop lockout, System MUST display a prominent, unmistakable locked-state banner in the HMI, halt any running auto sweep immediately, synchronize displayed servo angle controls to `0` degrees without emitting a servo command, and disable all manual and automated servo motion controls.
- **FR-003**: System MUST provide an explicit Unlock action in the HMI (accessible from the locked-state banner or persistent status area) that is enabled only when the HMI is both connected and in the locked state.
- **FR-004**: When the operator triggers the Unlock action, System MUST transmit `$CMD:SYS:UNLOCK\n` to the controller, show a non-modal inline "Waiting for controller unlock..." status in the locked-state banner (with a 2-second response timeout notice if unacknowledged), keep the Unlock action re-clickable for retry, and MUST keep servo motion controls disabled until `$TLM:<timestamp>:SYS:UNLOCKED` (or `$ALM:<timestamp>:SYS:BOOT`) is received.
- **FR-005**: Upon receiving `$TLM:<timestamp>:SYS:UNLOCKED` (or `$ALM:<timestamp>:SYS:BOOT` after a controller reset), System MUST exit the locked state, dismiss or clear the locked-state banner, re-enable servo motion controls (respecting active connection state), and record the event in the status/log display. Disconnecting and reconnecting MUST preserve the locked state and suppress any automatic startup `$CMD:SRV` command until unlock or boot confirmation is received.
- **FR-006**: System MUST provide a one-click "Sync PC Time (GMT+7)" action that is enabled whenever a serial connection is active (including when locked, unless prevented by connection state) and transmits `$CMD:RTC:HH:MM:SS\n` using the current PC time converted to fixed GMT+7 (UTC+07:00).
- **FR-007**: System MUST display explicit non-modal inline visual feedback in the persistent top bar (`Pending`, `Synced` upon `$TLM:<timestamp>:RTC:OK`, `Error` upon `$ALM:<timestamp>:RTC:ERR`, or a timeout notice if no reply arrives within 2 seconds) without using blocking modal dialogs, in addition to recording every outcome in the event log.
- **FR-008**: System MUST extract incoming ASCII stream buffering (chunk-to-line framing with a 512-byte max line cap, `\r\n`/`\n` normalization, empty-line skipping, and buffer reset) and `$TLM`/`$ALM` line parsing into a standalone, non-GUI protocol module that has no dependency on visual interface components.
- **FR-009**: The standalone parser module MUST enforce strict frame validation requiring a `$TLM:` or `$ALM:` prefix matching the expected message category (`$TLM` for `SYS:UNLOCKED`, `RTC:OK`, `SRV:<angle>`; `$ALM` for `BTN:PRESSED`, `SYS:LOCKED`, `SYS:BOOT`, `RTC:ERR`), a 6-digit numeric `HHMMSS` timestamp, and integer values within `0..180` for `SRV:<angle>`. Any line failing validation (wrong prefix, malformed timestamp, non-integer or out-of-range angle, missing fields, or unrecognized tokens) MUST be classified as `Invalid`/`Unknown` with its raw text preserved for logging and MUST NOT trigger any state transition.
- **FR-010**: System MUST include automated unit tests exercising the standalone protocol module across all supported `$TLM` and `$ALM` message types as well as split multi-chunk reads, `\r\n` endings, buffer overflow recovery, buffer reset, and malformed/out-of-range inputs, executable without physical hardware or a graphical display.
- **FR-011**: System MUST continue to process all serial input asynchronously via event-driven notifications without blocking the user interface thread, and MUST preserve existing protocol commands (`$CMD:LED`, `$CMD:SRV`, `$CMD:SYS:UNLOCK`, `$CMD:RTC`) and `/dev/scada_rs232` port discovery.

### Key Entities

- **Protocol Message (`$TLM` / `$ALM`)**: A newline-terminated ASCII message emitted by the controller containing a frame prefix (`$TLM` for telemetry/confirmation or `$ALM` for alarm/fault), a 6-digit timestamp (`HHMMSS`), a subsystem/event code (e.g., `SYS:LOCKED`, `SYS:UNLOCKED`, `SYS:BOOT`, `BTN:PRESSED`, `RTC:OK`, `RTC:ERR`, `SRV:<angle>`), and optional numeric payload.
- **Safety Lock State**: The HMI's operational safety state (`Unlocked` vs. `Locked` / `E-Stop`), controlling whether servo motion controls are enabled, whether the locked-state banner is visible, and whether the Unlock action is available.
- **RTC Sync Status**: The most recent outcome of a PC-to-controller clock synchronization attempt (`Idle`, `Pending`, `Synced (OK)`, `Error`, or `TimedOut`) along with its displayed inline status message.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Within one user interface refresh cycle (<100 ms) of receiving `$ALM:HHMMSS:BTN:PRESSED` or `$ALM:HHMMSS:SYS:LOCKED`, 100% of servo motion controls are disabled, any active sweep is halted, and the locked-state banner is visible.
- **SC-002**: Zero servo position commands (`$CMD:SRV`) are transmitted while the HMI is in the locked state across manual slider, spinbox, and auto-sweep interactions.
- **SC-003**: Operators can trigger RTC synchronization in a single click ("Sync PC Time (GMT+7)"), with the transmitted timestamp matching UTC+07:00 within 1 second of PC wall-clock time and visual confirmation or error status displayed within <100 ms of receiving `$TLM:...:RTC:OK` or `$ALM:...:RTC:ERR`.
- **SC-004**: 100% of supported `$TLM` and `$ALM` message types plus invalid/malformed line edge cases are covered by automated headless unit tests that pass completely on every build.
- **SC-005**: Building the application and unit test targets from clean state produces zero compiler warnings and passes all automated tests.

## Assumptions

- The STM32 firmware continues to emit newline-terminated ASCII frames using the strict `$TLM:<HHMMSS>:...` and `$ALM:<HHMMSS>:...` format, where `<HHMMSS>` is always a 6-digit numeric timestamp (e.g., `143055`).
- On initial serial connection when unlocked, the existing behavior of automatically sending `$CMD:LED:1`, syncing RTC, and sending the initial servo angle remains compatible; if the HMI is already in the locked state on reconnect, automatic startup `$CMD:SRV` is suppressed, and if the controller reports `$ALM:...:SYS:LOCKED` at connect time, the HMI transitions into the locked state cleanly.
- Servo angle values in `SRV:<angle>` are strictly integer degrees within `0` to `180`; values outside `0..180` or non-integers are rejected by the parser as invalid and ignored for state/angle updates.
- "Sync PC Time (GMT+7)" converts the current UTC clock to a fixed `+07:00` offset (`UTC+7`) rather than relying on the host OS local timezone setting, guaranteeing GMT+7 output even on machines configured in UTC or other timezones.
- On receiving `$ALM:...:SYS:BOOT`, the controller has restarted in an unlocked idle state at `0` degrees, so any prior HMI software lockout is cleared alongside displaying the boot notification.
