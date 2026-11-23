#include <QObject>
#include <QTimeZone>
#include <QtTest>

#include "servo_protocol.h"

using namespace ServoProtocol;

class TestServoProtocol : public QObject {
    Q_OBJECT

private slots:
    void parseSystemAndEstopMessages() {
        {
            const ProtocolMessage msg = parseProtocolLine(QStringLiteral("$ALM:143055:BTN:PRESSED"));
            QVERIFY(msg.isValid());
            QCOMPARE(msg.frameKind, FrameKind::Alarm);
            QCOMPARE(msg.type, MessageType::AlarmButtonPressed);
            QCOMPARE(msg.timestamp, QStringLiteral("143055"));
            QCOMPARE(msg.servoAngle, -1);
        }
        {
            const ProtocolMessage msg = parseProtocolLine(QStringLiteral("$ALM:143056:SYS:LOCKED"));
            QVERIFY(msg.isValid());
            QCOMPARE(msg.frameKind, FrameKind::Alarm);
            QCOMPARE(msg.type, MessageType::AlarmSystemLocked);
            QCOMPARE(msg.timestamp, QStringLiteral("143056"));
        }
        {
            const ProtocolMessage msg = parseProtocolLine(QStringLiteral("$ALM:000001:SYS:BOOT"));
            QVERIFY(msg.isValid());
            QCOMPARE(msg.frameKind, FrameKind::Alarm);
            QCOMPARE(msg.type, MessageType::AlarmSystemBoot);
            QCOMPARE(msg.timestamp, QStringLiteral("000001"));
        }
        {
            const ProtocolMessage msg = parseProtocolLine(QStringLiteral("$TLM:143110:SYS:UNLOCKED"));
            QVERIFY(msg.isValid());
            QCOMPARE(msg.frameKind, FrameKind::Telemetry);
            QCOMPARE(msg.type, MessageType::TelemetrySystemUnlocked);
            QCOMPARE(msg.timestamp, QStringLiteral("143110"));
        }
    }

    void formatUnlockAndServoCommands() {
        QCOMPARE(formatUnlockCommand(), QStringLiteral("$CMD:SYS:UNLOCK\n"));
        QCOMPARE(formatServoCommand(0), QStringLiteral("$CMD:SRV:0\n"));
        QCOMPARE(formatServoCommand(90), QStringLiteral("$CMD:SRV:90\n"));
        QCOMPARE(formatServoCommand(180), QStringLiteral("$CMD:SRV:180\n"));
        QCOMPARE(formatServoCommand(-10), QStringLiteral("$CMD:SRV:0\n"));
        QCOMPARE(formatServoCommand(250), QStringLiteral("$CMD:SRV:180\n"));
    }

