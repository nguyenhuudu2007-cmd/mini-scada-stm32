#pragma once

#include <QMainWindow>
#include <QTimer>

#include "servo_protocol.h"

class QLabel;
class QGroupBox;
class QPushButton;
class QComboBox;
class QStackedWidget;
class QButtonGroup;
class QSerialPort;
class SliderPage;
class SweepPage;
class HardwareTestPage;
class LogPage;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    enum class PageIndex {
        Slider = 0,
        Sweep = 1,
        HardwareTest = 2,
        Log = 3
    };

    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void switchMode(int index);
    void onServoAngleUpdated(int angle);
    void onSweepAngleRequested(int angle);
    void onLedRequested(bool on);
    void onRawCommandRequested(const QString &command);
    void onThrottleTimeout();
    void toggleConnection();
    void refreshSerialPorts();
    void syncRtcNow();
    void onRtcTimeout();
    void requestUnlock();
    void onUnlockTimeout();
    void onSerialReadyRead();

private:
    void setupUi();
    void createTopPersistentBar(class QVBoxLayout *mainLayout);
    void createSafetyBanner(class QVBoxLayout *mainLayout);
    void createModeSelectorBar(class QVBoxLayout *mainLayout);
    void createStackedPages(class QVBoxLayout *mainLayout);
    void sendCommandToSTM32(const QString &cmd);
    void updateCurrentAngleLabel(int angle, const QString &stateTag = QString());
    void syncSafetyUi();
    void syncRtcUi();
    void handleIncomingLine(const QString &line);
    void teardownActiveSession(const QString &logMessage, bool sendLedOff);
    void handleFatalSerialError(const QString &errorText);

    // Thành phần Thanh Giám Sát Cố Định (Top Bar)
    QComboBox   *m_comboPort{nullptr};
    QPushButton *m_btnRefresh{nullptr};
    QPushButton *m_btnConnect{nullptr};
    QPushButton *m_btnSyncRtc{nullptr};
    QLabel      *m_lblRtcStatus{nullptr};
    QLabel      *m_lblConnectionStatus{nullptr};
    QLabel      *m_lblCurrentAngle{nullptr};

    // Banner cảnh báo E-Stop & Unlock
    QGroupBox   *m_safetyBanner{nullptr};
    QLabel      *m_lblSafetyBanner{nullptr};
    QPushButton *m_btnUnlock{nullptr};

    // Nhóm nút Toggle chọn Mode
    QButtonGroup *m_modeGroup{nullptr};

    // Vùng lật trang động
    QStackedWidget *m_stackedWidget{nullptr};

    // Các trang cụ thể
    SliderPage       *m_sliderPage{nullptr};
    SweepPage        *m_sweepPage{nullptr};
    HardwareTestPage *m_hardwareTestPage{nullptr};
    LogPage          *m_logPage{nullptr};

    // Giao tiếp Serial Port & Trạng thái giao thức tách khỏi Widget
    QSerialPort                    *m_serialPort{nullptr};
    ServoProtocol::SerialLineFramer m_framer;
    ServoProtocol::HmiSessionState  m_sessionState;

    // Bộ tiết lưu Throttling cho thao tác kéo Slider (tối đa ~25 lệnh/giây = 40ms)
    QTimer m_throttleTimer;
    QTimer m_unlockTimeoutTimer;
    QTimer m_rtcTimeoutTimer;
    int    m_pendingAngle{-1};
    bool   m_tearingDown{false};
};
