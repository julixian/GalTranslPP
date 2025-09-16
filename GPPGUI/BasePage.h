#ifndef BASEPAGE_H
#define BASEPAGE_H

#include <functional>
#include <ElaScrollPage.h>

class BasePage : public ElaScrollPage
{
    Q_OBJECT
public:
    explicit BasePage(QWidget* parent = nullptr);
    ~BasePage() override;

public Q_SLOTS:
    virtual void apply2Config();

protected:
    std::function<void()> _applyFunc;
};

#endif // BASEPAGE_H
