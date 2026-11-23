#include "sweep_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QPushButton>
#include <QProgressBar>
#include <QTimer>

SweepPage::SweepPage(QWidget *parent)
    : QWidget(parent)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &SweepPage::onSweepTick);

    setupUi();
    setConnected(false);
}

void SweepPage::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // 1. Khung cấu hình thông số quét góc
    auto *boxParams = new QGroupBox(tr("Sweep Parameters"), this);
    auto *paramLayout = new QHBoxLayout(boxParams);

    m_spinMin = new QSpinBox(this);
    m_spinMin->setRange(0, 180);
    m_spinMin->setValue(0);
    m_spinMin->setSuffix(" deg");

    m_spinMax = new QSpinBox(this);
    m_spinMax->setRange(0, 180);
    m_spinMax->setValue(180);
    m_spinMax->setSuffix(" deg");

    m_spinStep = new QSpinBox(this);
    m_spinStep->setRange(1, 45);
    m_spinStep->setValue(5);
    m_spinStep->setSuffix(" deg");

    // Firmware cần vài ms cập nhật I2C LCD nên giữ tối thiểu 45ms/nhịp để không nghẽn rx_buf
    m_spinIntervalMs = new QSpinBox(this);
    m_spinIntervalMs->setRange(45, 1000);
    m_spinIntervalMs->setSingleStep(10);
    m_spinIntervalMs->setValue(60);
    m_spinIntervalMs->setSuffix(" ms");

    paramLayout->addWidget(new QLabel(tr("Min:"), this));
    paramLayout->addWidget(m_spinMin);
    paramLayout->addSpacing(8);
    paramLayout->addWidget(new QLabel(tr("Max:"), this));
    paramLayout->addWidget(m_spinMax);
    paramLayout->addSpacing(8);
    paramLayout->addWidget(new QLabel(tr("Step:"), this));
    paramLayout->addWidget(m_spinStep);
    paramLayout->addSpacing(8);
    paramLayout->addWidget(new QLabel(tr("Interval:"), this));
    paramLayout->addWidget(m_spinIntervalMs);
    paramLayout->addStretch();

    mainLayout->addWidget(boxParams);

    // 2. Khung điều khiển và thanh tiến trình trực quan
    auto *boxRun = new QGroupBox(tr("Sweep Execution"), this);
    auto *runLayout = new QVBoxLayout(boxRun);

    auto *topRow = new QHBoxLayout();
    m_btnToggle = new QPushButton(tr("Start Sweep"), this);
    m_btnToggle->setMinimumWidth(130);
    connect(m_btnToggle, &QPushButton::clicked, this, &SweepPage::onToggleClicked);

    m_lblStatus = new QLabel(tr("Status: Disconnected"), this);

    topRow->addWidget(m_btnToggle);
    topRow->addSpacing(12);
    topRow->addWidget(m_lblStatus);
    topRow->addStretch();

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 180);
    m_progressBar->setValue(0);
    m_progressBar->setFormat(tr("Target: %v deg"));

    runLayout->addLayout(topRow);
    runLayout->addWidget(m_progressBar);
    mainLayout->addWidget(boxRun);
    mainLayout->addStretch();
}

bool SweepPage::isRunning() const {
    return m_timer && m_timer->isActive();
}

void SweepPage::setConnected(bool connected) {
    m_connected = connected;
    if (!m_connected) {
        if (isRunning()) {
            stopSweep(tr("Disconnected"));
        } else if (m_locked) {
            updateUiState(tr("Status: Safety Locked (Disconnected)"),
                          "color: #DC2626; font-weight: bold;");
        } else {
            updateUiState(tr("Status: Disconnected (Connect Serial first)"));
        }
    } else if (!isRunning()) {
        if (m_locked) {
            updateUiState(tr("Status: Safety Locked (E-Stop Active)"),
                          "color: #DC2626; font-weight: bold;");
        } else {
            updateUiState(tr("Status: Idle (Ready)"));
        }
    }
    m_btnToggle->setEnabled(m_connected && !m_locked);
}

void SweepPage::setLocked(bool locked) {
    m_locked = locked;
    if (m_locked) {
        if (isRunning()) {
            stopSweep(tr("Stopped by STM32 E-Stop"));
        } else {
            updateUiState(tr("Status: Safety Locked (E-Stop Active)"),
                          "color: #DC2626; font-weight: bold;");
        }
    } else if (!isRunning()) {
        updateUiState(m_connected ? tr("Status: Idle (Ready)")
                                  : tr("Status: Disconnected (Connect Serial first)"));
    }
}

void SweepPage::onToggleClicked() {
    if (isRunning()) {
        stopSweep(tr("Stopped by user"));
    } else {
        startSweep();
    }
}

void SweepPage::startSweep() {
    if (!m_connected || m_locked || isRunning()) {
        return;
    }

    int minAngle = qMin(m_spinMin->value(), m_spinMax->value());
    int maxAngle = qMax(m_spinMin->value(), m_spinMax->value());
    m_spinMin->setValue(minAngle);
    m_spinMax->setValue(maxAngle);

    m_currentAngle = minAngle;
    m_direction = 1;

    m_progressBar->setValue(m_currentAngle);
    m_timer->start(m_spinIntervalMs->value());

    updateUiState(tr("Status: Sweeping (%1..%2 deg)").arg(minAngle).arg(maxAngle),
                  "color: #15803D; font-weight: bold;");
    emit sweepStateChanged(true, tr("Started (%1-%2 deg, step %3, %4 ms)")
                                     .arg(minAngle)
                                     .arg(maxAngle)
                                     .arg(m_spinStep->value())
                                     .arg(m_spinIntervalMs->value()));
    emit angleRequested(m_currentAngle);
}

void SweepPage::stopSweep(const QString &reason) {
    bool wasRunning = isRunning();
    if (m_timer) {
        m_timer->stop();
    }

    QString statusText = reason.isEmpty()
                             ? tr("Status: Stopped")
                             : tr("Status: %1").arg(reason);
    QString style = reason.contains("E-Stop", Qt::CaseInsensitive)
                        ? "color: #DC2626; font-weight: bold;"
                        : QString();
    updateUiState(statusText, style);

    if (wasRunning) {
        emit sweepStateChanged(false, reason.isEmpty() ? tr("Stopped") : reason);
    }
}

void SweepPage::onSweepTick() {
    int minAngle = qMin(m_spinMin->value(), m_spinMax->value());
    int maxAngle = qMax(m_spinMin->value(), m_spinMax->value());
    int step = m_spinStep->value();

    if (minAngle == maxAngle) {
        m_currentAngle = minAngle;
    } else {
        m_currentAngle += m_direction * step;
        if (m_currentAngle >= maxAngle) {
            m_currentAngle = maxAngle;
            m_direction = -1;
        } else if (m_currentAngle <= minAngle) {
            m_currentAngle = minAngle;
            m_direction = 1;
        }
    }

    m_progressBar->setValue(m_currentAngle);
    emit angleRequested(m_currentAngle);
}

void SweepPage::updateUiState(const QString &statusText, const QString &styleSheet) {
    bool running = isRunning();
    m_btnToggle->setText(running ? tr("Stop Sweep") : tr("Start Sweep"));
    m_btnToggle->setEnabled(m_connected && !m_locked);
    m_spinMin->setEnabled(!running && !m_locked);
    m_spinMax->setEnabled(!running && !m_locked);
    m_spinStep->setEnabled(!running && !m_locked);
    m_spinIntervalMs->setEnabled(!running && !m_locked);
    m_lblStatus->setText(statusText);
    m_lblStatus->setStyleSheet(styleSheet);
}
