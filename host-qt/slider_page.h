#pragma once

#include <QList>
#include <QWidget>

class QSlider;
class QSpinBox;
class QLabel;
class QPushButton;

class SliderPage : public QWidget {
    Q_OBJECT

public:
    explicit SliderPage(QWidget *parent = nullptr);
    ~SliderPage() override = default;

    int angle() const;

public slots:
    void setAngle(int angle);
    // Cập nhật giao diện khi phần cứng tự đổi góc (E-Stop PA0 / Reset), KHÔNG phát signal ngược lại
    void syncAngleFromHardware(int angle);
    void setLocked(bool locked);

signals:
    void angleChanged(int newAngle);

private slots:
    void onSliderValueChanged(int value);
    void onSpinBoxValueChanged(int value);

private:
    void setupUi();
    void updateAngleInternal(int angle, bool updateSlider, bool updateSpinBox);

    QSlider              *m_slider{nullptr};
    QLabel               *m_lblBigAngle{nullptr};
    QSpinBox             *m_spinBox{nullptr};
    QList<QPushButton *>  m_presetButtons;
    bool                  m_locked{false};
};
