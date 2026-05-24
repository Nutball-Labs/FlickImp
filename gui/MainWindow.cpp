// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Nutball Labs / Stephen Berg
#include "MainWindow.h"
#include "../lib/config.hpp"
#include "../lib/version.hpp"
#include <QCheckBox>
#include <QDesktopServices>
#include <QFile>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QProcess>
#include <QSpinBox>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

static constexpr const char* SERVICE        = "flickimp";
static constexpr const char* SYSTEM_CONFIG  = "/etc/flickimp/fi_config.json";
static constexpr const char* STAGING_CONFIG = "/tmp/.fi_flickimp_config_staging";
static constexpr int          DEFAULT_PORT  = 8647;

// ---------- service helpers -------------------------------------------------

QString MainWindow::queryService(const QStringList& args) const {
    QProcess p;
    p.start("systemctl", args);
    p.waitForFinished(3000);
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

void MainWindow::runPrivileged(const QString& action) {
    QProcess::execute("pkexec", {"systemctl", action, SERVICE});
}

// Write all settings atomically to the system config via pkexec.
void MainWindow::writeSystemConfig(int port, const QString& dbPath,
                                   const QString& webRoot,
                                   const QString& apiKey,
                                   const QString& bearerToken) {
    FlickImp::Config cfg = FlickImp::load_config_from(SYSTEM_CONFIG);
    cfg.port               = port;
    cfg.fi_db_path         = dbPath.toStdString();
    cfg.fi_web_root        = webRoot.toStdString();
    cfg.tmdb_api_key       = apiKey.toStdString();
    cfg.tmdb_bearer_token  = bearerToken.toStdString();
    FlickImp::save_config_to(cfg, STAGING_CONFIG);
    QProcess::execute("pkexec", {
        "install", "-m", "644", "-o", "root", "-g", "flickimp",
        STAGING_CONFIG, SYSTEM_CONFIG
    });
    QFile::remove(STAGING_CONFIG);
}

// ---------- MainWindow ------------------------------------------------------

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("FlickImp Service");
    setFixedSize(440, 560);

    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    // --- Identity header ---
    auto* hdrRow = new QHBoxLayout;
    hdrRow->setSpacing(12);

    auto* logoLabel = new QLabel;
    QPixmap logoPixmap(":/images/FlickImp_icon.png");
    logoLabel->setPixmap(logoPixmap.scaledToHeight(64, Qt::SmoothTransformation));
    logoLabel->setFixedSize(64, 64);
    hdrRow->addWidget(logoLabel);

    auto* titleCol = new QVBoxLayout;
    titleCol->setSpacing(2);
    auto* appNameLabel = new QLabel(APP_NAME);
    QFont titleFont = appNameLabel->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    appNameLabel->setFont(titleFont);
    auto* versionLabel = new QLabel(QString("Version %1").arg(APP_VERSION));
    versionLabel->setStyleSheet("color: gray; font-size: 10pt;");
    titleCol->addWidget(appNameLabel);
    titleCol->addWidget(versionLabel);
    hdrRow->addLayout(titleCol);
    hdrRow->addStretch();
    root->addLayout(hdrRow);

    auto* sep0 = new QFrame;
    sep0->setFrameShape(QFrame::HLine);
    sep0->setFrameShadow(QFrame::Sunken);
    root->addWidget(sep0);

    // --- Status row ---
    auto* statusRow = new QHBoxLayout;
    statusRow->addWidget(new QLabel("Service:"));
    statusLabel_ = new QLabel("Checking…");
    statusLabel_->setMinimumWidth(140);
    statusRow->addWidget(statusLabel_);
    statusRow->addStretch();
    root->addLayout(statusRow);

    // --- Control buttons ---
    auto* btnRow = new QHBoxLayout;
    startBtn_   = new QPushButton("Start");
    stopBtn_    = new QPushButton("Stop");
    restartBtn_ = new QPushButton("Restart");
    btnRow->addWidget(startBtn_);
    btnRow->addWidget(stopBtn_);
    btnRow->addWidget(restartBtn_);
    root->addLayout(btnRow);

    connect(startBtn_,   &QPushButton::clicked, this, &MainWindow::onStart);
    connect(stopBtn_,    &QPushButton::clicked, this, &MainWindow::onStop);
    connect(restartBtn_, &QPushButton::clicked, this, &MainWindow::onRestart);

    // --- Boot toggle ---
    bootCheck_ = new QCheckBox("Start automatically at boot");
    root->addWidget(bootCheck_);
    connect(bootCheck_, &QCheckBox::toggled, this, &MainWindow::onToggleBoot);

    // --- Separator ---
    auto* sep1 = new QFrame;
    sep1->setFrameShape(QFrame::HLine);
    sep1->setFrameShadow(QFrame::Sunken);
    root->addWidget(sep1);

    // --- Port setting ---
    FlickImp::Config sysCfg = FlickImp::load_config_from(SYSTEM_CONFIG);

    auto* portRow = new QHBoxLayout;
    portRow->addWidget(new QLabel("Port:"));
    portSpin_ = new QSpinBox;
    portSpin_->setRange(1024, 65535);
    portSpin_->setValue(sysCfg.port > 0 ? sysCfg.port : DEFAULT_PORT);
    portSpin_->setFixedWidth(80);
    auto* portApplyBtn = new QPushButton("Apply");
    portApplyBtn->setFixedWidth(70);
    auto* portNote = new QLabel("(restart to take effect)");
    portNote->setStyleSheet("color: gray; font-size: 10pt;");
    portRow->addWidget(portSpin_);
    portRow->addWidget(portApplyBtn);
    portRow->addWidget(portNote);
    portRow->addStretch();
    root->addLayout(portRow);
    connect(portApplyBtn, &QPushButton::clicked, this, &MainWindow::onApplyPort);

    // --- DB file path ---
    auto* dbRow = new QHBoxLayout;
    dbRow->addWidget(new QLabel("DB File:  "));
    dbPathEdit_ = new QLineEdit;
    dbPathEdit_->setPlaceholderText("empty = auto (/var/lib/flickimp/db)");
    if (!sysCfg.fi_db_path.empty())
        dbPathEdit_->setText(QString::fromStdString(sysCfg.fi_db_path));
    dbRow->addWidget(dbPathEdit_);
    root->addLayout(dbRow);

    // --- Web root path ---
    auto* webRow = new QHBoxLayout;
    webRow->addWidget(new QLabel("Web Root:"));
    webRootEdit_ = new QLineEdit;
    webRootEdit_->setPlaceholderText("empty = auto (/var/lib/flickimp/web)");
    if (!sysCfg.fi_web_root.empty())
        webRootEdit_->setText(QString::fromStdString(sysCfg.fi_web_root));
    webRow->addWidget(webRootEdit_);
    root->addLayout(webRow);

    // --- Separator ---
    auto* sep2 = new QFrame;
    sep2->setFrameShape(QFrame::HLine);
    sep2->setFrameShadow(QFrame::Sunken);
    root->addWidget(sep2);

    // --- TMDB credentials ---
    root->addWidget(new QLabel("TMDB Credentials:"));

    auto* keyRow = new QHBoxLayout;
    keyRow->addWidget(new QLabel("API Key:"));
    apiKeyEdit_ = new QLineEdit;
    apiKeyEdit_->setPlaceholderText("v3 API key from themoviedb.org");
    apiKeyEdit_->setEchoMode(QLineEdit::Password);
    if (!sysCfg.tmdb_api_key.empty())
        apiKeyEdit_->setText(QString::fromStdString(sysCfg.tmdb_api_key));
    keyRow->addWidget(apiKeyEdit_);
    root->addLayout(keyRow);

    auto* bearerRow = new QHBoxLayout;
    bearerRow->addWidget(new QLabel("Bearer:   "));
    bearerEdit_ = new QLineEdit;
    bearerEdit_->setPlaceholderText("v4 Read Access Token from themoviedb.org");
    bearerEdit_->setEchoMode(QLineEdit::Password);
    if (!sysCfg.tmdb_bearer_token.empty())
        bearerEdit_->setText(QString::fromStdString(sysCfg.tmdb_bearer_token));
    bearerRow->addWidget(bearerEdit_);
    root->addLayout(bearerRow);

    auto* saveCredsBtn = new QPushButton("Save Credentials");
    root->addWidget(saveCredsBtn);
    connect(saveCredsBtn, &QPushButton::clicked, this, &MainWindow::onSaveCredentials);

    // --- Open browser ---
    root->addStretch();
    auto* browserBtn = new QPushButton("Open in Browser");
    root->addWidget(browserBtn);
    connect(browserBtn, &QPushButton::clicked, this, &MainWindow::onOpenBrowser);

    // --- Auto-refresh ---
    refreshTimer_ = new QTimer(this);
    connect(refreshTimer_, &QTimer::timeout, this, &MainWindow::refreshStatus);
    refreshTimer_->start(3000);
    refreshStatus();
}

