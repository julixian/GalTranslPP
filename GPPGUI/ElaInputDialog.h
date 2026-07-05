#ifndef ELAINPUTDIALOG_H
#define ELAINPUTDIALOG_H

#include "ElaContentDialog.h"

class ElaLineEdit;

class ElaInputDialog : public ElaContentDialog
{
    Q_OBJECT

public:
    explicit ElaInputDialog(const QString& label, const QString& text, QString& result, QWidget* parent = nullptr);
    ~ElaInputDialog() override;

private Q_SLOTS:
    virtual void onRightButtonClicked() override;
    virtual void onMiddleButtonClicked() override;

private:
    QString& m_result;
    ElaLineEdit* m_lineEdit = nullptr;

};

#endif // ELAINPUTDIALOG_H
