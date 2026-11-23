#include "hardware_test_page.h"

#include <QDateTime>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int kSelfTestAngles[] = {0, 90, 180, 0};
constexpr int kSelfTestStepCount = sizeof(kSelfTestAngles) / sizeof(kSelfTestAngles[0]);
constexpr int kSelfTestIntervalMs = 600;
} // namespace

HardwareTestPage::HardwareTestPage(QWidget *parent)
    : QWidget(parent)
{
    m_selfTestTimer.setInterval(kSelfTestIntervalMs);
    connect(&m_selfTestTimer, &QTimer::timeout, this, &HardwareTestPage::onSelfTestStep);

    m_ackWatchdogTimer.setSingleShot(true);
    connect(&m_ackWatchdogTimer, &QTimer::timeout, this, &HardwareTestPage::onAckWatchdogTimeout);

    setupUi();
    renderBringUpChecklist();
    updateControlStates();
}

void HardwareTestPage::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // 1. Bang kiem tra Bring-Up 4 buoc tu dong cham PASS / FAIL & do RTT
    auto *boxBringUp = new QGroupBox(tr("Hardware Bring-Up Checklist (PASS / FAIL)"), this);
    auto *bringUpLayout = new QVBoxLayout(boxBringUp);

    auto *bringUpTop = new QHBoxLayout();
    m_btnRunBringUp = new QPushButton(tr("Run Bring-Up Check (4 Steps)"), this);
    m_lblRtt = new QLabel(tr("Last ACK RTT: -- ms"), this);
    connect(m_btnRunBringUp, &QPushButton::clicked, this, &HardwareTestPage::onStartStopBringUp);

    bringUpTop->addWidget(m_btnRunBringUp);
    bringUpTop->addSpacing(12);
    bringUpTop->addWidget(m_lblRtt);
    bringUpTop->addStretch();

    auto *grid = new QGridLayout();
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(4);
    const QStringList stepTitles = {
        tr("1. UART + LED Echo ($CMD:LED:1)"),
        tr("2. Servo PWM ACK ($CMD:SRV:90)"),
        tr("3. E-Stop Physical Trigger (PA0)"),
        tr("4. DS1307 RTC Health ($CMD:RTC)")
    };
    for (int i = 0; i < ServoProtocol::HardwareBringUpTracker::kStepCount; ++i) {
        auto *lblTitle = new QLabel(stepTitles[i], this);
        m_stepLabels[i] = new QLabel(this);
        grid->addWidget(lblTitle, i / 2, (i % 2) * 2);
        grid->addWidget(m_stepLabels[i], i / 2, (i % 2) * 2 + 1);
    }

    bringUpLayout->addLayout(bringUpTop);
    bringUpLayout->addLayout(grid);
    mainLayout->addWidget(boxBringUp);

    // 2. Kiem tra LED PC13
    auto *boxLed = new QGroupBox(tr("LED PC13 Check"), this);
    auto *ledLayout = new QHBoxLayout(boxLed);

    m_btnLedOn = new QPushButton(tr("LED ON ($CMD:LED:1)"), this);
    m_btnLedOff = new QPushButton(tr("LED OFF ($CMD:LED:0)"), this);
    m_lblLedState = new QLabel(tr("LED State: Unknown"), this);

    connect(m_btnLedOn, &QPushButton::clicked, this, [this]() {
        startManualAckProbe();
        emit ledRequested(true);
    });
    connect(m_btnLedOff, &QPushButton::clicked, this, [this]() {
        startManualAckProbe();
        emit ledRequested(false);
    });

    ledLayout->addWidget(m_btnLedOn);
    ledLayout->addWidget(m_btnLedOff);
    ledLayout->addSpacing(12);
    ledLayout->addWidget(m_lblLedState);
    ledLayout->addStretch();
    mainLayout->addWidget(boxLed);

    // 3. Kiem tra nhanh buoc goc Servo & chuoi tu kiem tra 0 -> 90 -> 180 -> 0
    auto *boxServo = new QGroupBox(tr("Servo Step && Self-Test"), this);
    auto *servoLayout = new QVBoxLayout(boxServo);

    auto *stepRow = new QHBoxLayout();
    stepRow->addWidget(new QLabel(tr("Quick Angles:"), this));
    for (int angle : {0, 45, 90, 135, 180}) {
        auto *btn = new QPushButton(QString("%1 deg").arg(angle), this);
        connect(btn, &QPushButton::clicked, this, [this, angle]() {
            if (m_connected && !m_locked) {
                startManualAckProbe();
                emit angleRequested(angle);
            }
        });
        m_quickAngleButtons.append(btn);
        stepRow->addWidget(btn);
    }
    stepRow->addStretch();

    auto *seqRow = new QHBoxLayout();
    m_btnSelfTest = new QPushButton(tr("Run Self-Test (0->90->180->0)"), this);
    m_lblSelfTestStatus = new QLabel(tr("Status: Disconnected"), this);
    connect(m_btnSelfTest, &QPushButton::clicked, this, &HardwareTestPage::onStartStopSelfTest);

    seqRow->addWidget(m_btnSelfTest);
    seqRow->addSpacing(12);
    seqRow->addWidget(m_lblSelfTestStatus);
    seqRow->addStretch();

    servoLayout->addLayout(stepRow);
    servoLayout->addLayout(seqRow);
    mainLayout->addWidget(boxServo);

    // 4. Bang gui lenh ASCII tho & theo doi TX/RX truc tiep
    auto *boxRaw = new QGroupBox(tr("Raw ASCII Command Console"), this);
    auto *rawLayout = new QVBoxLayout(boxRaw);

    auto *cmdRow = new QHBoxLayout();
    m_editRawCmd = new QLineEdit(this);
    m_editRawCmd->setPlaceholderText(tr("Enter command, e.g. $CMD:LED:1 or $CMD:SRV:90"));
    m_btnSendRaw = new QPushButton(tr("Send"), this);
    m_btnClearConsole = new QPushButton(tr("Clear"), this);

    connect(m_btnSendRaw, &QPushButton::clicked, this, &HardwareTestPage::onSendRawClicked);
    connect(m_editRawCmd, &QLineEdit::returnPressed, this, &HardwareTestPage::onSendRawClicked);
    connect(m_btnClearConsole, &QPushButton::clicked, this, [this]() {
        if (m_console) {
            m_console->clear();
        }
    });

    cmdRow->addWidget(m_editRawCmd, 1);
    cmdRow->addWidget(m_btnSendRaw);
    cmdRow->addWidget(m_btnClearConsole);

    m_console = new QPlainTextEdit(this);
    m_console->setReadOnly(true);
    m_console->setMaximumBlockCount(kMaxConsoleLines);
    m_console->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    rawLayout->addLayout(cmdRow);
    rawLayout->addWidget(m_console, 1);
    mainLayout->addWidget(boxRaw, 1);
}

