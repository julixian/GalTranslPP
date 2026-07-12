#ifndef VALUESLIDERWIDGET_H
#define VALUESLIDERWIDGET_H

#include <QWidget>

class ElaSlider;
class ElaDoubleSpinBox;

class ValueSliderWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ValueSliderWidget(double minValue = 0.0, double maxValue = 1.0, QWidget* parent = nullptr);

    // 公共接口，用于设置和获取 minValue - maxValue 之间的值
    void setValue(double value);
    double value() const;
    void setDecimals(int decimals);

Q_SIGNALS:
    // 当值发生变化时，发出此信号，方便外部连接
    void valueChangedSignal(double newValue);

private Q_SLOTS:
    // 私有槽，用于处理内部控件的信号
    void onSliderValueChanged(int intValue);
    void onSpinBoxValueChanged(double doubleValue);

private:
    ElaSlider* m_slider = nullptr;
    ElaDoubleSpinBox* m_spinBox = nullptr;
    double m_maxValue;
    double m_minValue;
};

#endif
