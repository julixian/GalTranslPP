#pragma once

#include <QToolButton>

class ElaMenu;

// 名称、拖动手柄和菜单箭头共用一个外框。
class GppApiCard : public QToolButton
{
public:
    explicit GppApiCard(QWidget* parent = nullptr);
    void setApiName(const QString& name);
    void setApiEnabled(bool enabled);
    void setMenu(ElaMenu* menu);
    QWidget* dragHandle() const;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QWidget* m_dragHandle;
    bool m_apiEnabled = true;
};