bool HardwareTestPage::isSelfTestRunning() const {
    return m_selfTestTimer.isActive();
}

bool HardwareTestPage::isBringUpRunning() const {
    return m_bringUpTracker.isRunning();
}

void HardwareTestPage::setConnected(bool connected) {
    m_connected = connected;
    if (!m_connected) {
        m_ackWatchdogTimer.stop();
        m_expectingManualAck = false;
        if (isSelfTestRunning()) {
            stopSelfTest(tr("Disconnected"));
        }
        if (isBringUpRunning()) {
            stopBringUp(tr("Disconnected"));
        }
        if (m_lblLedState) {
            m_lblLedState->setText(tr("LED State: Unknown"));
            m_lblLedState->setStyleSheet(QString());
        }
        if (m_lblRtt) {
            m_lblRtt->setText(tr("Last ACK RTT: -- ms"));
            m_lblRtt->setStyleSheet(QString());
        }
    }
    updateControlStates();
}

void HardwareTestPage::setLocked(bool locked) {
    m_locked = locked;
    if (m_locked && isSelfTestRunning()) {
        stopSelfTest(tr("Stopped by STM32 E-Stop"));
    }
    // Neu dang cho buoc ServoAck ma bi khoa E-Stop dot ngot -> danh dau FAIL buoc Servo
    if (m_locked && m_bringUpTracker.isRunning() &&
        m_bringUpTracker.activeStep() == ServoProtocol::BringUpStep::ServoAck) {
        m_ackWatchdogTimer.stop();
        m_bringUpTracker.onActiveStepTimeout(tr("Safety locked before Servo ACK"));
        renderBringUpChecklist();
        triggerActiveBringUpStep();
    }
    updateControlStates();
}

