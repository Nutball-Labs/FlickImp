// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#pragma once
#include <QMainWindow>

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QCheckBox;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void refreshStatus();
    void onStart();
    void onStop();
    void onRestart();
    void onToggleBoot(bool checked);
    void onApplyPort();
    void onSaveCredentials();
    void onOpenBrowser();

private:
    void    runPrivileged(const QString& action);
    QString queryService(const QStringList& args) const;
    void    writeSystemConfig(int port, const QString& dbPath,
                              const QString& webRoot,
                              const QString& apiKey, const QString& bearerToken);

    QLabel*      statusLabel_;
    QPushButton* startBtn_;
    QPushButton* stopBtn_;
    QPushButton* restartBtn_;
    QCheckBox*   bootCheck_;
    QSpinBox*    portSpin_;
    QLineEdit*   dbPathEdit_;
    QLineEdit*   webRootEdit_;
    QLineEdit*   apiKeyEdit_;
    QLineEdit*   bearerEdit_;
    QTimer*      refreshTimer_;
};

// SN: 00003
