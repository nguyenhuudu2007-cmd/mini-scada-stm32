#pragma once

#include <array>

#include <QElapsedTimer>
#include <QList>
#include <QTimer>
#include <QWidget>

#include "servo_protocol.h"

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

/**
 * @brief Trang Hardware Test: kiem tra Bring-Up 4 buoc PASS/FAIL kem do tre RTT,
 *        LED PC13, buoc goc Servo nhanh, chuoi tu kiem tra (0 -> 90 -> 180 -> 0),
 *        va bang dieu khien gui lenh ASCII tho xuong STM32.
 */
class HardwareTestPage : public QWidget {
    Q_OBJECT

public:
    explicit HardwareTestPage(QWidget *parent = nullptr);
    ~HardwareTestPage() override = default;

    bool isSelfTestRunning() const;
    bool isBringUpRunning() const;

public slots:
    void setConnected(bool connected);
    void setLocked(bool locked);
    void setLedState(bool on);
    void onProtocolMessage(const ServoProtocol::ProtocolMessage &msg);
    void stopSelfTest(const QString &reason = QString());
    void stopBringUp(const QString &reason = QString());
    void appendConsoleLine(const QString &tag, const QString &text);

signals:
    void angleRequested(int angle);
    void ledRequested(bool on);
    void rtcSyncRequested();
    void rawCommandRequested(const QString &command);
    void selfTestStateChanged(bool running, const QString &reason);
    void bringUpStateChanged(bool running, const QString &summary);

private slots:
    void onStartStopBringUp();
    void onAckWatchdogTimeout();
    void onStartStopSelfTest();
    void onSelfTestStep();
    void onSendRawClicked();

private:
    void setupUi();
    void updateControlStates();
    void triggerActiveBringUpStep();
    void startManualAckProbe();
    void renderBringUpChecklist();
    void updateRttLabel(qint64 rttMs);

    QPushButton *m_btnRunBringUp{nullptr};
    QLabel      *m_lblRtt{nullptr};
    std::array<QLabel *, ServoProtocol::HardwareBringUpTracker::kStepCount> m_stepLabels{};

    QPushButton          *m_btnLedOn{nullptr};
    QPushButton          *m_btnLedOff{nullptr};
    QLabel               *m_lblLedState{nullptr};
    QList<QPushButton *>  m_quickAngleButtons;
    QPushButton          *m_btnSelfTest{nullptr};
    QLabel               *m_lblSelfTestStatus{nullptr};
    QLineEdit            *m_editRawCmd{nullptr};
    QPushButton          *m_btnSendRaw{nullptr};
    QPushButton          *m_btnClearConsole{nullptr};
    QPlainTextEdit       *m_console{nullptr};

    QTimer                                m_selfTestTimer;
    QTimer                                m_ackWatchdogTimer;
    QElapsedTimer                         m_rttTimer;
    ServoProtocol::HardwareBringUpTracker m_bringUpTracker;

    int  m_stepIndex{0};
    bool m_connected{false};
    bool m_locked{false};
    bool m_expectingManualAck{false};

    static constexpr int kMaxConsoleLines = 200;
    static constexpr int kAckTimeoutMs = 500;
    static constexpr int kEstopWaitTimeoutMs = 10000;
};
