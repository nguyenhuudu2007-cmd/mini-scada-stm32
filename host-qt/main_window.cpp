#include "main_window.h"
#include "slider_page.h"
#include "sweep_page.h"
#include "hardware_test_page.h"
#include "log_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QStackedWidget>
#include <QButtonGroup>
#include <QGroupBox>
#include <QDebug>
#include <QDateTime>
#include <QFileInfo>
#include <QSerialPort>
#include <QSerialPortInfo>

namespace {
constexpr int kResponseTimeoutMs = 2000;
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_serialPort = new QSerialPort(this);
    connect(m_serialPort, &QSerialPort::readyRead, this, &MainWindow::onSerialReadyRead);
    connect(m_serialPort, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError err) {
        if (err == QSerialPort::NoError) {
            return;
        }
        const QString errStr = m_serialPort->errorString();
        const bool isFatalLinkLoss =
            (err == QSerialPort::ResourceError ||
             err == QSerialPort::DeviceNotFoundError ||
             err == QSerialPort::PermissionError ||
             err == QSerialPort::OpenError);
        if (isFatalLinkLoss && m_serialPort->isOpen()) {
            handleFatalSerialError(errStr);
        } else if (m_logPage) {
            m_logPage->appendLog(LogPage::Level::Alarm, QString("Serial error: %1").arg(errStr));
        }
    });

    m_throttleTimer.setSingleShot(true);
    connect(&m_throttleTimer, &QTimer::timeout, this, &MainWindow::onThrottleTimeout);

    m_unlockTimeoutTimer.setSingleShot(true);
    connect(&m_unlockTimeoutTimer, &QTimer::timeout, this, &MainWindow::onUnlockTimeout);

    m_rtcTimeoutTimer.setSingleShot(true);
    connect(&m_rtcTimeoutTimer, &QTimer::timeout, this, &MainWindow::onRtcTimeout);

    setupUi();
    resize(860, 500);
    setWindowTitle("STM32 Mini SCADA - Servo Control Dashboard");

    refreshSerialPorts();
    if (m_logPage) {
        m_logPage->appendLog(LogPage::Level::Info,
                             QString("Application started. Persistent log: %1").arg(m_logPage->logFilePath()));
    }
}

MainWindow::~MainWindow() {
    if (m_sweepPage && m_sweepPage->isRunning()) {
        m_sweepPage->stopSweep("Application closing");
    }
    if (m_hardwareTestPage) {
        if (m_hardwareTestPage->isSelfTestRunning()) {
            m_hardwareTestPage->stopSelfTest("Application closing");
        }
        if (m_hardwareTestPage->isBringUpRunning()) {
            m_hardwareTestPage->stopBringUp("Application closing");
        }
    }
    if (m_serialPort && m_serialPort->isOpen()) {
        if (m_serialPort->error() == QSerialPort::NoError) {
            sendCommandToSTM32(ServoProtocol::formatLedCommand(false));
            m_serialPort->flush();
        }
        m_serialPort->close();
    }
}

void MainWindow::setupUi() {
    auto *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // 1. Box Kết Nối (Top Box)
    createTopPersistentBar(mainLayout);

    // 2. Banner Cảnh Báo An Toàn E-Stop (ẩn khi bình thường)
    createSafetyBanner(mainLayout);

    // 3. Box Chọn Chế Độ (Mode Box)
    createModeSelectorBar(mainLayout);

    // 4. Vùng Nội Dung Các Trang
    createStackedPages(mainLayout);

    syncSafetyUi();
    syncRtcUi();
}

