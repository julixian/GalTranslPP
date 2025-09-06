#ifndef DEFAULTPROMPTPAGE_H
#define DEFAULTPROMPTPAGE_H

#include "BasePage.h"

class DefaultPromptPage : public BasePage
{
    Q_OBJECT
public:

    explicit DefaultPromptPage(QWidget* parent = nullptr);
    ~DefaultPromptPage();
};

#endif // DEFAULTPROMPTPAGE_H
