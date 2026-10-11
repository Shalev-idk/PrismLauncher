// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Prism Launcher fork - server panels
 *  Based on Prism Launcher (https://prismlauncher.org), GPL-3.0.
 */
#include "ServerPanelDialog.h"

#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QTabWidget>
#include <QTableWidget>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

ServerPanelDialog::ServerPanelDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("Servers"));
    resize(900, 600);

    auto* root = new QVBoxLayout(this);

    // Header: server picker + add/remove
    auto* top = new QHBoxLayout();
    m_servers = new QComboBox();
    m_servers->setMinimumWidth(220);
    auto* add = new QPushButton(tr("Add server..."));
    auto* rem = new QPushButton(tr("Remove"));
    m_status = new QLabel();
    top->addWidget(new QLabel(tr("Server:")));
    top->addWidget(m_servers);
    top->addWidget(add);
    top->addWidget(rem);
    top->addStretch();
    top->addWidget(m_status);
    root->addLayout(top);

    // Controls
    auto* ctl = new QHBoxLayout();
    auto* start = new QPushButton(tr("Start"));
    auto* stop = new QPushButton(tr("Stop"));
    auto* restart = new QPushButton(tr("Restart"));
    auto* folder = new QPushButton(tr("Open folder"));
    ctl->addWidget(start);
    ctl->addWidget(stop);
    ctl->addWidget(restart);
    ctl->addWidget(folder);
    ctl->addStretch();
    root->addLayout(ctl);

    auto* tabs = new QTabWidget();
    root->addWidget(tabs, 1);

    // Console
    auto* consoleTab = new QWidget();
    auto* cl = new QVBoxLayout(consoleTab);
    m_console = new QPlainTextEdit();
    m_console->setReadOnly(true);
    m_console->setMaximumBlockCount(5000);
    m_console->setFont(QFont("monospace"));
    m_cmd = new QLineEdit();
    m_cmd->setPlaceholderText(tr("Type a command (no /) and press Enter"));
    cl->addWidget(m_console, 1);
    cl->addWidget(m_cmd);
    tabs->addTab(consoleTab, tr("Console"));

    // Settings
    auto* setTab = new QWidget();
    auto* sl = new QVBoxLayout(setTab);
    m_props = new QTableWidget(0, 2);
    m_props->setHorizontalHeaderLabels({ tr("Setting"), tr("Value") });
    m_props->horizontalHeader()->setStretchLastSection(true);
    m_props->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    auto* saveProps = new QPushButton(tr("Save (applies after restart)"));
    sl->addWidget(m_props, 1);
    sl->addWidget(saveProps);
    tabs->addTab(setTab, tr("Settings"));

    // Mods
    auto* modTab = new QWidget();
    auto* ml = new QVBoxLayout(modTab);
    m_mods = new QListWidget();
    auto* modBtns = new QHBoxLayout();
    auto* addMod = new QPushButton(tr("Add mod (.jar)..."));
    auto* disMod = new QPushButton(tr("Enable / disable selected"));
    auto* modFolder = new QPushButton(tr("Open mods folder"));
    modBtns->addWidget(addMod);
    modBtns->addWidget(disMod);
    modBtns->addWidget(modFolder);
    modBtns->addStretch();
    ml->addWidget(m_mods, 1);
    ml->addLayout(modBtns);
    tabs->addTab(modTab, tr("Mods"));

    // Backups
    auto* bkTab = new QWidget();
    auto* bl = new QVBoxLayout(bkTab);
    m_backups = new QListWidget();
    auto* bkBtns = new QHBoxLayout();
    auto* bkNow = new QPushButton(tr("Back up now"));
    auto* bkFolder = new QPushButton(tr("Open backups folder"));
    bkBtns->addWidget(bkNow);
    bkBtns->addWidget(bkFolder);
    bkBtns->addStretch();
    bl->addWidget(m_backups, 1);
    bl->addLayout(bkBtns);
    tabs->addTab(bkTab, tr("Backups"));

    m_timer = new QTimer(this);
    m_timer->setInterval(1500);

    connect(add, &QPushButton::clicked, this, &ServerPanelDialog::addServer);
    connect(rem, &QPushButton::clicked, this, &ServerPanelDialog::removeServer);
    connect(m_servers, &QComboBox::currentIndexChanged, this, &ServerPanelDialog::serverChanged);
    connect(start, &QPushButton::clicked, this, &ServerPanelDialog::startServer);
    connect(stop, &QPushButton::clicked, this, &ServerPanelDialog::stopServer);
    connect(restart, &QPushButton::clicked, this, &ServerPanelDialog::restartServer);
    connect(folder, &QPushButton::clicked, this, [this] { openFolder(); });
    connect(m_cmd, &QLineEdit::returnPressed, this, &ServerPanelDialog::sendCommand);
    connect(saveProps, &QPushButton::clicked, this, &ServerPanelDialog::saveProperties);
    connect(modFolder, &QPushButton::clicked, this, [this] { openFolder("mods"); });
    connect(bkFolder, &QPushButton::clicked, this, [this] { openFolder("backups"); });
    connect(bkNow, &QPushButton::clicked, this, &ServerPanelDialog::backupNow);
    connect(addMod, &QPushButton::clicked, this, [this] {
        auto files = QFileDialog::getOpenFileNames(this, tr("Add mods"), QString(), "Mods (*.jar)");
        for (auto& f : files)
            QFile::copy(f, QDir(dir()).filePath("mods/" + QFileInfo(f).fileName()));
        loadMods();
    });
    connect(disMod, &QPushButton::clicked, this, [this] {
        auto* it = m_mods->currentItem();
        if (!it)
            return;
        QDir d(QDir(dir()).filePath("mods"));
        QString n = it->data(Qt::UserRole).toString();
        QString to = n.endsWith(".disabled") ? n.chopped(9) : n + ".disabled";
        d.rename(n, to);
        loadMods();
    });
    connect(m_timer, &QTimer::timeout, this, &ServerPanelDialog::refresh);

    loadServers();
    m_timer->start();
}

