#include "log_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileInfo>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QFontDatabase>
#include <QUrl>

LogPage::LogPage(QWidget *parent)
    : QWidget(parent)
{
    // Lưu log theo ngày vào thư mục logs/ cạnh file thực thi ServoHMI
    QString logDirPath = QCoreApplication::applicationDirPath() + "/logs";
    QDir().mkpath(logDirPath);
    QString dateStr = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    m_logFilePath = logDirPath + QString("/scada_%1.log").arg(dateStr);

    setupUi();
}

void LogPage::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // Thanh công cụ lọc & lưu log
    auto *topLayout = new QHBoxLayout();

    m_chkShowTraffic = new QCheckBox(tr("Show TX/RX"), this);
    m_chkShowTraffic->setChecked(true);
    connect(m_chkShowTraffic, &QCheckBox::toggled, this, &LogPage::rebuildView);

    m_chkSaveTraffic = new QCheckBox(tr("Save TX/RX to file"), this);
    m_chkSaveTraffic->setChecked(false);
    m_chkSaveTraffic->setToolTip(tr("INFO/WARN/ALARM are always saved automatically"));

    m_btnClear = new QPushButton(tr("Clear View"), this);
    connect(m_btnClear, &QPushButton::clicked, this, &LogPage::clearView);

    m_btnOpenFolder = new QPushButton(tr("Open Folder"), this);
    connect(m_btnOpenFolder, &QPushButton::clicked, this, &LogPage::openLogFolder);

    const QFileInfo logInfo(m_logFilePath);
    m_lblFilePath = new QLabel(tr("Log: %1").arg(logInfo.fileName()), this);
    m_lblFilePath->setToolTip(m_logFilePath);
    m_lblFilePath->setStyleSheet("color: #4B5563; font-size: 11px;");

    topLayout->addWidget(m_chkShowTraffic);
    topLayout->addWidget(m_chkSaveTraffic);
    topLayout->addStretch();
    topLayout->addWidget(m_lblFilePath);
    topLayout->addSpacing(8);
    topLayout->addWidget(m_btnOpenFolder);
    topLayout->addWidget(m_btnClear);

    // Khung text monospace chỉ đọc, giới hạn 500 dòng giữ RAM ổn định
    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setMaximumBlockCount(kMaxEntries);
    m_textEdit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(m_textEdit, 1);
}

QString LogPage::logFilePath() const {
    return m_logFilePath;
}

void LogPage::appendLog(Level level, const QString &message) {
    Entry entry;
    entry.timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    entry.level = level;
    entry.message = message;

    m_entries.append(entry);
    if (m_entries.size() > kMaxEntries) {
        m_entries.removeFirst();
    }

    renderEntry(entry);

    // Sự kiện hệ thống & cảnh báo (INFO/WARN/ALARM) luôn tự ghi ra file;
    // TX/RX chỉ ghi file khi người dùng tích chọn để tránh phình file lúc Sweep.
    const bool isTraffic = (level == Level::Tx || level == Level::Rx);
    if (!isTraffic || m_chkSaveTraffic->isChecked()) {
        persistEntry(entry);
    }
}

void LogPage::clearView() {
    m_entries.clear();
    m_textEdit->clear();
}

void LogPage::openLogFolder() {
    const QString dirPath = QFileInfo(m_logFilePath).absolutePath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
}

void LogPage::rebuildView() {
    m_textEdit->clear();
    for (const Entry &entry : m_entries) {
        renderEntry(entry);
    }
}

void LogPage::renderEntry(const Entry &entry) {
    const bool isTraffic = (entry.level == Level::Tx || entry.level == Level::Rx);
    if (isTraffic && !m_chkShowTraffic->isChecked()) {
        return;
    }

    QTextCursor cursor = m_textEdit->textCursor();
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat fmt;
    fmt.setForeground(levelColor(entry.level));
    if (entry.level == Level::Alarm) {
        fmt.setFontWeight(QFont::Bold);
    }

    QString line = QString("[%1] [%2] %3\n")
                       .arg(entry.timestamp, levelTag(entry.level).leftJustified(5, ' '), entry.message);
    cursor.insertText(line, fmt);

    m_textEdit->setTextCursor(cursor);
    m_textEdit->ensureCursorVisible();
}

void LogPage::persistEntry(const Entry &entry) {
    QFile file(m_logFilePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << "[" << entry.timestamp << "] ["
            << levelTag(entry.level).leftJustified(5, ' ') << "] "
            << entry.message << "\n";
    }
}

QString LogPage::levelTag(Level level) {
    switch (level) {
        case Level::Info:  return "INFO";
        case Level::Tx:    return "TX";
        case Level::Rx:    return "RX";
        case Level::Warn:  return "WARN";
        case Level::Alarm: return "ALARM";
    }
    return "INFO";
}

QColor LogPage::levelColor(Level level) {
    // Chọn tông màu đậm để hiển thị rõ trên nền sáng (Light Theme)
    switch (level) {
        case Level::Info:  return QColor("#1F2937");
        case Level::Tx:    return QColor("#1D4ED8");
        case Level::Rx:    return QColor("#15803D");
        case Level::Warn:  return QColor("#B45309");
        case Level::Alarm: return QColor("#DC2626");
    }
    return QColor("#1F2937");
}