    void sessionStateEstopLockoutAndUnlockFlow() {
        HmiSessionState state;
        QVERIFY(!state.isConnected());
        QVERIFY(!state.isLocked());
        QVERIFY(!state.canSendServoCommand());
        QVERIFY(!state.canUnlock());

        state.onConnected();
        QVERIFY(state.isConnected());
        QVERIFY(state.canSendServoCommand());
        QVERIFY(state.shouldSendStartupServo());
        QVERIFY(!state.canUnlock());

        // Trigger E-Stop via BTN:PRESSED
        state.applyMessage(parseProtocolLine(QStringLiteral("$ALM:101500:BTN:PRESSED")));
        QVERIFY(state.isLocked());
        QCOMPARE(state.lockReason(), QStringLiteral("BTN:PRESSED"));
        QCOMPARE(state.currentAngle(), 0);
        QVERIFY(!state.canSendServoCommand());
        QVERIFY(!state.shouldSendStartupServo());
        QVERIFY(state.canUnlock());
        QCOMPARE(state.unlockRequestState(), UnlockRequestState::Idle);

        // Request unlock -> Pending, still locked
        QVERIFY(state.onUnlockRequested());
        QCOMPARE(state.unlockRequestState(), UnlockRequestState::Pending);
        QVERIFY(state.isLocked());
        QVERIFY(!state.canSendServoCommand());

        // Timeout without response -> TimedOut, still unlockable for retry
        state.onUnlockTimeout();
        QCOMPARE(state.unlockRequestState(), UnlockRequestState::TimedOut);
        QVERIFY(state.isLocked());
        QVERIFY(state.canUnlock());

        // Repeated SYS:LOCKED resets stale unlock request state and keeps locked
        state.onUnlockRequested();
        QCOMPARE(state.unlockRequestState(), UnlockRequestState::Pending);
        state.applyMessage(parseProtocolLine(QStringLiteral("$ALM:101502:SYS:LOCKED")));
        QVERIFY(state.isLocked());
        QCOMPARE(state.unlockRequestState(), UnlockRequestState::Idle);

        // Disconnect/Reconnect while locked preserves lock and suppresses startup SRV
        state.onDisconnected();
        QVERIFY(!state.isConnected());
        QVERIFY(state.isLocked());
        QVERIFY(!state.canUnlock());
        QVERIFY(!state.canSendServoCommand());

        state.onConnected();
        QVERIFY(state.isConnected());
        QVERIFY(state.isLocked());
        QVERIFY(!state.shouldSendStartupServo());
        QVERIFY(!state.canSendServoCommand());
        QVERIFY(state.canUnlock());

        // Confirm unlock via $TLM:...:SYS:UNLOCKED
        state.applyMessage(parseProtocolLine(QStringLiteral("$TLM:101510:SYS:UNLOCKED")));
        QVERIFY(!state.isLocked());
        QCOMPARE(state.unlockRequestState(), UnlockRequestState::Idle);
        QVERIFY(state.canSendServoCommand());
        QVERIFY(!state.canUnlock());

        // Lock again then recover via SYS:BOOT
        state.applyMessage(parseProtocolLine(QStringLiteral("$ALM:101520:SYS:LOCKED")));
        QVERIFY(state.isLocked());
        state.applyMessage(parseProtocolLine(QStringLiteral("$ALM:101525:SYS:BOOT")));
        QVERIFY(!state.isLocked());
        QCOMPARE(state.currentAngle(), 0);
        QVERIFY(state.canSendServoCommand());
    }

    void parseRtcMessages() {
        {
            const ProtocolMessage msg = parseProtocolLine(QStringLiteral("$TLM:091530:RTC:OK"));
            QVERIFY(msg.isValid());
            QCOMPARE(msg.frameKind, FrameKind::Telemetry);
            QCOMPARE(msg.type, MessageType::TelemetryRtcOk);
            QCOMPARE(msg.timestamp, QStringLiteral("091530"));
        }
        {
            const ProtocolMessage msg = parseProtocolLine(QStringLiteral("$ALM:091531:RTC:ERR"));
            QVERIFY(msg.isValid());
            QCOMPARE(msg.frameKind, FrameKind::Alarm);
            QCOMPARE(msg.type, MessageType::AlarmRtcError);
            QCOMPARE(msg.timestamp, QStringLiteral("091531"));
        }
    }

    void formatGmt7RtcCommandUsesUtcPlusSeven() {
        const QDateTime utcMorning(QDate(2026, 11, 22), QTime(0, 5, 9), QTimeZone::UTC);
        QCOMPARE(formatGmt7RtcCommand(utcMorning), QStringLiteral("$CMD:RTC:07:05:09\n"));

        const QDateTime utcEvening(QDate(2026, 11, 22), QTime(18, 30, 45), QTimeZone::UTC);
        QCOMPARE(formatGmt7RtcCommand(utcEvening), QStringLiteral("$CMD:RTC:01:30:45\n"));

        // Non-GMT+7 source offset (UTC-05:00 10:00:00 == 15:00:00 UTC == 22:00:00 GMT+7)
        const QDateTime minusFive(QDate(2026, 11, 22), QTime(10, 0, 0), QTimeZone::fromSecondsAheadOfUtc(-5 * 3600));
        QCOMPARE(formatGmt7RtcCommand(minusFive), QStringLiteral("$CMD:RTC:22:00:00\n"));
    }

