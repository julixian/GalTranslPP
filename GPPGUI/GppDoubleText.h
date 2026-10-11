#pragma once

#include "ElaText.h"
#include "ElaToolTip.h"

class ElaLineEdit;

class GppDoubleText : public QWidget
{
    Q_OBJECT

public:
    explicit GppDoubleText(const QString& firstLine, int firstLinePixelSize,
        const QString& secondLine, int secondLinePixelSize, const QString& toolTip, QWidget* parent = nullptr);

    [[nodiscard]] QString getFirstLineText() const;

private:
    ElaText* m_firstLine = nullptr;
    ElaText* m_secondLine = nullptr;
    ElaToolTip* m_toolTip = nullptr;
};