void HardwareTestPage::setLedState(bool on) {
    if (!m_lblLedState) {
        return;
    }
    m_lblLedState->setText(on ? tr("LED State: ON (PC13)") : tr("LED State: OFF (PC13)"));
    m_lblLedState->setStyleSheet(on ? "font-weight: bold; color: #15803D;"
                                    : "color: #4B5563;");
}

void HardwareTestPage::onProtocolMessage(const ServoProtocol::ProtocolMessage &msg) {
    if (!msg.isValid()) {
        return;
    }

    const qint64 rttMs = m_rttTimer.isValid() ? m_rttTimer.elapsed() : -1;

    if (m_expectingManualAck &&
        (msg.type == ServoProtocol::MessageType::TelemetryLedState ||
         msg.type == ServoProtocol::MessageType::TelemetryServoAngle ||
         msg.type == ServoProtocol::MessageType::TelemetryRtcOk ||
         msg.type == ServoProtocol::MessageType::AlarmRtcError)) {
        m_ackWatchdogTimer.stop();
        m_expectingManualAck = false;
        updateRttLabel(rttMs);
    }

    if (m_bringUpTracker.isRunning() && m_bringUpTracker.onProtocolMessage(msg, rttMs)) {
        m_ackWatchdogTimer.stop();
        updateRttLabel(rttMs);
        renderBringUpChecklist();

        if (m_bringUpTracker.isRunning()) {
            triggerActiveBringUpStep();
        } else {
            int passed = 0;
            for (int i = 0; i < ServoProtocol::HardwareBringUpTracker::kStepCount; ++i) {
                const auto step = static_cast<ServoProtocol::BringUpStep>(i);
                if (m_bringUpTracker.stepResult(step).status == ServoProtocol::BringUpStatus::Pass) {
                    ++passed;
                }
            }
            const QString summary = tr("Bring-Up Check finished: %1/%2 PASS")
                                        .arg(passed)
                                        .arg(ServoProtocol::HardwareBringUpTracker::kStepCount);
            appendConsoleLine(QStringLiteral("INFO"), summary);
            updateControlStates();
            emit bringUpStateChanged(false, summary);
        }
    }
}

void HardwareTestPage::stopSelfTest(const QString &reason) {
    const bool wasRunning = isSelfTestRunning();
    m_selfTestTimer.stop();
    m_stepIndex = 0;
    updateControlStates();

    if (wasRunning) {
        if (!reason.isEmpty() && m_lblSelfTestStatus) {
            m_lblSelfTestStatus->setText(tr("Status: %1").arg(reason));
        }
        emit selfTestStateChanged(false, reason.isEmpty() ? tr("Stopped") : reason);
    }
}

void HardwareTestPage::stopBringUp(const QString &reason) {
    const bool wasRunning = m_bringUpTracker.isRunning();
    m_ackWatchdogTimer.stop();
    m_bringUpTracker.stopSequence(reason);
    renderBringUpChecklist();
    updateControlStates();

    if (wasRunning) {
        emit bringUpStateChanged(false, reason.isEmpty() ? tr("Stopped") : reason);
    }
}

void HardwareTestPage::appendConsoleLine(const QString &tag, const QString &text) {
    if (!m_console) {
        return;
    }
    const QString ts = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    m_console->appendPlainText(QString("[%1] [%2] %3").arg(ts, tag, text));
}

void HardwareTestPage::onStartStopBringUp() {
    if (isBringUpRunning()) {
        stopBringUp(tr("Stopped by user"));
        return;
    }
    if (!m_connected || m_locked) {
        return;
    }
    if (isSelfTestRunning()) {
        stopSelfTest(tr("Switched to Bring-Up Check"));
    }

    m_expectingManualAck = false;
    m_bringUpTracker.startSequence();
    renderBringUpChecklist();
    updateControlStates();
    emit bringUpStateChanged(true, tr("Started 4-step Bring-Up diagnostic"));
    triggerActiveBringUpStep();
}

