#pragma once

#include "ElaRadioButton.h"
#include <QFont>

class GppRadioButton : public ElaRadioButton {
public:
    explicit GppRadioButton(QWidget* parent = nullptr)
        : GppRadioButton(QString{}, parent) {}

    explicit GppRadioButton(const QString& text, QWidget* parent = nullptr)
        : ElaRadioButton(text, parent) {
        // 统一文字和随字体高度计算的单选圆圈尺寸。
        auto buttonFont = font();
        buttonFont.setPixelSize(15);
        setFont(buttonFont);
    }
};