void MainWindow::createTopPersistentBar(QVBoxLayout *mainLayout) {
    auto *boxConn = new QGroupBox(tr("Serial Connection"), this);
    auto *boxLayout = new QVBoxLayout(boxConn);
    boxLayout->setSpacing(6);

    auto *connRow = new QHBoxLayout();
    auto *lblPort = new QLabel(tr("Port:"), this);
    m_comboPort = new QComboBox(this);
    m_comboPort->setEditable(true); // Cho phép gõ /dev/scada_rs232 hoặc /tmp/ttyQt khi test ảo
    m_comboPort->setMinimumWidth(160);

    m_btnRefresh = new QPushButton(tr("Refresh"), this);
    connect(m_btnRefresh, &QPushButton::clicked, this, &MainWindow::refreshSerialPorts);

    m_btnConnect = new QPushButton(tr("Connect"), this);
    connect(m_btnConnect, &QPushButton::clicked, this, &MainWindow::toggleConnection);

    m_lblConnectionStatus = new QLabel(tr("Status: Disconnected"), this);
    m_lblCurrentAngle = new QLabel(this);
    updateCurrentAngleLabel(90, "IDLE");

    connRow->addWidget(lblPort);
    connRow->addWidget(m_comboPort);
    connRow->addWidget(m_btnRefresh);
    connRow->addWidget(m_btnConnect);
    connRow->addWidget(m_lblConnectionStatus);
    connRow->addStretch();
    connRow->addWidget(m_lblCurrentAngle);

    auto *rtcRow = new QHBoxLayout();
    m_btnSyncRtc = new QPushButton(tr("Sync PC Time (GMT+7)"), this);
    m_btnSyncRtc->setEnabled(false);
    connect(m_btnSyncRtc, &QPushButton::clicked, this, &MainWindow::syncRtcNow);

    m_lblRtcStatus = new QLabel(tr("RTC: Idle"), this);

    rtcRow->addWidget(m_btnSyncRtc);
    rtcRow->addSpacing(8);
    rtcRow->addWidget(m_lblRtcStatus);
    rtcRow->addStretch();

    boxLayout->addLayout(connRow);
    boxLayout->addLayout(rtcRow);

    mainLayout->addWidget(boxConn);
}

void MainWindow::createSafetyBanner(QVBoxLayout *mainLayout) {
    m_safetyBanner = new QGroupBox(tr("Safety Status"), this);

    auto *bannerLayout = new QHBoxLayout(m_safetyBanner);
    bannerLayout->setSpacing(8);

    m_lblSafetyBanner = new QLabel(this);
    m_lblSafetyBanner->setWordWrap(true);
    m_lblSafetyBanner->setStyleSheet("color: #DC2626; font-weight: bold;");

    m_btnUnlock = new QPushButton(tr("Unlock"), this);
    connect(m_btnUnlock, &QPushButton::clicked, this, &MainWindow::requestUnlock);

    bannerLayout->addWidget(m_lblSafetyBanner, 1);
    bannerLayout->addWidget(m_btnUnlock);

    m_safetyBanner->setVisible(false);
    mainLayout->addWidget(m_safetyBanner);
}

void MainWindow::refreshSerialPorts() {
    QString prevText = m_comboPort->currentText();
    m_comboPort->clear();

    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        QString portStr = info.systemLocation();
        m_comboPort->addItem(portStr, portStr);
    }

    // Thêm symlink cố định của udev nếu tồn tại trên máy
    if (QFileInfo::exists("/dev/scada_rs232") && m_comboPort->findText("/dev/scada_rs232") == -1) {
        m_comboPort->addItem("/dev/scada_rs232", "/dev/scada_rs232");
    }

    if (m_comboPort->count() == 0) {
        m_comboPort->addItem("/dev/ttyUSB0", "/dev/ttyUSB0");
    }

    if (!prevText.isEmpty() && m_comboPort->findText(prevText) != -1) {
        m_comboPort->setCurrentText(prevText);
    }
}

