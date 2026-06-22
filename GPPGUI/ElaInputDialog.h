#ifndef ELAINPUTDIALOG_H
#define ELAINPUTDIALOG_H

#include "ElaContentDialog.h"

class ElaLineEdit;

class ElaInputDialog : public ElaContentDialog
{
    Q_OBJECT

public:
    explicit ElaInputDialog(QWidget* parent, const QString& label, const QString& text, QString& result);
    ~ElaInputDialog() override;

private Q_SLOTS:
    virtual void onRightButtonClicked() override;
    virtual void onMiddleButtonClicked() override;

private:
    QString& m_result;
    ElaLineEdit* m_lineEdit = nullptr;

};

#endif // ELAINPUTDIALOG_H
