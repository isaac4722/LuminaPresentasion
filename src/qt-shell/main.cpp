// src/qt-shell/main.cpp — Carcasa Qt 5.15.2
//
// Tercera carcasa opcional. Mismo protocolo IPC que la carcasa gestionada.
// Qt 5.15.2 LGPL, enlace dinámico, DLL oficiales sin modificar.

#include "MainWindow.h"
#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("FusionQtShell");
    app.setOrganizationName("FusionHP");
    app.setApplicationVersion("1.0.0");

    MainWindow w;
    w.show();

    return app.exec();
}
