#include "CommonDictPage.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

#include "ElaAcrylicUrlCard.h"
#include "ElaFlowLayout.h"
#include "ElaImageCard.h"
#include "ElaMenu.h"
#include "ElaMessageBar.h"
#include "ElaNavigationRouter.h"
#include "ElaPopularCard.h"
#include "ElaScrollArea.h"
#include "ElaText.h"
#include "ElaToolTip.h"

CommonDictPage::CommonDictPage(QWidget* parent)
    : BasePage(parent)
{
    setWindowTitle("Home");

    addCentralWidget(new ElaText("还没写", this));
}

CommonDictPage::~CommonDictPage()
{
}
