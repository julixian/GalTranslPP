#include "BasePage.h"
#include "ElaTheme.h"

BasePage::BasePage(QWidget* parent)
    : ElaScrollPage(parent)
{
    connect(eTheme, &ElaTheme::themeModeChanged, this, [=]() {
        if (!parent)
        {
            update();
        }
    });

    setContentsMargins(0, 0, 0, 0);
}

void BasePage::apply2Config()
{
    if (m_applyFunc) {
        m_applyFunc();
    }
}

BasePage::~BasePage()
{
}
