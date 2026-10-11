#pragma once

#include "ElaMultiSelectComboBox.h"

class GppMultiSelectComboBox : public ElaMultiSelectComboBox
{
public:
    explicit GppMultiSelectComboBox(QWidget* parent = nullptr) : ElaMultiSelectComboBox(parent)
    {
        setFixedHeight(35);
    }
};