void MainWindow::toggleConnection() {
    if (!m_serialPort->isOpen()) {
        QString portName = m_comboPort->currentText().trimmed();
        m_serialPort->setPortName(portName);
        m_serialPort->setBaudRate(QSerialPort::Baud115200);
        m_serialPort->setDataBits(QSerialPort::Data8);
        m_serialPort->setParity(QSerialPort::NoParity);
        m_serialPort->setStopBits(QSerialPort::OneStop);
        m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

        if (m_serialPort->open(QIODevice::ReadWrite)) {
            m_framer.reset();
            m_pendingAngle = -1;
            m_throttleTimer.stop();
            m_sessionState.onConnected();

            m_comboPort->setEnabled(false);
            m_btnRefresh->setEnabled(false);
            m_btnConnect->setText(tr("Disconnect"));
            m_btnSyncRtc->setEnabled(true);
            m_lblConnectionStatus->setText(tr("Status: Connected (%1)").arg(portName));

            if (m_sweepPage) {
                m_sweepPage->setConnected(true);
            }
            if (m_hardwareTestPage) {
                m_hardwareTestPage->setConnected(true);
            }
            syncSafetyUi();
            syncRtcUi();

            if (m_logPage) {
                m_logPage->appendLog(LogPage::Level::Info,
                                     QString("Connected to %1 @ 115200 8N1").arg(portName));
                if (m_sessionState.isLocked()) {
                    m_logPage->appendLog(LogPage::Level::Warn,
                                         "Reconnected while in E-Stop safety lock — startup servo command suppressed");
                }
            }

            // Giãn cách các lệnh khởi tạo 120ms để không ghi đè rx_buf[32] trên STM32
            // khi MCU đang bận ghi DS1307/LCD qua I2C.
            sendCommandToSTM32(ServoProtocol::formatLedCommand(true));

            QTimer::singleShot(120, this, [this]() {
                if (m_serialPort && m_serialPort->isOpen()) {
                    syncRtcNow();
                }
            });

            if (m_sessionState.shouldSendStartupServo()) {
                QTimer::singleShot(260, this, [this]() {
                    if (m_serialPort && m_serialPort->isOpen() && m_sessionState.shouldSendStartupServo()) {
                        int initAngle = m_sliderPage ? m_sliderPage->angle() : 90;
                        sendCommandToSTM32(ServoProtocol::formatServoCommand(initAngle));
                    }
                });
            }

            qDebug() << "[SERIAL] Connected to" << portName;
        } else {
            m_lblConnectionStatus->setText(tr("Error: %1").arg(m_serialPort->errorString()));
            if (m_logPage) {
                m_logPage->appendLog(LogPage::Level::Alarm,
                                     QString("Failed to open %1: %2").arg(portName, m_serialPort->errorString()));
            }
            qWarning() << "[SERIAL] Failed to open:" << portName << m_serialPort->errorString();
        }
    } else {
        teardownActiveSession(QStringLiteral("Disconnected from serial port"), true);
        qDebug() << "[SERIAL] Disconnected";
    }
}

void MainWindow::syncRtcNow() {
    if (!m_serialPort || !m_serialPort->isOpen() || !m_sessionState.onRtcSyncRequested()) {
        return;
    }

    const QString cmd = ServoProtocol::formatGmt7RtcCommand();
    syncRtcUi();
    m_rtcTimeoutTimer.start(kResponseTimeoutMs);
    sendCommandToSTM32(cmd);

    if (m_logPage) {
        m_logPage->appendLog(LogPage::Level::Info,
                             QString("GMT+7 RTC sync sent: %1").arg(cmd.trimmed()));
    }
}

void MainWindow::onRtcTimeout() {
    m_sessionState.onRtcTimeout();
    syncRtcUi();

    if (m_logPage) {
        m_logPage->appendLog(LogPage::Level::Warn,
                             "RTC sync acknowledgement timed out after 2s — click Sync PC Time (GMT+7) to retry");
    }
}

void MainWindow::requestUnlock() {
    if (!m_serialPort || !m_serialPort->isOpen() || !m_sessionState.onUnlockRequested()) {
        return;
    }

    syncSafetyUi();
    m_unlockTimeoutTimer.start(kResponseTimeoutMs);
    sendCommandToSTM32(ServoProtocol::formatUnlockCommand());

    if (m_logPage) {
        m_logPage->appendLog(LogPage::Level::Info, "Unlock requested ($CMD:SYS:UNLOCK)");
    }
}