    void sessionStateRtcSyncFlow() {
        HmiSessionState state;
        QVERIFY(!state.canSyncRtc());
        QVERIFY(!state.onRtcSyncRequested());
        QCOMPARE(state.rtcSyncState(), RtcSyncState::Idle);

        state.onConnected();
        QVERIFY(state.canSyncRtc());

        // Success flow
        QVERIFY(state.onRtcSyncRequested());
        QCOMPARE(state.rtcSyncState(), RtcSyncState::Pending);
        state.applyMessage(parseProtocolLine(QStringLiteral("$TLM:120001:RTC:OK")));
        QCOMPARE(state.rtcSyncState(), RtcSyncState::Synced);
        QCOMPARE(state.rtcLastTimestamp(), QStringLiteral("120001"));

        // Error flow (also permitted while locked)
        state.applyMessage(parseProtocolLine(QStringLiteral("$ALM:120002:SYS:LOCKED")));
        QVERIFY(state.isLocked());
        QVERIFY(state.canSyncRtc());
        QVERIFY(state.onRtcSyncRequested());
        state.applyMessage(parseProtocolLine(QStringLiteral("$ALM:120003:RTC:ERR")));
        QCOMPARE(state.rtcSyncState(), RtcSyncState::Error);
        QCOMPARE(state.rtcLastTimestamp(), QStringLiteral("120003"));

        // Timeout flow
        QVERIFY(state.onRtcSyncRequested());
        QCOMPARE(state.rtcSyncState(), RtcSyncState::Pending);
        state.onRtcTimeout();
        QCOMPARE(state.rtcSyncState(), RtcSyncState::TimedOut);

        state.onDisconnected();
        QCOMPARE(state.rtcSyncState(), RtcSyncState::Idle);
    }

    void parseServoTelemetryAndBoundaries() {
        for (int angle : {0, 90, 180}) {
            const QString line = QStringLiteral("$TLM:120000:SRV:%1").arg(angle);
            const ProtocolMessage msg = parseProtocolLine(line);
            QVERIFY(msg.isValid());
            QCOMPARE(msg.frameKind, FrameKind::Telemetry);
            QCOMPARE(msg.type, MessageType::TelemetryServoAngle);
            QCOMPARE(msg.timestamp, QStringLiteral("120000"));
            QCOMPARE(msg.servoAngle, angle);
            QCOMPARE(msg.rawLine, line);
        }

        const QStringList badAngles = {
            QStringLiteral("$TLM:120000:SRV:-1"),
            QStringLiteral("$TLM:120000:SRV:181"),
            QStringLiteral("$TLM:120000:SRV:abc"),
            QStringLiteral("$TLM:120000:SRV:+90"),
            QStringLiteral("$TLM:120000:SRV:90deg"),
            QStringLiteral("$TLM:120000:SRV:")
        };
        for (const QString &line : badAngles) {
            const ProtocolMessage msg = parseProtocolLine(line);
            QVERIFY2(!msg.isValid(), qPrintable(line));
            QCOMPARE(msg.type, MessageType::Invalid);
            QCOMPARE(msg.rawLine, line);
        }
    }

    void rejectMalformedAndMismatchedFrames() {
        const QStringList invalidLines = {
            QString(),
            QStringLiteral("   \t  "),
            QStringLiteral("$CMD:SRV:90"),
            QStringLiteral("TLM:120000:RTC:OK"),
            QStringLiteral("$ALM:120000:SYS:UNLOCKED"),
            QStringLiteral("$ALM:120000:RTC:OK"),
            QStringLiteral("$ALM:120000:SRV:90"),
            QStringLiteral("$TLM:120000:BTN:PRESSED"),
            QStringLiteral("$TLM:120000:SYS:LOCKED"),
            QStringLiteral("$TLM:120000:SYS:BOOT"),
            QStringLiteral("$TLM:120000:RTC:ERR"),
            QStringLiteral("$TLM:12345:RTC:OK"),
            QStringLiteral("$TLM:1234567:RTC:OK"),
            QStringLiteral("$TLM:12ab56:RTC:OK"),
            QStringLiteral("$TLM::RTC:OK"),
            QStringLiteral("$TLM:120000:RTC:OK:EXTRA"),
            QStringLiteral("$TLM:120000:RTC:OK:"),
            QStringLiteral("garbled noise BTN:PRESSED")
        };

        HmiSessionState state;
        state.onConnected();
        state.applyMessage(parseProtocolLine(QStringLiteral("$TLM:120000:SRV:42")));

        for (const QString &line : invalidLines) {
            const ProtocolMessage msg = parseProtocolLine(line);
            QVERIFY2(!msg.isValid(), qPrintable(line));
            QCOMPARE(msg.type, MessageType::Invalid);
            QCOMPARE(msg.frameKind, FrameKind::None);
            QCOMPARE(msg.rawLine, line.trimmed());

            // Must cause zero state transitions
            state.applyMessage(msg);
            QVERIFY(!state.isLocked());
            QCOMPARE(state.currentAngle(), 42);
            QCOMPARE(state.rtcSyncState(), RtcSyncState::Idle);
        }
    }

