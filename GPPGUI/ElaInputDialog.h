#ifndef ELAINPUTDIALOG_H
#define ELAINPUTDIALOG_H

#include "ElaContentDialog.h"

class ElaLineEdit;

class ElaInputDialog : public ElaContentDialog
{
    Q_OBJECT

public:
    explicit ElaInputDialog(const QString& label, const QString& text, QString& result, QWidget* parent = nullptr);

public Q_SLOTS:
	void onRightButtonClicked() override;
	void onMiddleButtonClicked() override;

private:
    QString& m_result;
    ElaLineEdit* m_lineEdit = nullptr;

};

#endif