void MainWindow::onUnlockTimeout() {
    m_sessionState.onUnlockTimeout();
    syncSafetyUi();

    if (m_sessionState.isLocked() && m_logPage) {
        m_logPage->appendLog(LogPage::Level::Warn,
                             "Unlock confirmation timed out after 2s — click Unlock to retry");
    }
}

void MainWindow::onSerialReadyRead() {
    const QStringList lines = m_framer.appendBytes(m_serialPort->readAll());
    for (const QString &line : lines) {
        handleIncomingLine(line);
    }
}

void MainWindow::handleIncomingLine(const QString &line) {
    qDebug() << "[RX]" << line;
    if (m_logPage) {
        m_logPage->appendLog(LogPage::Level::Rx, line);
    }
    if (m_hardwareTestPage) {
        m_hardwareTestPage->appendConsoleLine(QStringLiteral("RX"), line);
    }

    const ServoProtocol::ProtocolMessage msg = ServoProtocol::parseProtocolLine(line);
    if (!msg.isValid()) {
        return;
    }

    m_sessionState.applyMessage(msg);
    if (m_hardwareTestPage) {
        m_hardwareTestPage->onProtocolMessage(msg);
    }

    switch (msg.type) {
    case ServoProtocol::MessageType::Invalid:
        return;

    case ServoProtocol::MessageType::AlarmButtonPressed:
        m_throttleTimer.stop();
        m_pendingAngle = -1;
        m_unlockTimeoutTimer.stop();
        if (m_sweepPage && m_sweepPage->isRunning()) {
            m_sweepPage->stopSweep("Stopped by STM32 E-Stop (PA0)");
        }
        if (m_sliderPage) {
            m_sliderPage->syncAngleFromHardware(0);
        }
        updateCurrentAngleLabel(0, "E-STOP");
        syncSafetyUi();
        if (m_logPage) {
            m_logPage->appendLog(LogPage::Level::Alarm,
                                 QString("E-STOP triggered on STM32 PA0 (%1) -> Servo forced to 0 deg").arg(line));
        }
        return;

    case ServoProtocol::MessageType::AlarmSystemLocked:
        m_throttleTimer.stop();
        m_pendingAngle = -1;
        m_unlockTimeoutTimer.stop();
        if (m_sweepPage && m_sweepPage->isRunning()) {
            m_sweepPage->stopSweep("STM32 E-Stop Locked");
        }
        if (m_sliderPage) {
            m_sliderPage->syncAngleFromHardware(0);
        }
        updateCurrentAngleLabel(0, "LOCKED");
        syncSafetyUi();
        if (m_logPage) {
            m_logPage->appendLog(LogPage::Level::Warn,
                                 "STM32 safety lock active (SYS:LOCKED) — motion controls disabled");
        }
        return;

    case ServoProtocol::MessageType::AlarmSystemBoot:
        m_throttleTimer.stop();
        m_pendingAngle = -1;
        m_unlockTimeoutTimer.stop();
        if (m_sweepPage && m_sweepPage->isRunning()) {
            m_sweepPage->stopSweep("STM32 Reset detected");
        }
        if (m_sliderPage) {
            m_sliderPage->syncAngleFromHardware(0);
        }
        updateCurrentAngleLabel(0, "BOOT");
        syncSafetyUi();
        if (m_logPage) {
            m_logPage->appendLog(LogPage::Level::Warn,
                                 "STM32 hardware reset detected -> Servo reset to 0 deg");
        }
        return;

    case ServoProtocol::MessageType::TelemetrySystemUnlocked:
        m_unlockTimeoutTimer.stop();
        updateCurrentAngleLabel(m_sessionState.currentAngle(), "UNLOCKED");
        syncSafetyUi();
        if (m_logPage) {
            m_logPage->appendLog(LogPage::Level::Info, "STM32 unlocked — servo motion controls enabled");
        }
        return;

    case ServoProtocol::MessageType::TelemetryRtcOk:
        m_rtcTimeoutTimer.stop();
        syncRtcUi();
        if (m_logPage) {
            m_logPage->appendLog(LogPage::Level::Info,
                                 QString("STM32 DS1307 RTC synced successfully (%1)").arg(msg.timestamp));
        }
        return;

    case ServoProtocol::MessageType::AlarmRtcError:
        m_rtcTimeoutTimer.stop();
        syncRtcUi();
        if (m_logPage) {
            m_logPage->appendLog(LogPage::Level::Warn,
                                 QString("STM32 reported DS1307 I2C RTC error (%1)").arg(msg.timestamp));
        }
        return;

    case ServoProtocol::MessageType::TelemetryServoAngle:
        updateCurrentAngleLabel(msg.servoAngle,
                                m_sessionState.isLocked() ? QStringLiteral("LOCKED") : QString());
        if (m_sliderPage && (!m_sweepPage || !m_sweepPage->isRunning())) {
            m_sliderPage->syncAngleFromHardware(msg.servoAngle);
        }
        return;

    case ServoProtocol::MessageType::TelemetryLedState:
        if (m_hardwareTestPage) {
            m_hardwareTestPage->setLedState(msg.ledState == 1);
        }
        return;
    }
}

