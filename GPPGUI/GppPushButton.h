#pragma once

#include "ElaPushButton.h"

class GppPushButton : public ElaPushButton
{
public:
    explicit GppPushButton(QWidget* parent = nullptr) : ElaPushButton(parent)
    {
        setFixedHeight(35);
    }

    explicit GppPushButton(const QString& text, QWidget* parent = nullptr)
        : GppPushButton(parent)
    {
        setText(text);
    }
};
