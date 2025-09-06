#ifndef ABOUTDIALOG_H
#define ABOUTDIALOG_H

#include <ElaDialog.h>

class AboutDialog : public ElaDialog
{
    Q_OBJECT
public:
    explicit AboutDialog(QWidget* parent = nullptr);
    ~AboutDialog();
};

#endif // ABOUTDIALOG_H
