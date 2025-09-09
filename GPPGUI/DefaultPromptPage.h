#ifndef DEFAULTPROMPTPAGE_H
#define DEFAULTPROMPTPAGE_H

#include <toml++/toml.hpp>
#include <QStackedWidget>
#include <functional>
#include "BasePage.h"

class ElaPivot;

class DefaultPromptPage : public BasePage
{
    Q_OBJECT
public:

    explicit DefaultPromptPage(QWidget* parent = nullptr);
    ~DefaultPromptPage();

public Q_SLOTS:
    void apply2Config();

private:

    ElaPivot* _pivot;
    QStackedWidget* _stackedWidget;

    toml::table _promptConfig;
    std::function<void()> _applyFunc;

    void _setupUI();
};

#endif // DEFAULTPROMPTPAGE_H
