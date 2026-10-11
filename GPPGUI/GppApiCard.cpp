#include "GppApiCard.h"

#include "ElaApplication.h"
#include "ElaMenu.h"
#include "ElaTheme.h"
#include <QPainter>

GppApiCard::GppApiCard(QWidget* parent) : QToolButton(parent)
{
    setPopupMode(QToolButton::InstantPopup);
    setFixedHeight(35);
    auto cardFont = font();
    cardFont.setPixelSize(eApp->getFontPixelSize() + 2);
    setFont(cardFont);
    // 手柄独占左侧区域，不使用按钮样式的命中范围，也不向菜单按钮传递鼠标事件。
    m_dragHandle = new QWidget(this);
    m_dragHandle->setAttribute(Qt::WA_NoMousePropagation);
    m_dragHandle->setGeometry(0, 0, 34, height());
    connect(eTheme, &ElaTheme::themeModeChanged, this, [this]()
        {
            update();
        });
}

void GppApiCard::setApiName(const QString& name)
{
    setText(name);
    setToolTip(name);
    setFixedWidth(qBound(220, fontMetrics().horizontalAdvance(name) + 78, 460));
}

void GppApiCard::setApiEnabled(bool enabled)
{
    m_apiEnabled = enabled;
    update();
}

void GppApiCard::setMenu(ElaMenu* menu)
{
    // 交给 QToolButton 管理菜单，并阻止关闭菜单的鼠标点击重新触发弹出。
    menu->setAttribute(Qt::WA_NoMouseReplay);
    QToolButton::setMenu(menu);
    connect(menu, &QMenu::aboutToShow, this, [this]()
        {
            update();
        });
    connect(menu, &QMenu::aboutToHide, this, [this]()
        {
            update();
        });
}

QWidget* GppApiCard::dragHandle() const
{
    return m_dragHandle;
}

void GppApiCard::paintEvent(QPaintEvent*)
{
    const auto theme = eTheme->getThemeMode();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(ElaThemeColor(theme, BasicBorder));
    const auto background = isDown()
        ? ElaThemeType::BasicPress
        : underMouse() ? ElaThemeType::BasicHover : ElaThemeType::BasicBase;
    painter.setBrush(eTheme->getThemeColor(theme, background));
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);

    painter.setPen(m_apiEnabled ? ElaThemeColor(theme, BasicText) : ElaThemeColor(theme, BasicTextDisable));
    const QRect textRect(40, 0, width() - 76, height());
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
        fontMetrics().elidedText(text(), Qt::ElideRight, textRect.width()));
    QFont iconFont("ElaAwesome");
    iconFont.setPixelSize(18);
    painter.setFont(iconFont);
    painter.setPen(ElaThemeColor(theme, BasicText));
    painter.drawText(m_dragHandle->geometry(), Qt::AlignCenter,
        QChar(static_cast<char16_t>(ElaIconType::GripVertical)));
    iconFont.setPixelSize(13);
    painter.setFont(iconFont);
    painter.setPen(m_apiEnabled ? ElaThemeColor(theme, BasicText) : ElaThemeColor(theme, BasicTextDisable));
    painter.drawText(QRect(width() - 28, 0, 20, height()), Qt::AlignCenter,
        QChar(static_cast<char16_t>(ElaIconType::AngleDown)));
}
