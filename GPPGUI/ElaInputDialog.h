// ElaInputDialog.h

#ifndef ELAINPUTDIALOG_H
#define ELAINPUTDIALOG_H

#include "ElaContentDialog.h"

class ElaLineEdit;

class ElaInputDialog : public ElaContentDialog
{
    Q_OBJECT

public:
    explicit ElaInputDialog(QWidget* parent, const QString& label, const QString& text, QString& result, bool* ok);
    ~ElaInputDialog();

private Q_SLOTS:
    virtual void onRightButtonClicked() override;
    virtual void onMiddleButtonClicked() override;

private:
    QString& _result;
    ElaLineEdit* _lineEdit;
    bool* _ok;

};

#endif // ELAINPUTDIALOG_H