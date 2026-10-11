// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher fork - server panels
 *  Based on Prism Launcher (https://prismlauncher.org), GPL-3.0.
 */
#pragma once

#include <QDialog>
#include <QJsonArray>
#include <QString>

class QComboBox;
class QPlainTextEdit;
class QLineEdit;
class QLabel;
class QTableWidget;
class QListWidget;
class QTimer;
class QPushButton;

// One panel per Minecraft server folder: status, start/stop, console, settings, mods, backups.
class ServerPanelDialog : public QDialog {
    Q_OBJECT
   public:
    explicit ServerPanelDialog(QWidget* parent = nullptr);

   private slots:
    void addServer();
    void removeServer();
    void serverChanged();
    void startServer();
    void stopServer();
    void restartServer();
    void sendCommand();
    void refresh();
    void saveProperties();
    void openFolder(const QString& sub = QString());
    void backupNow();

   private:
    QString dir() const;
    QString runCommand(const QString& cmd);  // via ShalevGuard inbox
    bool isOnline() const;
    void loadServers();
    void saveServers();
    void loadProperties();
    void loadMods();
    void loadBackups();
    void tailLog();

    QComboBox* m_servers;
    QLabel* m_status;
    QPlainTextEdit* m_console;
    QLineEdit* m_cmd;
    QTableWidget* m_props;
    QListWidget* m_mods;
    QListWidget* m_backups;
    QTimer* m_timer;
    QJsonArray m_list;  // [{name, path, port}]
    qint64 m_logPos = 0;
    QString m_logPath;
    int m_counter = 0;
};
