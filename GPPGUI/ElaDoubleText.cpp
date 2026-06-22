#include "ElaDoubleText.h"

#include <QVBoxLayout>
#include <QFormLayout>

ElaDoubleText::ElaDoubleText(QWidget* parent, const QString& firstLine, int firstLinePixelSize, const QString& secondLine, int secondLinePixelSize, const QString& toolTip)
	: QWidget(parent)
{
	QVBoxLayout* textLayout = new QVBoxLayout(this);
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
		textLayout->addWidget(m_secondLine);
	}
}

QString ElaDoubleText::getFirstLineText() const
{
	return m_firstLine->text();
}

ElaDoubleText::~ElaDoubleText()
{

}
