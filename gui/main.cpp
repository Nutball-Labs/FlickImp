// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#include "MainWindow.h"
#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("FlickImp Config");
    app.setApplicationDisplayName("FlickImp Service");
    app.setOrganizationName("Nutball-Labs");

    MainWindow w;
    w.show();
    return app.exec();
}

// SN: 00001
