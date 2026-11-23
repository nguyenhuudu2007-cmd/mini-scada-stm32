#include "slider_page.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSlider>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>

SliderPage::SliderPage(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    setAngle(90);
}

void SliderPage::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // Box điều khiển thanh trượt
    auto *boxSlider = new QGroupBox(tr("Slider Control"), this);
    auto *sliderLayout = new QHBoxLayout(boxSlider);

    m_slider = new QSlider(Qt::Horizontal, this);
    m_slider->setRange(0, 180);
    m_slider->setTickPosition(QSlider::TicksBelow);
    m_slider->setTickInterval(15);

    m_lblBigAngle = new QLabel("90 deg", this);
    m_lblBigAngle->setAlignment(Qt::AlignCenter);
    m_lblBigAngle->setMinimumWidth(80);

    sliderLayout->addWidget(m_slider, 1);
    sliderLayout->addWidget(m_lblBigAngle);
    mainLayout->addWidget(boxSlider);

    // Box điều khiển chính xác & presets (dùng && để hiện dấu & thay vì gạch chân phím tắt)
    auto *boxInput = new QGroupBox(tr("Target && Presets"), this);
    auto *inputLayout = new QHBoxLayout(boxInput);

    auto *lblTarget = new QLabel(tr("Target Angle:"), this);
    m_spinBox = new QSpinBox(this);
    m_spinBox->setRange(0, 180);
    m_spinBox->setSuffix(" deg");

    inputLayout->addWidget(lblTarget);
    inputLayout->addWidget(m_spinBox);
    inputLayout->addSpacing(20);

    auto *lblPresets = new QLabel(tr("Presets:"), this);
    inputLayout->addWidget(lblPresets);

    const QList<int> presetAngles = {0, 45, 90, 135, 180};
    for (int pAngle : presetAngles) {
        auto *btnPreset = new QPushButton(QString("%1 deg").arg(pAngle), this);
        connect(btnPreset, &QPushButton::clicked, this, [this, pAngle]() {
            setAngle(pAngle);
        });
        m_presetButtons.append(btnPreset);
        inputLayout->addWidget(btnPreset);
    }

    inputLayout->addStretch();
    mainLayout->addWidget(boxInput);
    mainLayout->addStretch();

    // Kết nối nội bộ
    connect(m_slider, &QSlider::valueChanged, this, &SliderPage::onSliderValueChanged);
    connect(m_slider, &QSlider::sliderReleased, this, [this]() {
        emit angleChanged(m_slider->value());
    });
    connect(m_spinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &SliderPage::onSpinBoxValueChanged);
}

int SliderPage::angle() const {
    return m_slider->value();
}

void SliderPage::setAngle(int angle) {
    if (m_locked) {
        return;
    }
    updateAngleInternal(angle, true, true);
}

void SliderPage::setLocked(bool locked) {
    m_locked = locked;
    const bool enabled = !locked;
    m_slider->setEnabled(enabled);
    m_spinBox->setEnabled(enabled);
    for (QPushButton *btn : m_presetButtons) {
        btn->setEnabled(enabled);
    }
}

void SliderPage::syncAngleFromHardware(int angle) {
    angle = qBound(0, angle, 180);
    m_lblBigAngle->setText(QString("%1 deg").arg(angle));

    // Khóa signal tạm thời để tránh kích hoạt onSliderValueChanged/angleChanged gửi ngược xuống STM32
    m_slider->blockSignals(true);
    m_slider->setValue(angle);
    m_slider->blockSignals(false);

    m_spinBox->blockSignals(true);
    m_spinBox->setValue(angle);
    m_spinBox->blockSignals(false);
}

void SliderPage::onSliderValueChanged(int value) {
    updateAngleInternal(value, false, true);
}

void SliderPage::onSpinBoxValueChanged(int value) {
    updateAngleInternal(value, true, false);
}

void SliderPage::updateAngleInternal(int angle, bool updateSlider, bool updateSpinBox) {
    angle = qBound(0, angle, 180);

    m_lblBigAngle->setText(QString("%1 deg").arg(angle));

    if (updateSlider && m_slider->value() != angle) {
        m_slider->blockSignals(true);
        m_slider->setValue(angle);
        m_slider->blockSignals(false);
    }

    if (updateSpinBox && m_spinBox->value() != angle) {
        m_spinBox->blockSignals(true);
        m_spinBox->setValue(angle);
        m_spinBox->blockSignals(false);
    }

    emit angleChanged(angle);
}
