#ifndef ELAALIGNEDCHECKBOX_H
#define ELAALIGNEDCHECKBOX_H

#include "ElaCheckBox.h"

class ElaAlignedCheckBox final : public ElaCheckBox
{
    Q_OBJECT
public:
    explicit ElaAlignedCheckBox(QWidget* parent = nullptr);
    explicit ElaAlignedCheckBox(const QString& text, QWidget* parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    static constexpr int kIndicatorTopOffset = 8;
};

#endif