QString ServerPanelDialog::dir() const
{
    int i = m_servers->currentIndex();
    return i < 0 ? QString() : m_list[i].toObject()["path"].toString();
}

void ServerPanelDialog::loadServers()
{
    QSettings s;
    m_list = QJsonDocument::fromJson(s.value("ServerPanels/list").toByteArray()).array();
    if (m_list.isEmpty()) {
        QJsonObject o{ { "name", "Minecraft server" }, { "path", "C:\\MinecraftServer" }, { "port", 25565 } };
        m_list.append(o);
    }
    m_servers->blockSignals(true);
    m_servers->clear();
    for (auto v : m_list)
        m_servers->addItem(v.toObject()["name"].toString());
    m_servers->blockSignals(false);
    serverChanged();
}

void ServerPanelDialog::saveServers()
{
    QSettings s;
    s.setValue("ServerPanels/list", QJsonDocument(m_list).toJson(QJsonDocument::Compact));
}

void ServerPanelDialog::addServer()
{
    QString p = QFileDialog::getExistingDirectory(this, tr("Choose the server folder"));
    if (p.isEmpty())
        return;
    bool ok;
    QString name = QInputDialog::getText(this, tr("Add server"), tr("Name:"), QLineEdit::Normal, QDir(p).dirName(), &ok);
    if (!ok || name.isEmpty())
        return;
    m_list.append(QJsonObject{ { "name", name }, { "path", p }, { "port", 25565 } });
    saveServers();
    loadServers();
    m_servers->setCurrentIndex(m_list.size() - 1);
}

void ServerPanelDialog::removeServer()
{
    int i = m_servers->currentIndex();
    if (i < 0 || QMessageBox::question(this, tr("Remove"), tr("Remove this panel? The server files are not touched.")) != QMessageBox::Yes)
        return;
    m_list.removeAt(i);
    saveServers();
    loadServers();
}

void ServerPanelDialog::serverChanged()
{
    m_console->clear();
    m_logPos = 0;
    m_logPath = QDir(dir()).filePath("logs/latest.log");
    loadProperties();
    loadMods();
    loadBackups();
    refresh();
}

bool ServerPanelDialog::isOnline() const
{
    int i = m_servers->currentIndex();
    if (i < 0)
        return false;
    QTcpSocket s;
    s.connectToHost("127.0.0.1", m_list[i].toObject()["port"].toInt(25565));
    return s.waitForConnected(150);
}

void ServerPanelDialog::refresh()
{
    bool on = isOnline();
    m_status->setText(on ? tr("<b style='color:#2ecc71'>● Online</b>") : tr("<b style='color:#e74c3c'>● Offline</b>"));
    tailLog();
    if (++m_counter % 10 == 0)
        loadBackups();
}

void ServerPanelDialog::tailLog()
{
    QFile f(m_logPath);
    if (!f.open(QIODevice::ReadOnly))
        return;
    if (f.size() < m_logPos)  // rotated on restart
        m_logPos = 0;
    if (m_logPos == 0 && f.size() > 60000)
        m_logPos = f.size() - 60000;
    f.seek(m_logPos);
    QByteArray chunk = f.readAll();
    m_logPos = f.pos();
    if (!chunk.isEmpty()) {
        auto* sb = m_console->verticalScrollBar();
        bool bottom = sb->value() >= sb->maximum() - 4;
        m_console->appendPlainText(QString::fromUtf8(chunk).trimmed());
        if (bottom)
            sb->setValue(sb->maximum());
    }
}

