#include "ElaDoubleText.h"

#include <QVBoxLayout>
#include <QFormLayout>

ElaDoubleText::ElaDoubleText(const QString& firstLine, int firstLinePixelSize, const QString& secondLine, int secondLinePixelSize, const QString& toolTip, QWidget* parent)
	: QWidget(parent)
{
	QVBoxLayout* textLayout = new QVBoxLayout(this);
	textLayout->setContentsMargins(0, 5, 0, 5);
	m_firstLine = new ElaText(firstLine, firstLinePixelSize, this);
	m_firstLine->setWordWrap(false);

	if (!toolTip.isEmpty()) {
		m_toolTip = new ElaToolTip(m_firstLine);
		m_toolTip->setToolTip(toolTip);
	}
	textLayout->addWidget(m_firstLine);

	if (!secondLine.isEmpty()) {
		m_secondLine = new ElaText(secondLine, secondLinePixelSize, this);
		m_secondLine->setWordWrap(false);
		textLayout->setSpacing(2);
		textLayout->addWidget(m_secondLine);
	}
}

QString ElaDoubleText::getFirstLineText() const
{
	return m_firstLine->text();
}
