#include "BasePage.h"
#include "ElaTheme.h"

BasePage::BasePage(QWidget* parent)
    : ElaScrollPage(parent)
{
    if (!parent) {
        connect(eTheme, &ElaTheme::themeModeChanged, this, [=]()
            {
                update();
            });
    }
    setContentsMargins(0, 0, 0, 0);
}

BasePage::~BasePage() = default;

void BasePage::apply2Config()
{
    if (m_applyFunc) {
        m_applyFunc();
    }
}