QString ServerPanelDialog::runCommand(const QString& cmd)
{
    // ShalevGuard runs every line of a .txt dropped in shalevguard\inbox as the server console.
    QDir d(QDir(dir()).filePath("shalevguard/inbox"));
    if (!d.exists()) {
        QMessageBox::warning(this, tr("Servers"), tr("This server has no shalevguard/inbox folder, so commands can't be sent."));
        return {};
    }
    QString name = QString("panel-%1.txt").arg(QDateTime::currentMSecsSinceEpoch());
    QFile f(d.filePath(name + ".tmp"));
    if (f.open(QIODevice::WriteOnly)) {
        f.write(cmd.toUtf8() + "\n");
        f.close();
        f.rename(d.filePath(name));  // rename so it's never read half-written
    }
    return name;
}

void ServerPanelDialog::sendCommand()
{
    QString c = m_cmd->text().trimmed();
    if (c.startsWith('/'))
        c.remove(0, 1);
    if (c.isEmpty())
        return;
    m_console->appendPlainText("> " + c);
    runCommand(c);
    m_cmd->clear();
}

void ServerPanelDialog::startServer()
{
    if (isOnline()) {
        QMessageBox::information(this, tr("Servers"), tr("The server is already running."));
        return;
    }
    // start.bat is the only correct launcher (Fabric, 8 GB). Detached so it keeps running if this launcher closes.
    QProcess p;
#ifdef Q_OS_WIN
    p.setProgram("cmd.exe");
    // Native arguments: Qt's normal quoting breaks `start "title"`.
    p.setNativeArguments("/c start \"MCServer\" /min start.bat");
#else
    p.setProgram("sh");
    p.setArguments({ "-c", "./start.sh" });
#endif
    p.setWorkingDirectory(dir());
    p.startDetached();
}

void ServerPanelDialog::stopServer()
{
    if (QMessageBox::question(this, tr("Stop"), tr("Stop the server? Players will be disconnected.")) == QMessageBox::Yes)
        runCommand("stop");
}

void ServerPanelDialog::restartServer()
{
    if (QMessageBox::question(this, tr("Restart"), tr("Restart the server? Players will be disconnected.")) != QMessageBox::Yes)
        return;
    runCommand("stop");
    auto* t = new QTimer(this);
    t->setInterval(2000);
    int tries = 0;
    connect(t, &QTimer::timeout, this, [this, t, tries]() mutable {
        if (!isOnline() || ++tries > 45) {
            t->stop();
            t->deleteLater();
            if (tries <= 45)
                startServer();
        }
    });
    t->start();
}

void ServerPanelDialog::loadProperties()
{
    m_props->setRowCount(0);
    QFile f(QDir(dir()).filePath("server.properties"));
    if (!f.open(QIODevice::ReadOnly))
        return;
    for (auto line : QString::fromUtf8(f.readAll()).split('\n')) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith('#'))
            continue;
        int eq = line.indexOf('=');
        if (eq < 0)
            continue;
        int r = m_props->rowCount();
        m_props->insertRow(r);
        auto* k = new QTableWidgetItem(line.left(eq));
        k->setFlags(k->flags() & ~Qt::ItemIsEditable);
        m_props->setItem(r, 0, k);
        m_props->setItem(r, 1, new QTableWidgetItem(line.mid(eq + 1)));
    }
}

void ServerPanelDialog::saveProperties()
{
    QString path = QDir(dir()).filePath("server.properties");
    QFile::remove(path + ".panel-bak");
    QFile::copy(path, path + ".panel-bak");  // one-step undo
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write("#Minecraft server properties (saved by Servers panel)\n");
    for (int r = 0; r < m_props->rowCount(); ++r)
        f.write((m_props->item(r, 0)->text() + "=" + m_props->item(r, 1)->text() + "\n").toUtf8());
    QMessageBox::information(this, tr("Servers"), tr("Saved. Restart the server to apply. Previous copy: server.properties.panel-bak"));
}

void ServerPanelDialog::loadMods()
{
    m_mods->clear();
    QDir d(QDir(dir()).filePath("mods"));
    for (auto& fi : d.entryInfoList({ "*.jar", "*.jar.disabled" }, QDir::Files, QDir::Name)) {
        bool off = fi.fileName().endsWith(".disabled");
        auto* it = new QListWidgetItem((off ? "○ " : "● ") + fi.fileName().remove(".disabled") + QString(" (%1 KB)").arg(fi.size() / 1024));
        it->setData(Qt::UserRole, fi.fileName());
        if (off)
            it->setForeground(Qt::gray);
        m_mods->addItem(it);
    }
}

void ServerPanelDialog::loadBackups()
{
    m_backups->clear();
    QDir b(QDir(dir()).filePath("backups"));
    for (auto sub : { "new-player", "daily", "manual" })
        for (auto& fi : QDir(b.filePath(sub)).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time))
            m_backups->addItem(QString("[%1] %2").arg(sub, fi.fileName()));
}

void ServerPanelDialog::openFolder(const QString& sub)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(sub.isEmpty() ? dir() : QDir(dir()).filePath(sub)));
}

void ServerPanelDialog::backupNow()
{
    runCommand("guard backup now");
    runCommand("backup local");  // Fastback snapshot
}