void MainWindow::refreshStatus() {
    QString state = queryService({"is-active", SERVICE});

    if (state == "active") {
        statusLabel_->setText("● Running");
        statusLabel_->setStyleSheet("color: #22c55e; font-weight: bold;");
    } else if (state == "failed") {
        statusLabel_->setText("● Failed");
        statusLabel_->setStyleSheet("color: #ef4444; font-weight: bold;");
    } else if (state == "activating") {
        statusLabel_->setText("● Starting…");
        statusLabel_->setStyleSheet("color: #f59e0b; font-weight: bold;");
    } else if (state == "deactivating") {
        statusLabel_->setText("● Stopping…");
        statusLabel_->setStyleSheet("color: #f59e0b; font-weight: bold;");
    } else {
        statusLabel_->setText("● Stopped");
        statusLabel_->setStyleSheet("color: #6b7280; font-weight: bold;");
    }

    bool running = (state == "active");
    startBtn_->setEnabled(!running);
    stopBtn_->setEnabled(running);
    restartBtn_->setEnabled(running);

    bool enabled = (queryService({"is-enabled", SERVICE}) == "enabled");
    bootCheck_->blockSignals(true);
    bootCheck_->setChecked(enabled);
    bootCheck_->blockSignals(false);
}

void MainWindow::onStart()   { runPrivileged("start");   refreshStatus(); }
void MainWindow::onStop()    { runPrivileged("stop");    refreshStatus(); }
void MainWindow::onRestart() { runPrivileged("restart"); refreshStatus(); }