void MainWindow::syncSafetyUi() {
    const bool locked = m_sessionState.isLocked();
    if (m_safetyBanner) {
        m_safetyBanner->setVisible(locked);
    }
    if (m_btnUnlock) {
        m_btnUnlock->setEnabled(m_sessionState.canUnlock());
    }
    if (m_lblSafetyBanner && locked) {
        const QString reason = m_sessionState.lockReason().isEmpty()
                                   ? QStringLiteral("E-STOP")
                                   : m_sessionState.lockReason();
        QString msg = tr("SAFETY LOCKED (%1) — Servo motion controls are disabled.").arg(reason);
        switch (m_sessionState.unlockRequestState()) {
        case ServoProtocol::UnlockRequestState::Idle:
            if (!m_sessionState.isConnected()) {
                msg += tr(" Connect serial port to unlock.");
            }
            break;
        case ServoProtocol::UnlockRequestState::Pending:
            msg += tr(" Waiting for controller unlock...");
            break;
        case ServoProtocol::UnlockRequestState::TimedOut:
            msg += tr(" No unlock response yet — click Unlock to retry.");
            break;
        }
        m_lblSafetyBanner->setText(msg);
    }
    if (m_sliderPage) {
        m_sliderPage->setLocked(locked);
    }
    if (m_sweepPage) {
        m_sweepPage->setLocked(locked);
    }
    if (m_hardwareTestPage) {
        m_hardwareTestPage->setLocked(locked);
    }
}

void MainWindow::syncRtcUi() {
    if (m_btnSyncRtc) {
        m_btnSyncRtc->setEnabled(m_sessionState.canSyncRtc());
    }
    if (!m_lblRtcStatus) {
        return;
    }

    switch (m_sessionState.rtcSyncState()) {
    case ServoProtocol::RtcSyncState::Idle:
        m_lblRtcStatus->setText(tr("RTC: Idle"));
        m_lblRtcStatus->setStyleSheet("color: #4B5563;");
        break;
    case ServoProtocol::RtcSyncState::Pending:
        m_lblRtcStatus->setText(tr("RTC: Syncing (GMT+7)..."));
        m_lblRtcStatus->setStyleSheet("font-weight: bold; color: #1D4ED8;");
        break;
    case ServoProtocol::RtcSyncState::Synced:
        m_lblRtcStatus->setText(tr("RTC: Synced (%1)").arg(m_sessionState.rtcLastTimestamp()));
        m_lblRtcStatus->setStyleSheet("font-weight: bold; color: #15803D;");
        break;
    case ServoProtocol::RtcSyncState::Error:
        m_lblRtcStatus->setText(tr("RTC: Error (%1) — retry").arg(m_sessionState.rtcLastTimestamp()));
        m_lblRtcStatus->setStyleSheet("font-weight: bold; color: #DC2626;");
        break;
    case ServoProtocol::RtcSyncState::TimedOut:
        m_lblRtcStatus->setText(tr("RTC: No reply after 2s — click to retry"));
        m_lblRtcStatus->setStyleSheet("font-weight: bold; color: #B45309;");
        break;
    }
}

