#pragma once

#include <QWidget>

class QSpinBox;
class QPushButton;
class QProgressBar;
class QLabel;
class QTimer;

/**
 * @brief Trang Auto Sweep: quét góc Servo tự động theo chu kỳ không chặn Event Loop.
 *
 * Nguyên lý học tập:
 * - Không dùng sleep() / while() trong hàm giao diện vì sẽ làm đơ toàn bộ cửa sổ Qt.
 * - Dùng QTimer phát tín hiệu timeout() định kỳ để MainWindow vẫn xử lý song song
 *   dữ liệu Serial trả về (như tín hiệu E-Stop từ chân PA0 của STM32).
 */
class SweepPage : public QWidget {
    Q_OBJECT

public:
    explicit SweepPage(QWidget *parent = nullptr);
    ~SweepPage() override = default;

    bool isRunning() const;

public slots:
    void setConnected(bool connected);
    void setLocked(bool locked);
    void startSweep();
    void stopSweep(const QString &reason = QString());

signals:
    void angleRequested(int angle);
    void sweepStateChanged(bool running, const QString &reason);

private slots:
    void onSweepTick();
    void onToggleClicked();

private:
    void setupUi();
    void updateUiState(const QString &statusText, const QString &styleSheet = QString());

    QSpinBox     *m_spinMin{nullptr};
    QSpinBox     *m_spinMax{nullptr};
    QSpinBox     *m_spinStep{nullptr};
    QSpinBox     *m_spinIntervalMs{nullptr};
    QPushButton  *m_btnToggle{nullptr};
    QProgressBar *m_progressBar{nullptr};
    QLabel       *m_lblStatus{nullptr};

    QTimer *m_timer{nullptr};
    bool    m_connected{false};
    bool    m_locked{false};
    int     m_currentAngle{0};
    int     m_direction{1};
};
