// ValueSliderWidget.h

#ifndef VALUESLIDERWIDGET_H
#define VALUESLIDERWIDGET_H

#include <QWidget>

class ElaSlider;
class ElaDoubleSpinBox;

class ValueSliderWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ValueSliderWidget(QWidget* parent = nullptr);
    ~ValueSliderWidget();

    // 公共接口，用于设置和获取 0.0 - 1.0 之间的值
    void setValue(double value);
    double value() const;
    void setDecimals(int decimals);

Q_SIGNALS:
    // 当值发生变化时，发出此信号，方便外部连接
    void valueChanged(double newValue);

private Q_SLOTS:
    // 私有槽，用于处理内部控件的信号
    void _onSliderValueChanged(int intValue);
    void _onSpinBoxValueChanged(double doubleValue);

private:
    ElaSlider* _slider;
    ElaDoubleSpinBox* _spinBox;
};

#endif // VALUESLIDERWIDGET_H
