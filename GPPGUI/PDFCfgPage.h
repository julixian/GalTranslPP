#ifndef PDFCFGPAGE_H
#define PDFCFGPAGE_H

#include <toml.hpp>
#include "BasePage.h"

class PDFCfgPage : public BasePage
{
    Q_OBJECT

public:
    explicit PDFCfgPage(toml::ordered_value& projectConfig, QWidget* parent = nullptr);
    ~PDFCfgPage() override;

private:
    toml::ordered_value& m_projectConfig;
};

#endif // PDFCFGPAGE_H
