// SPDX-License-Identifier: Apache-2.0
// Test-only UI: never installed or copied into release payloads.
#include "terminal.h"
#include "ssh_dialogs.h"

class SshDemo : public QWidget
{
public:
    SshDemo(const QString &root, const QString &python, const QString &dependencies,
        bool automatic, const QString &report, const QString &screenshot, const QString &readyFile)
        : directory(QDir(root).filePath("build/ssh-device-demo-XXXXXX")), automatic(automatic),
          report(report), screenshot(screenshot), readyFile(readyFile)
    {
        if (!directory.isValid()) { sw::fail("无法创建隔离测试目录"); }
        setWindowTitle("SSH 新设备 Demo · 仅开发测试");
        setObjectName("root");
        setWindowIcon(QIcon(":/brand/lightning.svg"));
        setStyleSheet(theme.stylesheet());
        setMinimumSize(800, 650);
        resize(1080, 800);
        auto body = new QVBoxLayout(this);
        body->setContentsMargins(20, 18, 20, 18);
        body->setSpacing(12);
        auto title = new QLabel("SSH 新设备测试台");
        title->setStyleSheet("font-size: 22px; font-weight: 600;");
        body->addWidget(title);
        auto scope = new QLabel("真实 OpenSSH 连接本机虚拟设备；独立的指纹和密码缓存，不修改个人 SSH 配置。");
        scope->setWordWrap(true);
        body->addWidget(scope);
        auto info = new QGridLayout();
        endpoint = field("服务启动中");
        password = field("等待测试密码");
        fingerprint = field("等待设备指纹");
        info->addWidget(new QLabel("连接地址"), 0, 0);
        info->addWidget(endpoint, 0, 1);
        info->addWidget(new QLabel("测试密码"), 0, 2);
        info->addWidget(password, 0, 3);
        info->addWidget(new QLabel("设备指纹"), 1, 0);
        info->addWidget(fingerprint, 1, 1, 1, 3);
        info->setColumnStretch(1, 1);
        info->setColumnStretch(3, 1);
        body->addLayout(info);
        auto actions = new QHBoxLayout();
        connectButton = button("连接测试设备");
        connectButton->setObjectName("primary");
        auto disconnect = button("断开");
        replaceKey = button("更换设备指纹");
        changePassword = button("修改设备密码");
        auto clear = button("清除测试缓存");
        auto open = button("打开隔离目录");
        for (auto widget : {connectButton, disconnect, replaceKey, changePassword, clear, open}) { actions->addWidget(widget); }
        body->addLayout(actions);
        auto row = new QHBoxLayout();
        led = new sw::StatusLed();
        led->theme = theme;
        status = new QLabel("正在启动本机测试设备…");
        row->addWidget(led);
        row->addWidget(status, 1);
        body->addLayout(row);
        auto guide = new QLabel("① 首次连接：确认指纹、输入密码　② 记住密码后重连　③ 更换指纹，检查拒绝连接，再清缓存重连　④ 修改密码，检查重新询问");
        guide->setWordWrap(true);
        body->addWidget(guide);
        terminal = new sw::TerminalView();
        terminal->theme = theme;
        terminal->setFont(sw::Theme::defaultDataFont());
        terminal->setStyleSheet(QString("font-family: \"%1\"; font-size: 12pt;").arg(sw::Theme::defaultDataFont().family()));
        terminal->setMinimumHeight(230);
        body->addWidget(terminal, 1);
        events = new QPlainTextEdit();
        events->setReadOnly(true);
        events->setMaximumBlockCount(200);
        events->setMaximumHeight(115);
        body->addWidget(events);
        connect(connectButton, &QPushButton::clicked, this, [this] { connectDevice(); });
        connect(disconnect, &QPushButton::clicked, this, [this] { disconnectDevice(); });
        connect(replaceKey, &QPushButton::clicked, this, [this] { command("replace_key"); });
        connect(changePassword, &QPushButton::clicked, this, [this] { command("change_password"); });
        connect(clear, &QPushButton::clicked, this, [this] { clearDialog(); });
        connect(open, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(directory.path())); });
        connect(terminal, &sw::TerminalView::transmit, this, [this](const QByteArray &bytes)
        {
            if (process) { process->write(bytes); }
        });
        connect(terminal, &sw::TerminalView::gridSizeChanged, this, [this](const QSize &size)
        {
            if (process) { process->resize(size); }
        });
        connect(&server, &QProcess::readyReadStandardOutput, this, [this]
        {
            serverOutput += server.readAllStandardOutput();
            while (serverOutput.contains('\n'))
            {
                const int end = serverOutput.indexOf('\n');
                const auto message = QJsonDocument::fromJson(serverOutput.left(end)).object();
                serverOutput.remove(0, end + 1);
                serverEvent(message);
            }
        });
        connect(&server, &QProcess::readyReadStandardError, this, [this]
        {
            const auto message = QString::fromUtf8(server.readAllStandardError()).trimmed();
            if (!message.isEmpty()) { events->appendPlainText(message); }
        });
        connect(&server, &QProcess::errorOccurred, this, [this] { fail("测试服务无法启动：" + server.errorString()); });
        connect(&server, &QProcess::finished, this, [this](int)
        {
            if (!closing) { fail("测试服务已退出，请检查开发依赖"); }
        });
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert("PYTHONPATH", dependencies);
        server.setProcessEnvironment(environment);
        server.start(python, {QDir(root).filePath("tests/demo/ssh_new_device_server.py")});
        connectButton->setEnabled(false);
        replaceKey->setEnabled(false);
        changePassword->setEnabled(false);
        if (automatic) { QTimer::singleShot(35000, this, [this] { fail("自动验证超时"); }); }
    }

    ~SshDemo() override
    {
        closing = true;
        disconnectDevice();
        server.disconnect(this);
        server.kill();
        server.waitForFinished(1000);
    }

