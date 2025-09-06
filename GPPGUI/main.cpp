#include <QApplication>
#include <Windows.h>

#include "ElaApplication.h"
#include "mainwindow.h"

#pragma comment(lib, "../lib/GalTranslPP.lib")
#pragma comment(lib, "../lib/ElaWidgetTools.lib")


int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    eApp->init();
    MainWindow w;
    w.show();
    return a.exec();
}