void HardwareTestPage::triggerActiveBringUpStep() {
    if (!m_connected || !m_bringUpTracker.isRunning()) {
        return;
    }

    m_rttTimer.restart();

    switch (m_bringUpTracker.activeStep()) {
    case ServoProtocol::BringUpStep::UartLedEcho:
        m_ackWatchdogTimer.start(kAckTimeoutMs);
        emit ledRequested(true);
        break;

    case ServoProtocol::BringUpStep::ServoAck:
        if (m_locked) {
            m_bringUpTracker.onActiveStepTimeout(tr("Safety locked — Unlock first"));
            renderBringUpChecklist();
            triggerActiveBringUpStep();
            return;
        }
        m_ackWatchdogTimer.start(kAckTimeoutMs);
        emit angleRequested(90);
        break;

    case ServoProtocol::BringUpStep::EstopTrigger:
        appendConsoleLine(QStringLiteral("INFO"),
                          tr("Bring-Up Step 3: Please press the physical PA0 E-Stop button on STM32 (10s)..."));
        m_ackWatchdogTimer.start(kEstopWaitTimeoutMs);
        break;

    case ServoProtocol::BringUpStep::RtcHealth:
        m_ackWatchdogTimer.start(kAckTimeoutMs);
        emit rtcSyncRequested();
        break;

    case ServoProtocol::BringUpStep::None:
        break;
    }
}

void HardwareTestPage::onAckWatchdogTimeout() {
    if (m_bringUpTracker.isRunning()) {
        const QString msg = (m_bringUpTracker.activeStep() == ServoProtocol::BringUpStep::EstopTrigger)
                                ? tr("Timeout 10s (PA0 not pressed)")
                                : tr("No ACK after 500 ms");
        appendConsoleLine(QStringLiteral("WARN"),
                          tr("Bring-Up step failed: %1").arg(msg));
        m_bringUpTracker.onActiveStepTimeout(msg);
        renderBringUpChecklist();

        if (m_bringUpTracker.isRunning()) {
            triggerActiveBringUpStep();
        } else {
            updateControlStates();
            emit bringUpStateChanged(false, tr("Bring-Up Check finished with failures"));
        }
        return;
    }

    if (m_expectingManualAck) {
        m_expectingManualAck = false;
        if (m_lblRtt) {
            m_lblRtt->setText(tr("Last ACK RTT: No ACK (>500 ms — check PA9/PA10 wiring)"));
            m_lblRtt->setStyleSheet("font-weight: bold; color: #DC2626;");
        }
        appendConsoleLine(QStringLiteral("WARN"),
                          tr("No ACK after 500 ms — check TX/RX wiring or board power"));
    }
}

void HardwareTestPage::startManualAckProbe() {
    if (!m_connected || m_bringUpTracker.isRunning()) {
        return;
    }
    m_expectingManualAck = true;
    m_rttTimer.restart();
    m_ackWatchdogTimer.start(kAckTimeoutMs);
}

void HardwareTestPage::updateRttLabel(qint64 rttMs) {
    if (!m_lblRtt || rttMs < 0) {
        return;
    }
    m_lblRtt->setText(tr("Last ACK RTT: %1 ms").arg(rttMs));
    m_lblRtt->setStyleSheet("font-weight: bold; color: #15803D;");
}

void HardwareTestPage::renderBringUpChecklist() {
    for (int i = 0; i < ServoProtocol::HardwareBringUpTracker::kStepCount; ++i) {
        QLabel *lbl = m_stepLabels[i];
        if (!lbl) {
            continue;
        }
        const auto res = m_bringUpTracker.stepResult(static_cast<ServoProtocol::BringUpStep>(i));
        switch (res.status) {
        case ServoProtocol::BringUpStatus::Idle:
            lbl->setText(tr("IDLE"));
            lbl->setStyleSheet("color: #6B7280;");
            break;
        case ServoProtocol::BringUpStatus::Waiting:
            lbl->setText(i == static_cast<int>(ServoProtocol::BringUpStep::EstopTrigger)
                             ? tr("WAITING (Press PA0...)")
                             : tr("WAITING..."));
            lbl->setStyleSheet("font-weight: bold; color: #1D4ED8;");
            break;
        case ServoProtocol::BringUpStatus::Pass:
            lbl->setText(res.rttMs >= 0
                             ? tr("PASS (%1 ms)").arg(res.rttMs)
                             : tr("PASS"));
            lbl->setStyleSheet("font-weight: bold; color: #15803D;");
            break;
        case ServoProtocol::BringUpStatus::Fail:
            lbl->setText(res.detail.isEmpty()
                             ? tr("FAIL")
                             : tr("FAIL (%1)").arg(res.detail));
            lbl->setStyleSheet("font-weight: bold; color: #DC2626;");
            break;
        }
    }
}