    void serialLineFramerChunksCrlfAndOverflow() {
        SerialLineFramer framer;

        // Split across multiple chunks with CRLF and empty lines
        QCOMPARE(framer.appendBytes(QByteArrayLiteral("\r\n  \n$TLM:12")), QStringList{});
        QCOMPARE(framer.appendBytes(QByteArrayLiteral("0000:SRV:")), QStringList{});
        QCOMPARE(framer.appendBytes(QByteArrayLiteral("45\r\n$ALM:120001:")),
                 QStringList{QStringLiteral("$TLM:120000:SRV:45")});
        QCOMPARE(framer.appendBytes(QByteArrayLiteral("SYS:LOCKED\n")),
                 QStringList{QStringLiteral("$ALM:120001:SYS:LOCKED")});

        // Exact 512-byte line passes
        const QByteArray exactMax(SerialLineFramer::kMaxLineLength, 'X');
        const QStringList exactOut = framer.appendBytes(exactMax + "\n");
        QCOMPARE(exactOut.size(), 1);
        QCOMPARE(exactOut.first().size(), SerialLineFramer::kMaxLineLength);

        // Overflow (>512 bytes before newline) is discarded and recovers at next newline
        const QByteArray overMax(SerialLineFramer::kMaxLineLength + 10, 'Y');
        QCOMPARE(framer.appendBytes(overMax), QStringList{});
        QCOMPARE(framer.appendBytes(QByteArrayLiteral("still_garbage\n$TLM:120005:RTC:OK\n")),
                 QStringList{QStringLiteral("$TLM:120005:RTC:OK")});

        // reset() discards stale partial buffer and overflow state
        QCOMPARE(framer.appendBytes(QByteArrayLiteral("$ALM:999999:BTN:")), QStringList{});
        framer.reset();
        QCOMPARE(framer.appendBytes(QByteArrayLiteral("PRESSED\n$TLM:120006:RTC:OK\n")),
                 (QStringList{QStringLiteral("PRESSED"), QStringLiteral("$TLM:120006:RTC:OK")}));
    }

    void parseLedTelemetryAndCommands() {
        {
            const ProtocolMessage msg = parseProtocolLine(QStringLiteral("$TLM:143501:LED:1"));
            QVERIFY(msg.isValid());
            QCOMPARE(msg.frameKind, FrameKind::Telemetry);
            QCOMPARE(msg.type, MessageType::TelemetryLedState);
            QCOMPARE(msg.timestamp, QStringLiteral("143501"));
            QCOMPARE(msg.ledState, 1);
            QCOMPARE(msg.servoAngle, -1);
        }
        {
            const ProtocolMessage msg = parseProtocolLine(QStringLiteral("$TLM:143502:LED:0"));
            QVERIFY(msg.isValid());
            QCOMPARE(msg.frameKind, FrameKind::Telemetry);
            QCOMPARE(msg.type, MessageType::TelemetryLedState);
            QCOMPARE(msg.ledState, 0);
        }

        const QStringList badLedLines = {
            QStringLiteral("$ALM:143502:LED:1"),
            QStringLiteral("$TLM:143502:LED:2"),
            QStringLiteral("$TLM:143502:LED:-1"),
            QStringLiteral("$TLM:143502:LED:ON"),
            QStringLiteral("$TLM:143502:LED:")
        };
        for (const QString &line : badLedLines) {
            const ProtocolMessage msg = parseProtocolLine(line);
            QVERIFY2(!msg.isValid(), qPrintable(line));
            QCOMPARE(msg.type, MessageType::Invalid);
        }

        QCOMPARE(formatLedCommand(true), QStringLiteral("$CMD:LED:1\n"));
        QCOMPARE(formatLedCommand(false), QStringLiteral("$CMD:LED:0\n"));

        QVERIFY(isServoMotionCommand(QStringLiteral("$CMD:SRV:90")));
        QVERIFY(isServoMotionCommand(QStringLiteral("  $CMD:SRV:0\n")));
        QVERIFY(isServoMotionCommand(QStringLiteral("CMD:SRV:45")));
        QVERIFY(!isServoMotionCommand(QStringLiteral("$CMD:LED:1")));
        QVERIFY(!isServoMotionCommand(QStringLiteral("$CMD:SYS:UNLOCK")));
        QVERIFY(!isServoMotionCommand(QStringLiteral("$CMD:RTC:12:00:00")));
    }

