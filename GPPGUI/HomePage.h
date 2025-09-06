#ifndef HOMEPAGE_H
#define HOMEPAGE_H

#include "BasePage.h"

class HomePage : public BasePage
{
    Q_OBJECT
public:

    Q_INVOKABLE explicit HomePage(QWidget* parent = nullptr);
    ~HomePage();
};

#endif // HOMEPAGE_H
