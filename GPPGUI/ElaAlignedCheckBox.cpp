#include "ElaAlignedCheckBox.h"

#include <QPainter>
#include <QStyleOptionButton>

ElaAlignedCheckBox::ElaAlignedCheckBox(QWidget* parent)
    : ElaCheckBox(parent)
{

}

ElaAlignedCheckBox::ElaAlignedCheckBox(const QString& text, QWidget* parent)
    : ElaCheckBox(text, parent)
{

}

QSize ElaAlignedCheckBox::sizeHint() const
{
    return ElaCheckBox::sizeHint() + QSize(0, IndicatorTopOffset);
}

QSize ElaAlignedCheckBox::minimumSizeHint() const
{
    return ElaCheckBox::minimumSizeHint() + QSize(0, IndicatorTopOffset);
}

void ElaAlignedCheckBox::paintEvent(QPaintEvent*)
{
    QStyleOptionButton option;
    initStyleOption(&option);

    const int indicatorWidth = style()->pixelMetric(QStyle::PM_IndicatorWidth, &option, this);
    const QRect indicatorClip(option.rect.x(), option.rect.y(), indicatorWidth, option.rect.height());
    const QRect textClip(option.rect.x() + indicatorWidth, option.rect.y(),
        option.rect.width() - indicatorWidth, option.rect.height());

    QStyleOptionButton textOption = option;
    textOption.rect.adjust(0, 0, 0, -IndicatorTopOffset);

    QPainter painter(this);
    painter.save();
    painter.setClipRect(textClip);
    style()->drawControl(QStyle::CE_CheckBox, &textOption, &painter, this);
    painter.restore();

    QStyleOptionButton indicatorOption = textOption;
    indicatorOption.rect.translate(0, IndicatorTopOffset);
    painter.setClipRect(indicatorClip);
    style()->drawControl(QStyle::CE_CheckBox, &indicatorOption, &painter, this);
}
