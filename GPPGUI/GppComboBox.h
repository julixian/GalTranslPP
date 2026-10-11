#pragma once

#include "ElaComboBox.h"

class GppComboBox : public ElaComboBox
{
public:
    explicit GppComboBox(QWidget* parent = nullptr) : ElaComboBox(parent)
    {
        setFixedHeight(35);
    }
};