void MainWindow::updateCurrentAngleLabel(int angle, const QString &stateTag) {
    if (stateTag.isEmpty()) {
        m_lblCurrentAngle->setText(tr("Current Angle: %1 deg").arg(angle));
        m_lblCurrentAngle->setStyleSheet("font-weight: bold; color: #15803D;");
    } else if (stateTag == "E-STOP" || stateTag == "LOCKED") {
        m_lblCurrentAngle->setText(tr("Current Angle: %1 deg [%2]").arg(angle).arg(stateTag));
        m_lblCurrentAngle->setStyleSheet("font-weight: bold; color: #DC2626;");
    } else if (stateTag == "BOOT") {
        m_lblCurrentAngle->setText(tr("Current Angle: %1 deg [BOOT]").arg(angle));
        m_lblCurrentAngle->setStyleSheet("font-weight: bold; color: #B45309;");
    } else {
        m_lblCurrentAngle->setText(tr("Current Angle: %1 deg [%2]").arg(angle).arg(stateTag));
        m_lblCurrentAngle->setStyleSheet(QString());
    }
}

void MainWindow::createModeSelectorBar(QVBoxLayout *mainLayout) {
    auto *boxMode = new QGroupBox(tr("Operation Mode"), this);
    auto *modeBarLayout = new QHBoxLayout(boxMode);

    m_modeGroup = new QButtonGroup(this);
    m_modeGroup->setExclusive(true);

    auto *btnSlider = new QPushButton(tr("Manual Slider"), this);
    auto *btnSweep  = new QPushButton(tr("Auto Sweep"), this);
    auto *btnTest   = new QPushButton(tr("Hardware Test"), this);
    auto *btnLog    = new QPushButton(tr("Event Log"), this);

    const QList<QPushButton *> buttons = {btnSlider, btnSweep, btnTest, btnLog};
    for (int i = 0; i < buttons.size(); ++i) {
        QPushButton *btn = buttons[i];
        btn->setCheckable(true);
        m_modeGroup->addButton(btn, i);
        modeBarLayout->addWidget(btn);

        connect(btn, &QPushButton::clicked, this, [this, i]() {
            switchMode(i);
        });
    }

    btnSlider->setChecked(true);
    mainLayout->addWidget(boxMode);
}

