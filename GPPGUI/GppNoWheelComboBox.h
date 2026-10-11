#pragma once

#include "GppComboBox.h"
#include <QWheelEvent>

class GppNoWheelComboBox : public GppComboBox
{
	Q_OBJECT
public:
	explicit GppNoWheelComboBox(QWidget* parent = nullptr) : GppComboBox(parent) { }

protected:
	// 重写滚轮事件
	void wheelEvent(QWheelEvent* event) override
	{
		// 忽略滚轮事件
		event->ignore();
	}
};
