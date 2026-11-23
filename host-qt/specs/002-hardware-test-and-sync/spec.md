# Feature Specification: Hardware Test Mode, Serial Disconnect Recovery & STM32 Protocol Alignment

**Feature Branch**: `feat/speckit-trial`
**Created**: 2026-11-23
**Status**: Active

## User Scenarios & Testing

### User Story 1 - LED Telemetry Parsing & Protocol Alignment (Priority: P1)
As a developer/operator, I want `ServoProtocol` to parse `$TLM:<HHMMSS>:LED:<0|1>`, format `$CMD:LED:<0|1>\n`, and identify raw `$CMD:SRV` motion commands safely, while aligning STM32 firmware (`stm32_SCADA_w1/Core/Src/main.c`) so E-Stop emits `$ALM:<HHMMSS>:BTN:PRESSED`, stays locked until `$CMD:SYS:UNLOCK`, and emits `$ALM:<HHMMSS>:SYS:BOOT` at startup.

### User Story 2 - Hardware Test Mode UI (Priority: P1)
As an operator, I want a dedicated `Hardware Test` tab replacing the placeholder in `MainWindow` to toggle STM32 `PC13` LED, trigger quick Servo test angles (`0`, `45`, `90`, `135`, `180`) and a non-blocking `0 -> 90 -> 180 -> 0` self-test sequence, and send raw `$CMD:...` lines with live TX/RX inspection, while respecting E-Stop lockout (`SC-002`: zero `$CMD:SRV` sent while locked) and stopping `Auto Sweep` when entering `Hardware Test`.

### User Story 3 - Unexpected USB Disconnect Recovery & Log Folder Access (Priority: P2)
As an operator, when the USB-RS232 cable disconnects unexpectedly (`QSerialPort::ResourceError` or link loss), the HMI must cleanly close the port without blocking `waitForBytesWritten()`, stop any active sweep or self-test, and reset the UI to `Disconnected`; additionally, `LogPage` should provide a compact log filename label and an `Open Folder` button.

### User Story 4 - 4-Step Bring-Up Diagnostic Checklist & RTT Latency (Priority: P2)
As an operator/developer, I want `HardwareTestPage` to provide a 4-step hardware bring-up checklist (`UART + LED Echo`, `Servo PWM ACK`, `E-Stop PA0 Trigger`, `DS1307 RTC Health`) that automatically grades `PASS / FAIL / WAITING` from real STM32 `$TLM`/`$ALM` responses and displays round-trip `TX -> RX` latency (`RTT ms`) with a `500 ms` missing-ACK warning.

## Functional Requirements
- **FR-001**: `ServoProtocol` MUST parse `$TLM:<HHMMSS>:LED:<0|1>` as `MessageType::TelemetryLedState` (`ledState` `0` or `1`) and reject malformed/wrong-prefix LED lines as `Invalid`.
- **FR-002**: `ServoProtocol` MUST provide `formatLedCommand(bool on)` and `isServoMotionCommand(const QString &cmd)` inside the widget-free `Qt::Core` library with unit tests.
- **FR-003**: `HardwareTestPage` MUST provide LED `PC13` controls, quick Servo angle buttons (`0/45/90/135/180`), a non-blocking `QTimer`-based self-test sequence (`0 -> 90 -> 180 -> 0`), and a raw ASCII command console with bounded TX/RX output.
- **FR-004**: Switching to `Hardware Test` or `Manual Slider` MUST stop `Auto Sweep`; switching away from `Hardware Test` MUST stop any active self-test sequence.
- **FR-005**: While safety-locked (`isLocked()`), all Servo motion buttons/sequences in `HardwareTestPage` MUST be disabled and raw `$CMD:SRV` commands MUST be blocked with a warning.
- **FR-006**: Fatal `QSerialPort::errorOccurred` events while open MUST trigger non-blocking disconnect teardown and reset UI to `Disconnected`.
- **FR-007**: `LogPage` MUST show a concise filename label (full path in tooltip) and an `Open Folder` action.
- **FR-008**: STM32 firmware `stm32_SCADA_w1/Core/Src/main.c` MUST emit `$ALM:...:BTN:PRESSED`, keep `is_stopped = 1` until `$CMD:SYS:UNLOCK`, and emit `$ALM:...:SYS:BOOT` after initialization.
- **FR-009**: `ServoProtocol` MUST provide a widget-free `HardwareBringUpTracker` tracking the 4 diagnostic steps (`UartLedEcho`, `ServoAck`, `EstopTrigger`, `RtcHealth`) with `Idle`, `Waiting`, `Pass`, and `Fail` states plus measured `rttMs`.
- **FR-010**: `HardwareTestPage` MUST display the 4-step Bring-Up Checklist, a `"Run Bring-Up Check"` action, live `RTT (ms)` indicator, and a `500 ms` no-ACK wiring warning on automated probes (`10 s` interactive wait for physical `PA0` E-Stop press).
