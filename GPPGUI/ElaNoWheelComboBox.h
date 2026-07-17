#ifndef ELANOWHEELCOMBOBOX_H
#define ELANOWHEELCOMBOBOX_H

#include "ElaComboBox.h"
#include <QWheelEvent>

class ElaNoWheelComboBox : public ElaComboBox
{
	Q_OBJECT
public:
	explicit ElaNoWheelComboBox(QWidget* parent = nullptr) : ElaComboBox(parent) { }

protected:
	// 重写滚轮事件
	void wheelEvent(QWheelEvent* event) override
	{
		// 忽略滚轮事件
		event->ignore();
	}
};

#endif