void HardwareTestPage::onStartStopSelfTest() {
    if (isSelfTestRunning()) {
        stopSelfTest(tr("Stopped by user"));
        return;
    }
    if (!m_connected || m_locked || isBringUpRunning()) {
        return;
    }

    m_stepIndex = 0;
    m_selfTestTimer.start();
    updateControlStates();
    emit selfTestStateChanged(true, tr("Self-Test sequence started (0->90->180->0)"));
    onSelfTestStep();
}

void HardwareTestPage::onSelfTestStep() {
    if (!m_connected || m_locked) {
        stopSelfTest(m_locked ? tr("Stopped by STM32 E-Stop") : tr("Disconnected"));
        return;
    }

    if (m_stepIndex >= kSelfTestStepCount) {
        stopSelfTest(tr("Self-Test completed"));
        return;
    }

    const int angle = kSelfTestAngles[m_stepIndex++];
    if (m_lblSelfTestStatus) {
        m_lblSelfTestStatus->setText(
            tr("Status: Running step %1/%2 (%3 deg)")
                .arg(m_stepIndex)
                .arg(kSelfTestStepCount)
                .arg(angle));
        m_lblSelfTestStatus->setStyleSheet("font-weight: bold; color: #1D4ED8;");
    }
    startManualAckProbe();
    emit angleRequested(angle);
}

void HardwareTestPage::onSendRawClicked() {
    if (!m_connected || !m_editRawCmd) {
        return;
    }
    const QString raw = m_editRawCmd->text().trimmed();
    if (raw.isEmpty()) {
        return;
    }
    if (!ServoProtocol::isServoMotionCommand(raw) || !m_locked) {
        startManualAckProbe();
    }
    emit rawCommandRequested(raw);
    m_editRawCmd->clear();
}

void HardwareTestPage::updateControlStates() {
    const bool selfRunning = isSelfTestRunning();
    const bool bringUpRunning = isBringUpRunning();
    const bool busy = selfRunning || bringUpRunning;
    const bool canServo = m_connected && !m_locked;

    if (m_btnRunBringUp) {
        m_btnRunBringUp->setText(bringUpRunning ? tr("Stop Bring-Up")
                                                : tr("Run Bring-Up Check (4 Steps)"));
        m_btnRunBringUp->setEnabled((canServo && !selfRunning) || bringUpRunning);
    }
    if (m_btnLedOn) {
        m_btnLedOn->setEnabled(m_connected && !bringUpRunning);
    }
    if (m_btnLedOff) {
        m_btnLedOff->setEnabled(m_connected && !bringUpRunning);
    }
    if (m_editRawCmd) {
        m_editRawCmd->setEnabled(m_connected);
    }
    if (m_btnSendRaw) {
        m_btnSendRaw->setEnabled(m_connected);
    }

    for (QPushButton *btn : m_quickAngleButtons) {
        btn->setEnabled(canServo && !busy);
    }
    if (m_btnSelfTest) {
        m_btnSelfTest->setText(selfRunning ? tr("Stop Self-Test")
                                           : tr("Run Self-Test (0->90->180->0)"));
        m_btnSelfTest->setEnabled((canServo && !bringUpRunning) || selfRunning);
    }

    if (m_lblSelfTestStatus && !selfRunning) {
        if (!m_connected) {
            m_lblSelfTestStatus->setText(tr("Status: Disconnected"));
            m_lblSelfTestStatus->setStyleSheet(QString());
        } else if (m_locked) {
            m_lblSelfTestStatus->setText(tr("Status: Safety Locked (Servo test disabled)"));
            m_lblSelfTestStatus->setStyleSheet("font-weight: bold; color: #DC2626;");
        } else {
            m_lblSelfTestStatus->setText(tr("Status: Ready"));
            m_lblSelfTestStatus->setStyleSheet("color: #15803D;");
        }
    }
}
