#include "DefaultPromptPage.h"

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

DefaultPromptPage::DefaultPromptPage(QWidget* parent)
    : BasePage(parent)
{
    setWindowTitle("默认提示词管理");

    addCentralWidget(new ElaText("还没写", this));
}

DefaultPromptPage::~DefaultPromptPage()
{
}
