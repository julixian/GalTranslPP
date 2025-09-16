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
}

void BasePage::apply2Config()
{
    if (_applyFunc) {
        _applyFunc();
    }
}

BasePage::~BasePage()
{
}
