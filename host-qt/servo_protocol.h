#pragma once

#include <array>

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QStringList>

namespace ServoProtocol {

enum class FrameKind {
    None,
    Telemetry,
    Alarm
};

enum class MessageType {
    Invalid,
    AlarmButtonPressed,
    AlarmSystemLocked,
    AlarmSystemBoot,
    AlarmRtcError,
    TelemetrySystemUnlocked,
    TelemetryRtcOk,
    TelemetryServoAngle,
    TelemetryLedState
};

/**
 * @brief Parsed representation of a single incoming STM32 ASCII protocol line.
 */
struct ProtocolMessage {
    MessageType type{MessageType::Invalid};
    FrameKind   frameKind{FrameKind::None};
    QString     timestamp;
    int         servoAngle{-1};
    int         ledState{-1};
    QString     rawLine;

    bool isValid() const { return type != MessageType::Invalid; }
};

/**
 * @brief Stateful chunk-to-line serial framer with bounded overflow protection.
 */
class SerialLineFramer {
public:
    static constexpr int kMaxLineLength = 512;

    QStringList appendBytes(const QByteArray &chunk);
    void reset();

private:
    QByteArray m_buffer;
    bool       m_discardUntilNewline{false};
};

enum class SafetyLockState {
    Unlocked,
    Locked
};

enum class UnlockRequestState {
    Idle,
    Pending,
    TimedOut
};

enum class RtcSyncState {
    Idle,
    Pending,
    Synced,
    Error,
    TimedOut
};

/**
 * @brief Widget-free HMI safety lockout and RTC synchronization state model.
 */
class HmiSessionState {
public:
    void onConnected();
    void onDisconnected();

    bool onUnlockRequested();
    void onUnlockTimeout();

    bool onRtcSyncRequested();
    void onRtcTimeout();

    void applyMessage(const ProtocolMessage &msg);

    bool               isConnected() const { return m_connected; }
    bool               isLocked() const { return m_lockState == SafetyLockState::Locked; }
    SafetyLockState    lockState() const { return m_lockState; }
    QString            lockReason() const { return m_lockReason; }
    UnlockRequestState unlockRequestState() const { return m_unlockRequestState; }
    RtcSyncState       rtcSyncState() const { return m_rtcSyncState; }
    QString            rtcLastTimestamp() const { return m_rtcLastTimestamp; }
    int                currentAngle() const { return m_currentAngle; }

    bool canSendServoCommand() const {
        return m_connected && m_lockState == SafetyLockState::Unlocked;
    }
    bool shouldSendStartupServo() const {
        return m_connected && m_lockState == SafetyLockState::Unlocked;
    }
    bool canUnlock() const {
        return m_connected && m_lockState == SafetyLockState::Locked;
    }
    bool canSyncRtc() const {
        return m_connected;
    }

private:
    bool               m_connected{false};
    SafetyLockState    m_lockState{SafetyLockState::Unlocked};
    QString            m_lockReason;
    UnlockRequestState m_unlockRequestState{UnlockRequestState::Idle};
    RtcSyncState       m_rtcSyncState{RtcSyncState::Idle};
    QString            m_rtcLastTimestamp;
    int                m_currentAngle{90};
};

ProtocolMessage parseProtocolLine(const QString &rawLine);
QString         formatGmt7RtcCommand(const QDateTime &utcNow = QDateTime::currentDateTimeUtc());
QString         formatUnlockCommand();
QString         formatServoCommand(int angle);
QString         formatLedCommand(bool on);
bool            isServoMotionCommand(const QString &cmd);

enum class BringUpStep {
    UartLedEcho = 0,
    ServoAck = 1,
    EstopTrigger = 2,
    RtcHealth = 3,
    None = 4
};

enum class BringUpStatus {
    Idle,
    Waiting,
    Pass,
    Fail
};

struct BringUpStepResult {
    BringUpStatus status{BringUpStatus::Idle};
    qint64        rttMs{-1};
    QString       detail;
};

/**
 * @brief Widget-free 4-step hardware bring-up diagnostic state tracker.
 */
class HardwareBringUpTracker {
public:
    static constexpr int kStepCount = 4;

    HardwareBringUpTracker();

    void reset();
    BringUpStep startSequence();
    void stopSequence(const QString &reason = QString());

    bool onProtocolMessage(const ProtocolMessage &msg, qint64 rttMs);
    void onActiveStepTimeout(const QString &reason);

    bool              isRunning() const { return m_running; }
    BringUpStep       activeStep() const { return m_activeStep; }
    BringUpStepResult stepResult(BringUpStep step) const;

private:
    void advanceToNextStep();

    std::array<BringUpStepResult, kStepCount> m_results{};
    bool                                      m_running{false};
    BringUpStep                               m_activeStep{BringUpStep::None};
};

} // namespace ServoProtocol