void MainWindow::onToggleBoot(bool checked) {
    runPrivileged(checked ? "enable" : "disable");
    refreshStatus();
}

void MainWindow::onApplyPort() {
    int port = portSpin_->value();
    writeSystemConfig(port,
        dbPathEdit_->text().trimmed(),
        webRootEdit_->text().trimmed(),
        apiKeyEdit_->text().trimmed(),
        bearerEdit_->text().trimmed());

    auto btn = QMessageBox::question(this, "Port Updated",
        QString("Port saved as %1.\nRestart the service now?").arg(port),
        QMessageBox::Yes | QMessageBox::No);
    if (btn == QMessageBox::Yes) {
        runPrivileged("restart");
        refreshStatus();
    }
}

void MainWindow::onSaveCredentials() {
    writeSystemConfig(portSpin_->value(),
        dbPathEdit_->text().trimmed(),
        webRootEdit_->text().trimmed(),
        apiKeyEdit_->text().trimmed(),
        bearerEdit_->text().trimmed());
    QMessageBox::information(this, "Credentials Saved",
        "TMDB credentials saved to /etc/flickimp/fi_config.json.");
}

void MainWindow::onOpenBrowser() {
    QDesktopServices::openUrl(
        QUrl(QString("http://localhost:%1").arg(portSpin_->value())));
}

// SN: 00003