    void hardwareBringUpTrackerPassFailAndTimeout() {
        HardwareBringUpTracker tracker;
        QVERIFY(!tracker.isRunning());
        QCOMPARE(tracker.stepResult(BringUpStep::UartLedEcho).status, BringUpStatus::Idle);

        // Start full 4-step sequence -> begins at UartLedEcho
        QCOMPARE(tracker.startSequence(), BringUpStep::UartLedEcho);
        QVERIFY(tracker.isRunning());
        QCOMPARE(tracker.activeStep(), BringUpStep::UartLedEcho);
        QCOMPARE(tracker.stepResult(BringUpStep::UartLedEcho).status, BringUpStatus::Waiting);

        // Wrong message while waiting on UartLedEcho does not pass the step
        QVERIFY(!tracker.onProtocolMessage(parseProtocolLine(QStringLiteral("$TLM:120000:SRV:90")), 5));
        QCOMPARE(tracker.stepResult(BringUpStep::UartLedEcho).status, BringUpStatus::Waiting);

        // Matching LED:1 passes UartLedEcho with RTT and advances to ServoAck
        QVERIFY(tracker.onProtocolMessage(parseProtocolLine(QStringLiteral("$TLM:120001:LED:1")), 12));
        QCOMPARE(tracker.stepResult(BringUpStep::UartLedEcho).status, BringUpStatus::Pass);
        QCOMPARE(tracker.stepResult(BringUpStep::UartLedEcho).rttMs, 12);
        QCOMPARE(tracker.activeStep(), BringUpStep::ServoAck);
        QCOMPARE(tracker.stepResult(BringUpStep::ServoAck).status, BringUpStatus::Waiting);

        // Wrong servo angle does not pass ServoAck(90)
        QVERIFY(!tracker.onProtocolMessage(parseProtocolLine(QStringLiteral("$TLM:120002:SRV:45")), 9));
        QCOMPARE(tracker.stepResult(BringUpStep::ServoAck).status, BringUpStatus::Waiting);

        // Matching SRV:90 passes ServoAck and advances to EstopTrigger
        QVERIFY(tracker.onProtocolMessage(parseProtocolLine(QStringLiteral("$TLM:120002:SRV:90")), 18));
        QCOMPARE(tracker.stepResult(BringUpStep::ServoAck).status, BringUpStatus::Pass);
        QCOMPARE(tracker.stepResult(BringUpStep::ServoAck).rttMs, 18);
        QCOMPARE(tracker.activeStep(), BringUpStep::EstopTrigger);

        // Physical E-Stop BTN:PRESSED passes EstopTrigger and advances to RtcHealth
        QVERIFY(tracker.onProtocolMessage(parseProtocolLine(QStringLiteral("$ALM:120003:BTN:PRESSED")), 240));
        QCOMPARE(tracker.stepResult(BringUpStep::EstopTrigger).status, BringUpStatus::Pass);
        QCOMPARE(tracker.activeStep(), BringUpStep::RtcHealth);

        // RTC:ERR fails RtcHealth and finishes sequence
        QVERIFY(tracker.onProtocolMessage(parseProtocolLine(QStringLiteral("$ALM:120004:RTC:ERR")), 15));
        QCOMPARE(tracker.stepResult(BringUpStep::RtcHealth).status, BringUpStatus::Fail);
        QCOMPARE(tracker.stepResult(BringUpStep::RtcHealth).rttMs, 15);
        QVERIFY(!tracker.isRunning());

        // Restart and test timeout failure on step 1
        tracker.startSequence();
        tracker.onActiveStepTimeout(QStringLiteral("No ACK after 500 ms"));
        QCOMPARE(tracker.stepResult(BringUpStep::UartLedEcho).status, BringUpStatus::Fail);
        QCOMPARE(tracker.stepResult(BringUpStep::UartLedEcho).detail, QStringLiteral("No ACK after 500 ms"));
        QCOMPARE(tracker.activeStep(), BringUpStep::ServoAck);
    }
};

QTEST_GUILESS_MAIN(TestServoProtocol)

#include "test_servo_protocol.moc"
