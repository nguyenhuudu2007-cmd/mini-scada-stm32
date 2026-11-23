#pragma once

#include <QWidget>
#include <QVector>

class QPlainTextEdit;
class QCheckBox;
class QPushButton;
class QLabel;

/**
 * @brief Trang Log sự kiện & cảnh báo (Event & Alarm Logging).
 *
 * Nguyên lý học tập:
 * - Phân cấp mức log: Info, Tx, Rx, Warn, Alarm (tương tự syslog / Clash logs).
 * - Ghi sự kiện quan trọng (kết nối, Sync RTC, E-Stop PA0, Reset) ra file trên ổ cứng
 *   bằng QFile (Audit Trail) để truy vết sự cố sau khi tắt app.
 */
class LogPage : public QWidget {
    Q_OBJECT

public:
    enum class Level {
        Info,
        Tx,
        Rx,
        Warn,
        Alarm
    };

    explicit LogPage(QWidget *parent = nullptr);
    ~LogPage() override = default;

    QString logFilePath() const;

public slots:
    void appendLog(Level level, const QString &message);
    void clearView();

private slots:
    void rebuildView();
    void openLogFolder();

private:
    struct Entry {
        QString timestamp;
        Level   level;
        QString message;
    };

    void setupUi();
    void renderEntry(const Entry &entry);
    void persistEntry(const Entry &entry);
    static QString levelTag(Level level);
    static QColor  levelColor(Level level);

    QPlainTextEdit *m_textEdit{nullptr};
    QCheckBox      *m_chkShowTraffic{nullptr};
    QCheckBox      *m_chkSaveTraffic{nullptr};
    QPushButton    *m_btnClear{nullptr};
    QPushButton    *m_btnOpenFolder{nullptr};
    QLabel         *m_lblFilePath{nullptr};

    QVector<Entry> m_entries;
    QString        m_logFilePath;
    static constexpr int kMaxEntries = 500;
};