void MainWindow::createStackedPages(QVBoxLayout *mainLayout) {
    m_stackedWidget = new QStackedWidget(this);

    // Trang 0: Manual Slider
    m_sliderPage = new SliderPage(this);
    connect(m_sliderPage, &SliderPage::angleChanged, this, &MainWindow::onServoAngleUpdated);

    // Trang 1: Auto Sweep
    m_sweepPage = new SweepPage(this);
    connect(m_sweepPage, &SweepPage::angleRequested, this, &MainWindow::onSweepAngleRequested);
    connect(m_sweepPage, &SweepPage::sweepStateChanged, this, [this](bool running, const QString &reason) {
        if (m_logPage) {
            m_logPage->appendLog(LogPage::Level::Info,
                                 QString("Auto Sweep %1: %2").arg(running ? "STARTED" : "STOPPED", reason));
        }
    });

    // Trang 2: Hardware Test
    m_hardwareTestPage = new HardwareTestPage(this);
    connect(m_hardwareTestPage, &HardwareTestPage::angleRequested, this, &MainWindow::onSweepAngleRequested);
    connect(m_hardwareTestPage, &HardwareTestPage::ledRequested, this, &MainWindow::onLedRequested);
    connect(m_hardwareTestPage, &HardwareTestPage::rtcSyncRequested, this, &MainWindow::syncRtcNow);
    connect(m_hardwareTestPage, &HardwareTestPage::rawCommandRequested, this, &MainWindow::onRawCommandRequested);
    connect(m_hardwareTestPage, &HardwareTestPage::selfTestStateChanged, this, [this](bool running, const QString &reason) {
        if (m_logPage) {
            m_logPage->appendLog(LogPage::Level::Info,
                                 QString("Hardware Self-Test %1: %2").arg(running ? "STARTED" : "STOPPED", reason));
        }
    });
    connect(m_hardwareTestPage, &HardwareTestPage::bringUpStateChanged, this, [this](bool running, const QString &summary) {
        if (m_logPage) {
            m_logPage->appendLog(LogPage::Level::Info,
                                 QString("Hardware Bring-Up %1: %2").arg(running ? "STARTED" : "FINISHED", summary));
        }
    });

    // Trang 3: Event & Alarm Log
    m_logPage = new LogPage(this);

    m_stackedWidget->addWidget(m_sliderPage);
    m_stackedWidget->addWidget(m_sweepPage);
    m_stackedWidget->addWidget(m_hardwareTestPage);
    m_stackedWidget->addWidget(m_logPage);

    mainLayout->addWidget(m_stackedWidget, 1);
}

void MainWindow::switchMode(int index) {
    // Dung Auto Sweep khi chuyen sang Manual Slider hoac Hardware Test de tranh xung dot lenh
    if ((index == static_cast<int>(PageIndex::Slider) ||
         index == static_cast<int>(PageIndex::HardwareTest)) &&
        m_sweepPage && m_sweepPage->isRunning()) {
        m_sweepPage->stopSweep(index == static_cast<int>(PageIndex::Slider)
                                   ? tr("Switched to Manual Slider")
                                   : tr("Switched to Hardware Test"));
    }
    // Dung chuoi tu kiem tra / Bring-Up neu roi khoi tab Hardware Test
    if (index != static_cast<int>(PageIndex::HardwareTest) && m_hardwareTestPage) {
        if (m_hardwareTestPage->isSelfTestRunning()) {
            m_hardwareTestPage->stopSelfTest(tr("Switched tab"));
        }
        if (m_hardwareTestPage->isBringUpRunning()) {
            m_hardwareTestPage->stopBringUp(tr("Switched tab"));
        }
    }
    m_stackedWidget->setCurrentIndex(index);
}

void MainWindow::onServoAngleUpdated(int angle) {
    if (!m_serialPort || !m_serialPort->isOpen()) {
        if (!m_sessionState.isLocked()) {
            updateCurrentAngleLabel(angle, "OFFLINE");
        }
        return;
    }

    if (!m_sessionState.canSendServoCommand()) {
        return;
    }

    if (!m_throttleTimer.isActive()) {
        sendCommandToSTM32(ServoProtocol::formatServoCommand(angle));
        m_pendingAngle = -1;
        m_throttleTimer.start(40);
    } else {
        m_pendingAngle = angle;
    }
}

void MainWindow::onSweepAngleRequested(int angle) {
    if (!m_serialPort || !m_serialPort->isOpen() || !m_sessionState.canSendServoCommand()) {
        return;
    }
    // SweepPage đã tự giãn nhịp >= 45ms bằng QTimer nên gửi thẳng không cần dồn vào throttle
    sendCommandToSTM32(ServoProtocol::formatServoCommand(angle));
}

void MainWindow::onLedRequested(bool on) {
    if (!m_serialPort || !m_serialPort->isOpen()) {
        return;
    }
    sendCommandToSTM32(ServoProtocol::formatLedCommand(on));
}

