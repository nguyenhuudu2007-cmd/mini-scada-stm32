#include "servo_protocol.h"

namespace ServoProtocol {

namespace {

bool isSixAsciiDigits(const QString &token) {
    if (token.size() != 6) {
        return false;
    }
    for (const QChar ch : token) {
        if (ch < QLatin1Char('0') || ch > QLatin1Char('9')) {
            return false;
        }
    }
    return true;
}

bool parseStrictServoAngle(const QString &token, int &outAngle) {
    if (token.isEmpty()) {
        return false;
    }
    for (const QChar ch : token) {
        if (ch < QLatin1Char('0') || ch > QLatin1Char('9')) {
            return false;
        }
    }
    bool ok = false;
    const int value = token.toInt(&ok, 10);
    if (!ok || value < 0 || value > 180) {
        return false;
    }
    outAngle = value;
    return true;
}

} // namespace

QStringList SerialLineFramer::appendBytes(const QByteArray &chunk) {
    QStringList lines;
    for (const char ch : chunk) {
        if (ch == '\n') {
            if (m_discardUntilNewline) {
                m_discardUntilNewline = false;
                m_buffer.clear();
            } else {
                const QString line = QString::fromUtf8(m_buffer).trimmed();
                m_buffer.clear();
                if (!line.isEmpty()) {
                    lines.append(line);
                }
            }
            continue;
        }

        if (m_discardUntilNewline) {
            continue;
        }

        if (m_buffer.size() >= kMaxLineLength) {
            m_discardUntilNewline = true;
            m_buffer.clear();
            continue;
        }

        m_buffer.append(ch);
    }
    return lines;
}

void SerialLineFramer::reset() {
    m_buffer.clear();
    m_discardUntilNewline = false;
}

void HmiSessionState::onConnected() {
    m_connected = true;
    m_unlockRequestState = UnlockRequestState::Idle;
    m_rtcSyncState = RtcSyncState::Idle;
}

void HmiSessionState::onDisconnected() {
    m_connected = false;
    m_unlockRequestState = UnlockRequestState::Idle;
    m_rtcSyncState = RtcSyncState::Idle;
}

bool HmiSessionState::onUnlockRequested() {
    if (!canUnlock()) {
        return false;
    }
    m_unlockRequestState = UnlockRequestState::Pending;
    return true;
}

void HmiSessionState::onUnlockTimeout() {
    if (m_lockState == SafetyLockState::Locked && m_unlockRequestState == UnlockRequestState::Pending) {
        m_unlockRequestState = UnlockRequestState::TimedOut;
    }
}

bool HmiSessionState::onRtcSyncRequested() {
    if (!canSyncRtc()) {
        return false;
    }
    m_rtcSyncState = RtcSyncState::Pending;
    return true;
}

void HmiSessionState::onRtcTimeout() {
    if (m_rtcSyncState == RtcSyncState::Pending) {
        m_rtcSyncState = RtcSyncState::TimedOut;
    }
}

void HmiSessionState::applyMessage(const ProtocolMessage &msg) {
    switch (msg.type) {
    case MessageType::Invalid:
        return;
    case MessageType::AlarmButtonPressed:
        m_lockState = SafetyLockState::Locked;
        m_lockReason = QStringLiteral("BTN:PRESSED");
        m_unlockRequestState = UnlockRequestState::Idle;
        m_currentAngle = 0;
        return;
    case MessageType::AlarmSystemLocked:
        m_lockState = SafetyLockState::Locked;
        m_lockReason = QStringLiteral("SYS:LOCKED");
        m_unlockRequestState = UnlockRequestState::Idle;
        m_currentAngle = 0;
        return;
    case MessageType::AlarmSystemBoot:
        m_lockState = SafetyLockState::Unlocked;
        m_lockReason.clear();
        m_unlockRequestState = UnlockRequestState::Idle;
        m_currentAngle = 0;
        return;
    case MessageType::TelemetrySystemUnlocked:
        m_lockState = SafetyLockState::Unlocked;
        m_lockReason.clear();
        m_unlockRequestState = UnlockRequestState::Idle;
        return;
    case MessageType::TelemetryRtcOk:
        m_rtcSyncState = RtcSyncState::Synced;
        m_rtcLastTimestamp = msg.timestamp;
        return;
    case MessageType::AlarmRtcError:
        m_rtcSyncState = RtcSyncState::Error;
        m_rtcLastTimestamp = msg.timestamp;
        return;
    case MessageType::TelemetryServoAngle:
        m_currentAngle = msg.servoAngle;
        return;
    case MessageType::TelemetryLedState:
        return;
    }
}

ProtocolMessage parseProtocolLine(const QString &rawLine) {
    ProtocolMessage msg;
    const QString trimmed = rawLine.trimmed();
    msg.rawLine = trimmed;

    if (trimmed.isEmpty()) {
        return msg;
    }

    const QStringList parts = trimmed.split(QLatin1Char(':'));
    if (parts.size() != 4) {
        return msg;
    }

    const QString &prefix = parts[0];
    const QString &timestamp = parts[1];
    const QString &subsystem = parts[2];
    const QString &detail = parts[3];

    if (!isSixAsciiDigits(timestamp)) {
        return msg;
    }

    if (prefix == QLatin1String("$ALM")) {
        if (subsystem == QLatin1String("BTN") && detail == QLatin1String("PRESSED")) {
            msg.type = MessageType::AlarmButtonPressed;
            msg.frameKind = FrameKind::Alarm;
            msg.timestamp = timestamp;
        } else if (subsystem == QLatin1String("SYS") && detail == QLatin1String("LOCKED")) {
            msg.type = MessageType::AlarmSystemLocked;
            msg.frameKind = FrameKind::Alarm;
            msg.timestamp = timestamp;
        } else if (subsystem == QLatin1String("SYS") && detail == QLatin1String("BOOT")) {
            msg.type = MessageType::AlarmSystemBoot;
            msg.frameKind = FrameKind::Alarm;
            msg.timestamp = timestamp;
        } else if (subsystem == QLatin1String("RTC") && detail == QLatin1String("ERR")) {
            msg.type = MessageType::AlarmRtcError;
            msg.frameKind = FrameKind::Alarm;
            msg.timestamp = timestamp;
        }
        return msg;
    }

    if (prefix == QLatin1String("$TLM")) {
        if (subsystem == QLatin1String("SYS") && detail == QLatin1String("UNLOCKED")) {
            msg.type = MessageType::TelemetrySystemUnlocked;
            msg.frameKind = FrameKind::Telemetry;
            msg.timestamp = timestamp;
        } else if (subsystem == QLatin1String("RTC") && detail == QLatin1String("OK")) {
            msg.type = MessageType::TelemetryRtcOk;
            msg.frameKind = FrameKind::Telemetry;
            msg.timestamp = timestamp;
        } else if (subsystem == QLatin1String("SRV")) {
            int angle = -1;
            if (parseStrictServoAngle(detail, angle)) {
                msg.type = MessageType::TelemetryServoAngle;
                msg.frameKind = FrameKind::Telemetry;
                msg.timestamp = timestamp;
                msg.servoAngle = angle;
            }
        } else if (subsystem == QLatin1String("LED")) {
            if (detail == QLatin1String("0") || detail == QLatin1String("1")) {
                msg.type = MessageType::TelemetryLedState;
                msg.frameKind = FrameKind::Telemetry;
                msg.timestamp = timestamp;
                msg.ledState = (detail == QLatin1String("1")) ? 1 : 0;
            }
        }
        return msg;
    }

    return msg;
}

QString formatGmt7RtcCommand(const QDateTime &utcNow) {
    constexpr int kGmt7OffsetSeconds = 7 * 3600;
    const QDateTime gmt7 = utcNow.toUTC().toOffsetFromUtc(kGmt7OffsetSeconds);
    return QStringLiteral("$CMD:RTC:%1\n").arg(gmt7.toString(QStringLiteral("HH:mm:ss")));
}

QString formatUnlockCommand() {
    return QStringLiteral("$CMD:SYS:UNLOCK\n");
}

QString formatServoCommand(int angle) {
    const int clamped = qBound(0, angle, 180);
    return QStringLiteral("$CMD:SRV:%1\n").arg(clamped);
}

QString formatLedCommand(bool on) {
    return QStringLiteral("$CMD:LED:%1\n").arg(on ? 1 : 0);
}

bool isServoMotionCommand(const QString &cmd) {
    QString normalized = cmd.trimmed();
    if (normalized.startsWith(QLatin1Char('$'))) {
        normalized.remove(0, 1);
    }
    return normalized.startsWith(QLatin1String("CMD:SRV:"), Qt::CaseInsensitive) ||
           normalized.compare(QLatin1String("CMD:SRV"), Qt::CaseInsensitive) == 0;
}

HardwareBringUpTracker::HardwareBringUpTracker() {
    reset();
}

void HardwareBringUpTracker::reset() {
    m_results.fill(BringUpStepResult{});
    m_running = false;
    m_activeStep = BringUpStep::None;
}

BringUpStep HardwareBringUpTracker::startSequence() {
    reset();
    m_running = true;
    m_activeStep = BringUpStep::UartLedEcho;
    m_results[0].status = BringUpStatus::Waiting;
    return m_activeStep;
}

void HardwareBringUpTracker::stopSequence(const QString &reason) {
    if (!m_running) {
        return;
    }
    if (m_activeStep != BringUpStep::None) {
        const int idx = static_cast<int>(m_activeStep);
        if (m_results[idx].status == BringUpStatus::Waiting) {
            m_results[idx].status = BringUpStatus::Fail;
            m_results[idx].detail = reason.isEmpty() ? QStringLiteral("Cancelled") : reason;
        }
    }
    m_running = false;
    m_activeStep = BringUpStep::None;
}

bool HardwareBringUpTracker::onProtocolMessage(const ProtocolMessage &msg, qint64 rttMs) {
    if (!m_running || m_activeStep == BringUpStep::None || !msg.isValid()) {
        return false;
    }

    const int idx = static_cast<int>(m_activeStep);
    BringUpStepResult &current = m_results[idx];

    switch (m_activeStep) {
    case BringUpStep::UartLedEcho:
        if (msg.type == MessageType::TelemetryLedState && msg.ledState == 1) {
            current.status = BringUpStatus::Pass;
            current.rttMs = rttMs;
            current.detail = QStringLiteral("LED:1 ACK");
            advanceToNextStep();
            return true;
        }
        return false;

    case BringUpStep::ServoAck:
        if (msg.type == MessageType::TelemetryServoAngle && msg.servoAngle == 90) {
            current.status = BringUpStatus::Pass;
            current.rttMs = rttMs;
            current.detail = QStringLiteral("SRV:90 ACK");
            advanceToNextStep();
            return true;
        }
        return false;

    case BringUpStep::EstopTrigger:
        if (msg.type == MessageType::AlarmButtonPressed || msg.type == MessageType::AlarmSystemLocked) {
            current.status = BringUpStatus::Pass;
            current.rttMs = rttMs;
            current.detail = QStringLiteral("PA0 E-Stop detected");
            advanceToNextStep();
            return true;
        }
        return false;

    case BringUpStep::RtcHealth:
        if (msg.type == MessageType::TelemetryRtcOk) {
            current.status = BringUpStatus::Pass;
            current.rttMs = rttMs;
            current.detail = msg.timestamp;
            advanceToNextStep();
            return true;
        }
        if (msg.type == MessageType::AlarmRtcError) {
            current.status = BringUpStatus::Fail;
            current.rttMs = rttMs;
            current.detail = QStringLiteral("DS1307 I2C Error");
            advanceToNextStep();
            return true;
        }
        return false;

    case BringUpStep::None:
        return false;
    }
    return false;
}

void HardwareBringUpTracker::onActiveStepTimeout(const QString &reason) {
    if (!m_running || m_activeStep == BringUpStep::None) {
        return;
    }
    const int idx = static_cast<int>(m_activeStep);
    m_results[idx].status = BringUpStatus::Fail;
    m_results[idx].detail = reason.isEmpty() ? QStringLiteral("Timeout") : reason;
    advanceToNextStep();
}

BringUpStepResult HardwareBringUpTracker::stepResult(BringUpStep step) const {
    const int idx = static_cast<int>(step);
    if (idx < 0 || idx >= kStepCount) {
        return BringUpStepResult{};
    }
    return m_results[idx];
}

void HardwareBringUpTracker::advanceToNextStep() {
    const int next = static_cast<int>(m_activeStep) + 1;
    if (next >= kStepCount) {
        m_running = false;
        m_activeStep = BringUpStep::None;
        return;
    }
    m_activeStep = static_cast<BringUpStep>(next);
    m_results[next].status = BringUpStatus::Waiting;
}

} // namespace ServoProtocol