private:
    QTemporaryDir directory;
    bool automatic;
    QString report;
    QString screenshot;
    QString readyFile;
    sw::Theme theme = sw::Theme::make(false);
    QProcess server;
    QByteArray serverOutput;
    QPointer<sw::SshProcess> process;
    QPointer<sw::SshAuthSession> authentication;
    sw::TerminalView *terminal;
    sw::StatusLed *led;
    QLabel *status;
    QLineEdit *endpoint;
    QLineEdit *password;
    QLineEdit *fingerprint;
    QPushButton *connectButton;
    QPushButton *replaceKey;
    QPushButton *changePassword;
    QPlainTextEdit *events;
    int port = 0;
    int generation = 0;
    int phase = 1;
    int confirmations = 0;
    int passwordPrompts = 0;
    bool online = false;
    bool closing = false;
    bool finished = false;
    QByteArray received;
    QJsonArray checks;

    static QLineEdit *field(const QString &text)
    {
        auto result = new QLineEdit(text);
        result->setReadOnly(true);
        return result;
    }

    static QPushButton *button(const QString &text)
    {
        auto result = new QPushButton(text);
        result->setProperty("variant", "outline");
        result->setCursor(Qt::PointingHandCursor);
        return result;
    }

    QString hostsPath() const { return directory.filePath("known_hosts"); }
    QString cachePath() const { return directory.filePath("password-cache"); }

    void fail(const QString &message)
    {
        if (finished || closing) { return; }
        status->setText(message);
        led->setState(sw::StatusLed::State::Error);
        events->appendPlainText(message);
        if (automatic)
        {
            finished = true;
            writeReport({{"passed", false}, {"phase", phase}, {"error", message}, {"checks", checks}});
            QTimer::singleShot(0, this, [] { QCoreApplication::exit(1); });
        }
    }

    void writeReport(const QJsonObject &object)
    {
        if (report.isEmpty()) { return; }
        QSaveFile file(report);
        if (file.open(QIODevice::WriteOnly))
        {
            file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
            file.commit();
        }
    }

    void serverEvent(const QJsonObject &message)
    {
        const auto kind = message["type"].toString();
        if (kind == "ready" || kind == "key_changed" || kind == "password_changed")
        {
            port = message["port"].toInt();
            endpoint->setText(QString("demo@127.0.0.1:%1").arg(port));
            password->setText(message["password"].toString());
            fingerprint->setText(message["fingerprint"].toString());
            connectButton->setEnabled(true);
            replaceKey->setEnabled(true);
            changePassword->setEnabled(true);
            status->setText(kind == "ready" ? "测试设备已就绪，点击连接开始认证。"
                : kind == "key_changed" ? "设备指纹已变更；直接重连应被拒绝，清除测试缓存后可重新确认。"
                                        : "设备密码已修改；重连后请输入上方的新密码。");
            events->appendPlainText(status->text());
            if (!readyFile.isEmpty() && kind == "ready")
            {
                QFile file(readyFile);
                if (file.open(QIODevice::WriteOnly))
                {
                    file.write(QJsonDocument(QJsonObject {{"port", port}, {"user", "demo"},
                        {"password", password->text()}, {"isolated_directory", directory.path()}}).toJson());
                }
            }
            if (automatic) { QTimer::singleShot(50, this, [this] { connectDevice(); }); }
        }
        else if (kind == "authentication")
        {
            events->appendPlainText(message["accepted"].toBool() ? "设备端：密码认证通过" : "设备端：密码被拒绝，等待重新输入");
        }
        else if (kind == "error") { fail(message["message"].toString()); }
    }

    void disconnectDevice()
    {
        ++generation;
        online = false;
        terminal->enabledInput = false;
        if (authentication) { authentication->stop(); }
        if (process) { process->stop(); process->deleteLater(); }
        if (authentication) { authentication->deleteLater(); }
        process = nullptr;
        authentication = nullptr;
        led->setState(sw::StatusLed::State::Offline);
        status->setText("已断开测试连接");
    }

    void connectDevice()
    {
        if (!port || finished) { return; }
        disconnectDevice();
        const int current = generation;
        received.clear();
        confirmations = 0;
        passwordPrompts = 0;
        terminal->reset();
        sw::Config config;
        config.kind = "ssh";
        config.sshHost = "127.0.0.1";
        config.sshUser = "demo";
        config.sshPort = port;
        try
        {
            authentication = new sw::SshAuthSession(config, cachePath(), this);
            process = new sw::SshProcess(this);
            connect(authentication, &sw::SshAuthSession::cancelled, this, [this]
            {
                QTimer::singleShot(0, this, [this] { disconnectDevice(); });
            });
            connect(authentication, &sw::SshAuthSession::promptRequested, this, [this](sw::SshAuthPrompt *prompt)
            {
                if (prompt->confirmation) { ++confirmations; }
                else { ++passwordPrompts; }
                sw::showSshAuthPrompt(prompt, this, theme);
                if (automatic)
                {
                    QTimer::singleShot(60, prompt, [this, confirmation = prompt->confirmation]
                    {
                        auto dialog = findChild<QDialog *>("sshAuthenticationDialog");
                        if (!dialog) { fail("认证窗口未出现"); return; }
                        if (!confirmation)
                        {
                            dialog->findChild<QLineEdit *>("sshPassword")->setText(
                                phase == 1 && passwordPrompts == 1 ? "wrong-demo-password" : password->text());
                            dialog->findChild<QCheckBox *>("sshRememberPassword")->setChecked(true);
                        }
                        dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
                    });
                }
            });
            connect(process, &sw::SshProcess::output, this, [this, current](const QByteArray &bytes)
            {
                if (current != generation) { return; }
                terminal->feed(bytes);
                received += bytes;
                if (!online && received.contains("SSH_DEMO_READY"))
                {
                    online = true;
                    terminal->enabledInput = true;
                    terminal->setFocus();
                    led->setState(sw::StatusLed::State::Online);
                    status->setText("已连接本机测试设备，可以输入 help 或 status。");
                    if (automatic) { QTimer::singleShot(100, this, [this] { verifyOnline(); }); }
                }
                if (received.size() > 65536) { received.remove(0, received.size() - 65536); }
            });
            connect(process, &sw::SshProcess::finished, this, [this, current](int code)
            {
                if (current != generation) { return; }
                online = false;
                terminal->enabledInput = false;
                led->setState(code ? sw::StatusLed::State::Error : sw::StatusLed::State::Offline);
                status->setText(code ? "SSH 连接失败；指纹变化时请清除测试缓存后重连。" : "测试会话已结束");
                if (automatic && phase == 3 && code != 0 && received.contains("REMOTE HOST IDENTIFICATION HAS CHANGED"))
                {
                    checks.append(QJsonObject {{"case", "changed_fingerprint_rejected"}, {"passed", true}});
                    clearCache(true, true);
                    phase = 4;
                    QTimer::singleShot(80, this, [this] { connectDevice(); });
                }
                else if (automatic) { fail(QString("阶段 %1 连接提前结束，退出码 %2").arg(phase).arg(code)); }
            });
            const QStringList arguments {"-tt", "-F", "NUL", "-o", "BatchMode=no",
                "-o", "StrictHostKeyChecking=ask", "-o", "GlobalKnownHostsFile=NUL",
                "-o", "UserKnownHostsFile=" + hostsPath(), "-o", "HashKnownHosts=yes",
                "-o", "PreferredAuthentications=password", "-o", "PubkeyAuthentication=no",
                "-o", "ConnectTimeout=5", "-l", "demo", "-p", QString::number(port), "127.0.0.1"};
            led->setState(sw::StatusLed::State::Pending);
            status->setText("正在连接，等待指纹确认或密码认证…");
            if (!process->start("ssh", arguments, {terminal->columns, terminal->rows}, authentication->environment()))
            {
                fail(process->errorString());
            }
        }
        catch (const std::exception &error) { fail(QString::fromUtf8(error.what())); }
    }

    void command(const QString &name)
    {
        disconnectDevice();
        server.write(QJsonDocument(QJsonObject {{"command", name}}).toJson(QJsonDocument::Compact) + '\n');
    }

    void clearCache(bool passwords, bool fingerprints)
    {
        disconnectDevice();
        if (passwords) { sw::SshCredentialCache::remove(cachePath(), "127.0.0.1"); }
        if (fingerprints) { sw::SshHostCache::remove(hostsPath(), {QString("[127.0.0.1]:%1").arg(port)}); }
        events->appendPlainText("已清除所选测试缓存，个人 SSH 文件保持不变。");
    }

    void clearDialog()
    {
        QDialog dialog(this);
        dialog.setWindowTitle("清除隔离的 SSH 测试缓存");
        auto body = new QVBoxLayout(&dialog);
        auto passwords = new QCheckBox("忘记测试设备的密码");
        auto fingerprints = new QCheckBox("清除测试设备的主机指纹");
        passwords->setChecked(true);
        fingerprints->setChecked(true);
        body->addWidget(passwords);
        body->addWidget(fingerprints);
        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        buttons->button(QDialogButtonBox::Ok)->setText("清除");
        buttons->button(QDialogButtonBox::Cancel)->setText("取消");
        body->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() == QDialog::Accepted)
        {
            try { clearCache(passwords->isChecked(), fingerprints->isChecked()); }
            catch (const std::exception &error) { fail(QString::fromUtf8(error.what())); }
        }
    }

    void verifyOnline()
    {
        bool passed = phase == 1 ? confirmations == 1 && passwordPrompts == 2
            : phase == 2 ? confirmations == 0 && passwordPrompts == 0
            : phase == 4 ? confirmations == 1 && passwordPrompts == 1
                         : confirmations == 0 && passwordPrompts == 1;
        passed = passed && !received.contains(password->text().toUtf8()) && !received.contains("wrong-demo-password");
        const QString name = phase == 1 ? "first_login_and_password_retry" : phase == 2 ? "remembered_password"
            : phase == 4 ? "reconfigure_after_clear" : "changed_password_reprompt";
        checks.append(QJsonObject {{"case", name}, {"passed", passed}});
        if (!passed) { fail("认证交互次数或密码隔离验证失败：" + name); return; }
        if (phase == 1) { phase = 2; connectDevice(); }
        else if (phase == 2) { phase = 3; command("replace_key"); }
        else if (phase == 4) { phase = 5; command("change_password"); }
        else
        {
            process->write("status\recho demo-roundtrip-ok\r");
            auto polling = new QTimer(this);
            auto elapsed = std::make_shared<QElapsedTimer>();
            elapsed->start();
            connect(polling, &QTimer::timeout, this, [this, polling, elapsed]
            {
                const auto text = terminal->text();
                bool echoed = false;
                for (const auto &line : text.split('\n')) { echoed = echoed || line.trimmed() == "demo-roundtrip-ok"; }
                if (!text.contains("device=new-device-demo") || !echoed)
                {
                    if (elapsed->elapsed() > 3000) { polling->stop(); fail("终端命令收发验证超时"); }
                    return;
                }
                polling->stop();
                checks.append(QJsonObject {{"case", "terminal_command_roundtrip"}, {"passed", true}});
                status->setText("自动验证通过：认证、密码缓存、设备变更和终端命令收发。");
                events->appendPlainText(status->text());
                finished = true;
                writeReport({{"passed", true}, {"checks", checks}, {"loopback_only", true}, {"isolated_cache", true}});
                if (!screenshot.isEmpty()) { grab().save(screenshot); }
                QTimer::singleShot(80, this, [] { QCoreApplication::exit(0); });
            });
            polling->start(30);
        }
    }
};

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setApplicationName("SerialWorkbenchSshDemo");
    app.setStyle("Fusion");
    app.setFont(sw::Theme::interfaceFont());
    QCommandLineParser options;
    options.addHelpOption();
    options.addOption({"root", "Workspace source directory", "path", WORKBENCH_DEMO_ROOT});
    options.addOption({"python", "Python interpreter with Paramiko", "path"});
    options.addOption({"dependencies", "Developer-only Python dependency directory", "path"});
    options.addOption({"self-test", "Run authentication scenarios automatically"});
    options.addOption({"report", "Write self-test JSON", "path"});
    options.addOption({"screenshot", "Save completed self-test screenshot", "path"});
    options.addOption({"ready-file", "Write demo endpoint and isolation path", "path"});
    options.process(app);
    const auto root = options.value("root");
    try
    {
        SshDemo demo(root, options.isSet("python") ? options.value("python") : QDir(root).filePath(".venv/Scripts/python.exe"),
            options.isSet("dependencies") ? options.value("dependencies") : QDir(root).filePath("build/ssh-test-deps"),
            options.isSet("self-test"), options.value("report"), options.value("screenshot"), options.value("ready-file"));
        demo.show();
        return app.exec();
    }
    catch (const std::exception &error)
    {
        QMessageBox::critical(nullptr, "SSH Demo 无法启动", QString::fromUtf8(error.what()));
        return 1;
    }
}