void MainWindow::onRawCommandRequested(const QString &command) {
    if (!m_serialPort || !m_serialPort->isOpen()) {
        return;
    }
    QString line = command.trimmed();
    if (line.isEmpty()) {
        return;
    }
    if (ServoProtocol::isServoMotionCommand(line) && !m_sessionState.canSendServoCommand()) {
        const QString warnMsg = tr("Blocked raw servo command while safety locked: %1").arg(line);
        if (m_hardwareTestPage) {
            m_hardwareTestPage->appendConsoleLine(QStringLiteral("WARN"), warnMsg);
        }
        if (m_logPage) {
            m_logPage->appendLog(LogPage::Level::Warn, warnMsg);
        }
        return;
    }
    line.append(QLatin1Char('\n'));
    sendCommandToSTM32(line);
}

void MainWindow::teardownActiveSession(const QString &logMessage, bool sendLedOff) {
    if (m_tearingDown) {
        return;
    }
    m_tearingDown = true;

    m_unlockTimeoutTimer.stop();
    m_rtcTimeoutTimer.stop();
    m_throttleTimer.stop();
    m_pendingAngle = -1;
    m_framer.reset();

    if (m_sweepPage && m_sweepPage->isRunning()) {
        m_sweepPage->stopSweep(tr("Disconnected"));
    }
    if (m_sweepPage) {
        m_sweepPage->setConnected(false);
    }
    if (m_hardwareTestPage) {
        if (m_hardwareTestPage->isSelfTestRunning()) {
            m_hardwareTestPage->stopSelfTest(tr("Disconnected"));
        }
        if (m_hardwareTestPage->isBringUpRunning()) {
            m_hardwareTestPage->stopBringUp(tr("Disconnected"));
        }
        m_hardwareTestPage->setConnected(false);
    }

    if (m_serialPort && m_serialPort->isOpen()) {
        if (sendLedOff && m_serialPort->error() == QSerialPort::NoError) {
            sendCommandToSTM32(ServoProtocol::formatLedCommand(false));
            m_serialPort->flush();
        }
        m_serialPort->close();
    }

    m_sessionState.onDisconnected();
    syncSafetyUi();
    syncRtcUi();

    m_comboPort->setEnabled(true);
    m_btnRefresh->setEnabled(true);
    m_btnConnect->setText(tr("Connect"));
    m_btnSyncRtc->setEnabled(false);
    m_lblConnectionStatus->setText(tr("Status: Disconnected"));
    updateCurrentAngleLabel(m_sliderPage ? m_sliderPage->angle() : 0,
                            m_sessionState.isLocked() ? QStringLiteral("LOCKED") : QStringLiteral("IDLE"));

    if (m_logPage && !logMessage.isEmpty()) {
        m_logPage->appendLog(sendLedOff ? LogPage::Level::Info : LogPage::Level::Alarm, logMessage);
    }
    m_tearingDown = false;
}

void MainWindow::handleFatalSerialError(const QString &errorText) {
    teardownActiveSession(tr("Serial connection lost: %1").arg(errorText), false);
    refreshSerialPorts();
    qWarning() << "[SERIAL] Fatal disconnect:" << errorText;
}

void MainWindow::onThrottleTimeout() {
    if (m_pendingAngle != -1 && m_serialPort && m_serialPort->isOpen() && m_sessionState.canSendServoCommand()) {
        sendCommandToSTM32(ServoProtocol::formatServoCommand(m_pendingAngle));
        m_pendingAngle = -1;
        m_throttleTimer.start(40);
    }
}

void MainWindow::sendCommandToSTM32(const QString &cmd) {
    if (m_serialPort && m_serialPort->isOpen()) {
        m_serialPort->write(cmd.toUtf8());
        const QString clean = cmd.trimmed();
        qDebug() << "[TX]" << clean;
        if (m_logPage) {
            m_logPage->appendLog(LogPage::Level::Tx, clean);
        }
        if (m_hardwareTestPage) {
            m_hardwareTestPage->appendConsoleLine(QStringLiteral("TX"), clean);
        }
    }
}
