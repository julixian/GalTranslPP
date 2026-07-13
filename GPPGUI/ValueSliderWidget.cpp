#include "ValueSliderWidget.h"

#include <QHBoxLayout>
#include "ElaSlider.h"
#include "ElaDoubleSpinBox.h"

ValueSliderWidget::ValueSliderWidget(double minValue, double maxValue, QWidget* parent)
    : QWidget(parent), m_minValue(minValue), m_maxValue(maxValue)
{
    // 1. 创建子控件
    m_slider = new ElaSlider(Qt::Horizontal, this);
    m_spinBox = new ElaDoubleSpinBox(this);

    // 2. 设置子控件的属性
    m_slider->setRange((int)(m_minValue * 100), (int)(m_maxValue * 100));

    m_spinBox->setRange(m_minValue, m_maxValue);
    m_spinBox->setDecimals(3); // 显示两位小数
    m_spinBox->setSingleStep(0.001); // 每次点击上下箭头变化的步长
    m_spinBox->setFixedWidth(140); // 给一个合适的固定宽度

    // 3. 创建布局并添加子控件
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(m_slider);
    layout->addWidget(m_spinBox);

    // 4. 建立内部信号槽连接
    connect(m_slider, &ElaSlider::valueChanged, this, &ValueSliderWidget::onSliderValueChanged);
    connect(m_spinBox, QOverload<double>::of(&ElaDoubleSpinBox::valueChanged), this, &ValueSliderWidget::onSpinBoxValueChanged);

    // 5. 设置初始值
    setValue((m_minValue + m_maxValue) / 2);
}

// 公共接口：设置值
void ValueSliderWidget::setValue(double value)
{
    value = qBound(m_minValue, value, m_maxValue);

    // 阻塞信号，防止在用代码设置值时触发无限循环
    m_slider->blockSignals(true);
    m_spinBox->blockSignals(true);

    m_slider->setValue((int)(value * 100));
    m_spinBox->setValue(value);

    // 恢复信号
    m_slider->blockSignals(false);
    m_spinBox->blockSignals(false);

    // 发出一次信号，通知外部值已改变
    Q_EMIT valueChangedSignal(value);
}

// 公共接口：获取值
double ValueSliderWidget::value() const
{
    return m_spinBox->value();
}

void ValueSliderWidget::setDecimals(int decimals) {
    m_spinBox->setDecimals(decimals);
    m_spinBox->setSingleStep(pow(0.1, decimals));
}

// 私有槽：当滑块的值改变时
void ValueSliderWidget::onSliderValueChanged(int intValue)
{
    // 将滑块的整数值 转换为浮点数值
    double doubleValue = intValue / 100.0;

    // 更新数字框的值，注意阻塞信号
    m_spinBox->blockSignals(true);
    m_spinBox->setValue(doubleValue);
    m_spinBox->blockSignals(false);

    // 发出信号，通知外部
    Q_EMIT valueChangedSignal(doubleValue);
}

// 私有槽：当数字框的值改变时
void ValueSliderWidget::onSpinBoxValueChanged(double doubleValue)
{
    // 将数字框的浮点数值  转换为整数值
    int intValue = (int)(doubleValue * 100);

    // 更新滑块的值，注意阻塞信号
    m_slider->blockSignals(true);
    m_slider->setValue(intValue);
    m_slider->blockSignals(false);

    // 发出信号，通知外部
    Q_EMIT valueChangedSignal(doubleValue);
}
