#include "ElaInputDialog.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QDebug>

#include "ElaScrollPageArea.h"
#include "ElaText.h"
#include "ElaLineEdit.h"

import std;

ElaInputDialog::ElaInputDialog(QWidget* parent, const QString& label, const QString& text, QString& result, bool* ok) :
	ElaContentDialog(parent), _result(result), _ok(ok)
{
	setWindowTitle("InputDialog");

	setLeftButtonText("Cancel");
	setMiddleButtonText("Reset");
	setRightButtonText("OK");

	*_ok = false;

	// 创建一个中心部件和布局
	QWidget* centerWidget = new QWidget(this);
	QVBoxLayout* mainLayout = new QVBoxLayout(centerWidget);

	ElaText* labelText = new ElaText(label, centerWidget);
	labelText->setTextPixelSize(16);
	mainLayout->addWidget(labelText);

	_lineEdit = new ElaLineEdit(centerWidget);
	_lineEdit->setPlaceholderText(text);
	mainLayout->addWidget(_lineEdit);

	setCentralWidget(centerWidget);
}

ElaInputDialog::~ElaInputDialog()
{

}

void ElaInputDialog::onRightButtonClicked()
{
	*_ok = true;
	_result = _lineEdit->text();
}

void ElaInputDialog::onMiddleButtonClicked()
{
	_lineEdit->clear();
}
