// SPDX-License-Identifier: Apache-2.0
#include "window.h"
#include "ssh_dialogs.h"
#include <QtTest>

class SshAuthTests : public QObject
{
    Q_OBJECT
private:
    static QByteArray fileBytes(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) { return {}; }
        return file.readAll();
    }

    static void writeFile(const QString &path, const QByteArray &data)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(data), data.size());
    }

    static void startHelper(QProcess &helper, sw::SshAuthSession &session, const QString &prompt)
    {
        auto environment = session.environment();
        auto executable = qEnvironmentVariable("SW_SSH_TEST_ASKPASS");
        if (executable.isEmpty())
        {
            executable = QCoreApplication::applicationDirPath() + "/SerialWorkbenchSSHAskpass.exe";
        }
        else
        {
            const auto windows = qEnvironmentVariable("SystemRoot");
            environment.insert("PATH", windows + "/System32;" + windows);
        }
        helper.setProcessEnvironment(environment);
        helper.start(executable, {prompt});
    }

private slots:
    void encryptedCacheAndTargetedRemoval()
    {
        QTemporaryDir directory;
        const auto root = directory.path();
        sw::SshCredentialCache::write(root, "first-host", "account-1", "test-secret-42");
        sw::SshCredentialCache::write(root, "first-host", "account-2", "other-secret");
        sw::SshCredentialCache::write(root, "second-host", "account-1", "second-host-secret");
        QCOMPARE(sw::SshCredentialCache::read(root, "FIRST-HOST", "account-1"), QString("test-secret-42"));
        for (const auto &file : QDir(root).entryList({"*.json"}, QDir::Files))
        {
            QVERIFY(!fileBytes(QDir(root).filePath(file)).contains("secret"));
        }
        sw::SshCredentialCache::remove(root, "first-host", "account-1");
        QVERIFY(sw::SshCredentialCache::read(root, "first-host", "account-1").isEmpty());
        QCOMPARE(sw::SshCredentialCache::read(root, "first-host", "account-2"), QString("other-secret"));
        sw::SshCredentialCache::remove(root, "first-host");
        QVERIFY(sw::SshCredentialCache::read(root, "first-host", "account-2").isEmpty());
        QCOMPARE(sw::SshCredentialCache::read(root, "second-host", "account-1"), QString("second-host-secret"));
    }

    void knownHostsPreservesOtherDevicesAndBacksUp()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath("known_hosts");
        const QByteArray salt("test-salt-for-host");
        const auto hashed = "|1|" + salt.toBase64() + "|"
            + QMessageAuthenticationCode::hash("[device]:2222", salt, QCryptographicHash::Sha1).toBase64();
        const auto original = QByteArray("# keep comment\r\nother ssh-ed25519 OTHER\r\n")
            + "device,shared ssh-ed25519 SHARED\r\n"
            + "[device]:22 ssh-ed25519 STANDARD\r\n"
            + hashed + " ssh-ed25519 SECRET\r\n";
        writeFile(path, original);
        QCOMPARE(sw::SshHostCache::remove(path, {"device", "[device]:2222"}), 2);
        const auto remaining = fileBytes(path);
        QVERIFY(remaining.contains("other ssh-ed25519 OTHER\r\n"));
        QVERIFY(remaining.contains("shared ssh-ed25519 SHARED\r\n"));
        QVERIFY(remaining.contains("[device]:22 ssh-ed25519 STANDARD\r\n"));
        QVERIFY(!remaining.contains(hashed));
        const auto backups = QDir(directory.path()).entryList({"*.bak"}, QDir::Files);
        QCOMPARE(backups.size(), 1);
        QCOMPARE(fileBytes(directory.filePath(backups.first())), original);
        QCOMPARE(sw::SshHostCache::remove(path, {"no-such-host"}), 0);
        QCOMPARE(fileBytes(path), remaining);
    }

    void effectiveConfigSupportsAliasesPortsAndPaths()
    {
        QTemporaryDir directory;
        const auto file = directory.filePath("known hosts");
        const auto dump = QString("hostname device.example\nport 2222\nuser test\nuserknownhostsfile \"%1\" none\n").arg(file).toUtf8();
        const auto records = sw::SshHostCache::parse(dump, "lab-alias");
        QCOMPARE(records.hostname, QString("device.example"));
        QCOMPARE(records.identifiers, QStringList {"[device.example]:2222"});
        QCOMPARE(records.files, QStringList {file});
        const auto second = directory.filePath("another known hosts");
        const auto unquoted = QString("hostname device.example\nuserknownhostsfile %1 %2\n").arg(file, second).toUtf8();
        QCOMPARE(sw::SshHostCache::parse(unquoted, "lab-alias").files, QStringList({file, second}));
        QCOMPARE(sw::SshHostCache::parse(dump + "hostkeyalias verified-device\n", "lab-alias").identifiers,
            QStringList {"verified-device"});
    }

    void askpassCachesAndRepromptsAfterFailure()
    {
        QTemporaryDir directory;
        sw::Config config;
        config.kind = "ssh";
        config.sshHost = "askpass-test";
        const QString message = "test@askpass-test's password:";
        const auto key = config.key() + '\n' + message;
        sw::SshCredentialCache::write(directory.path(), config.sshHost, key, "old-password");
        sw::SshAuthSession session(config, directory.path());
        int requests = 0;
        connect(&session, &sw::SshAuthSession::promptRequested, this, [&](sw::SshAuthPrompt *prompt)
        {
            ++requests;
            QVERIFY(prompt->retry);
            prompt->answer("new-password", true);
        });
        QProcess first;
        startHelper(first, session, message);
        QSignalSpy firstDone(&first, &QProcess::finished);
        QVERIFY(firstDone.wait(3000));
        QCOMPARE(first.exitCode(), 0);
        QCOMPARE(first.readAllStandardOutput().trimmed(), QByteArray("old-password"));
        QCOMPARE(requests, 0);
        QProcess retry;
        startHelper(retry, session, message);
        QSignalSpy retryDone(&retry, &QProcess::finished);
        QVERIFY(retryDone.wait(3000));
        QCOMPARE(retry.exitCode(), 0);
        QCOMPARE(retry.readAllStandardOutput().trimmed(), QByteArray("new-password"));
        QCOMPARE(requests, 1);
        QCOMPARE(sw::SshCredentialCache::read(directory.path(), config.sshHost, key), QString("new-password"));
    }

    void dialogsConfirmAndMaskPassword_data()
    {
        QTest::addColumn<bool>("confirmation");
        QTest::newRow("fingerprint") << true;
        QTest::newRow("password") << false;
    }

    void dialogsConfirmAndMaskPassword()
    {
        QFETCH(bool, confirmation);
        QTemporaryDir directory;
        sw::Config config;
        config.kind = "ssh";
        config.sshHost = "askpass-test";
        sw::SshAuthSession session(config, directory.path());
        QPointer<QDialog> dialog;
        QWidget parent;
        connect(&session, &sw::SshAuthSession::promptRequested, this, [&](sw::SshAuthPrompt *prompt)
        {
            QCOMPARE(prompt->confirmation, confirmation);
            sw::showSshAuthPrompt(prompt, &parent, sw::Theme::make(false));
            dialog = parent.findChild<QDialog *>("sshAuthenticationDialog");
        });
        QProcess helper;
        startHelper(helper, session, confirmation
            ? "The authenticity of host cannot be established. SHA256:test. Continue (yes/no/[fingerprint])?"
            : "test@askpass-test's password:");
        QSignalSpy done(&helper, &QProcess::finished);
        QTRY_VERIFY(dialog);
        auto input = dialog->findChild<QLineEdit *>("sshPassword");
        auto remember = dialog->findChild<QCheckBox *>("sshRememberPassword");
        QVERIFY(input && remember);
        QCOMPARE(input->echoMode(), QLineEdit::Password);
        QVERIFY(!remember->isChecked());
        if (!confirmation) { input->setText("test-secret-42"); }
        const auto capture = qEnvironmentVariable("SW_SSH_AUTH_CAPTURE_DIR");
        if (!capture.isEmpty())
        {
            dialog->grab().save(QDir(capture).filePath(confirmation ? "ssh-fingerprint.png" : "ssh-password.png"));
        }
        auto buttons = dialog->findChild<QDialogButtonBox *>();
        QVERIFY(buttons);
        QTest::mouseClick(buttons->button(QDialogButtonBox::Ok), Qt::LeftButton);
        QVERIFY(done.wait(3000));
        QCOMPARE(helper.exitCode(), 0);
        QCOMPARE(helper.readAllStandardOutput().trimmed(), confirmation ? QByteArray("yes") : QByteArray("test-secret-42"));
    }

    void passwordNeverEntersSharedHistoryAndCancelStopsHelper()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        engine.sshCacheDirectory = directory.path();
        engine.sshExecutable = QCoreApplication::applicationDirPath() + "/fake_ssh.exe";
        sw::Config config;
        config.kind = "ssh";
        config.sshHost = "askpass-test";
        connect(&engine, &sw::Engine::sshPromptRequested, this, [](sw::SshAuthPrompt *prompt)
        {
            prompt->answer("test-secret-42");
        });
        auto channel = engine.open(config);
        QTRY_COMPARE_WITH_TIMEOUT(channel->state, QString("connected"), 4000);
        sw::McpServer mcp(&engine);
        const auto events = mcp.call("read_events", {{"channel_id", channel->id}}).toObject()["events"].toArray();
        QVERIFY(!events.isEmpty());
        const auto serialized = QJsonDocument(events).toJson();
        QVERIFY(!serialized.contains("test-secret-42"));
        QVERIFY(!serialized.contains(QByteArray("test-secret-42").toHex(' ')));
        channel->close();

        sw::SshAuthSession session(config, directory.path());
        QPointer<sw::SshAuthPrompt> pending;
        connect(&session, &sw::SshAuthSession::promptRequested, this, [&](sw::SshAuthPrompt *prompt) { pending = prompt; });
        QProcess helper;
        startHelper(helper, session, "test@askpass-test's password:");
        QSignalSpy done(&helper, &QProcess::finished);
        QTRY_VERIFY(pending);
        session.stop();
        QVERIFY(done.wait(3000));
        QVERIFY(helper.exitCode() != 0);
        QVERIFY(helper.readAllStandardOutput().isEmpty());
    }

    void cancelledPromptStopsConnection()
    {
        QTemporaryDir directory;
        sw::Engine engine;
        engine.sshCacheDirectory = directory.path();
        engine.sshExecutable = QCoreApplication::applicationDirPath() + "/fake_ssh.exe";
        sw::Config config;
        config.kind = "ssh";
        config.sshHost = "askpass-test";
        int prompts = 0;
        connect(&engine, &sw::Engine::sshPromptRequested, this, [&](sw::SshAuthPrompt *prompt)
        {
            ++prompts;
            prompt->cancel();
        });
        auto channel = engine.open(config);
        QTRY_COMPARE_WITH_TIMEOUT(channel->state, QString("closed"), 4000);
        QCOMPARE(prompts, 1);
    }

    void clearCacheDialogOnlyResetsCurrentHost()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath("known_hosts");
        const QByteArray original("test-host ssh-ed25519 TARGET\nother-host ssh-ed25519 OTHER\n");
        writeFile(path, original);
        const auto previous = qgetenv("SW_SSH_TEST_KNOWN_HOSTS");
        qputenv("SW_SSH_TEST_KNOWN_HOSTS", path.toUtf8());
        const auto restore = qScopeGuard([previous]
        {
            if (previous.isEmpty()) { qunsetenv("SW_SSH_TEST_KNOWN_HOSTS"); }
            else { qputenv("SW_SSH_TEST_KNOWN_HOSTS", previous); }
        });
        sw::Engine engine;
        engine.sshExecutable = QCoreApplication::applicationDirPath() + "/fake_ssh.exe";
        engine.sshCacheDirectory = directory.path();
        sw::Config config;
        config.kind = "ssh";
        config.sshHost = "cache-target";
        sw::SshCredentialCache::write(directory.path(), config.sshHost, "test", "test-secret-42");
        sw::SshCredentialCache::write(directory.path(), "other-host", "test", "other-secret");
        auto target = new sw::Channel(&engine, config);
        engine.channels.insert(target->id, target);
        target->status("connected");
        auto otherConfig = config;
        otherConfig.sshHost = "other-host";
        auto other = new sw::Channel(&engine, otherConfig);
        engine.channels.insert(other->id, other);
        other->status("connected");
        QWidget parent;
        bool sawDialog = false;
        QTimer::singleShot(0, &parent, [&]
        {
            auto dialog = parent.findChild<QDialog *>("sshCacheDialog");
            QVERIFY(dialog);
            sawDialog = true;
            QVERIFY(dialog->findChild<QCheckBox *>("sshClearPasswords")->isChecked());
            QVERIFY(dialog->findChild<QCheckBox *>("sshClearFingerprints")->isChecked());
            const auto capture = qEnvironmentVariable("SW_SSH_AUTH_CAPTURE_DIR");
            if (!capture.isEmpty()) { dialog->grab().save(QDir(capture).filePath("ssh-cache.png")); }
            dialog->accept();
            QTimer::singleShot(50, &parent, [&parent]
            {
                for (auto box : parent.findChildren<QMessageBox *>()) { box->accept(); }
            });
        });
        sw::clearSshCache(config, &engine, &parent, sw::Theme::make(false));
        QVERIFY(sawDialog);
        QCOMPARE(target->state, QString("closed"));
        QCOMPARE(other->state, QString("connected"));
        QVERIFY(sw::SshCredentialCache::read(directory.path(), config.sshHost, "test").isEmpty());
        QCOMPARE(sw::SshCredentialCache::read(directory.path(), "other-host", "test"), QString("other-secret"));
        QCOMPARE(fileBytes(path), QByteArray("other-host ssh-ed25519 OTHER\n"));
    }

    void realOpenSshPasswordAndChangedHostKey()
    {
        const auto port = qEnvironmentVariable("SW_SSH_LOCAL_TEST_PORT");
        if (port.isEmpty()) { QSKIP("Set SW_SSH_LOCAL_TEST_PORT to the loopback SSH fixture port"); }
        QTemporaryDir directory;
        const auto hosts = directory.filePath("known_hosts");
        const auto helperDirectory = directory.filePath("helper with spaces");
        QVERIFY(QDir().mkpath(helperDirectory));
        const auto helper = QDir(helperDirectory).filePath("SerialWorkbenchSSHAskpass.exe");
        QVERIFY(QFile::copy(QCoreApplication::applicationDirPath() + "/SerialWorkbenchSSHAskpass.exe", helper));
        sw::Config config;
        config.kind = "ssh";
        config.sshHost = "127.0.0.1";
        config.sshUser = "test";
        config.sshPort = port.toInt();
        const QStringList arguments {"-tt", "-F", "NUL", "-o", "GlobalKnownHostsFile=NUL",
            "-o", "UserKnownHostsFile=" + hosts, "-o", "HashKnownHosts=yes",
            "-o", "StrictHostKeyChecking=ask", "-o", "PreferredAuthentications=password",
            "-o", "PubkeyAuthentication=no", "-o", "ConnectTimeout=5",
            "-l", "test", "-p", port, "127.0.0.1"};
        for (int phase = 0; phase < 4; ++phase)
        {
            if (phase == 3)
            {
                QCOMPARE(sw::SshHostCache::remove(hosts, {"[127.0.0.1]:" + port}), 1);
                sw::SshCredentialCache::remove(directory.path(), config.sshHost);
            }
            sw::SshAuthSession session(config, directory.path());
            sw::SshProcess process;
            QByteArray output;
            int confirmations = 0;
            int passwords = 0;
            connect(&process, &sw::SshProcess::output, this, [&](const QByteArray &data) { output += data; });
            connect(&session, &sw::SshAuthSession::promptRequested, this, [&](sw::SshAuthPrompt *prompt)
            {
                if (prompt->confirmation)
                {
                    ++confirmations;
                    prompt->answer("yes");
                }
                else
                {
                    ++passwords;
                    prompt->answer(phase == 0 && passwords == 1 ? "wrong-password" : "test-secret-42", true);
                }
            });
            QSignalSpy ended(&process, &sw::SshProcess::finished);
            auto environment = session.environment();
            environment.insert("SSH_ASKPASS", QDir::toNativeSeparators(helper));
            QVERIFY(process.start("ssh", arguments, {100, 30}, environment));
            if (phase == 2)
            {
                QVERIFY(ended.wait(8000));
                QVERIFY(ended.first().first().toInt() != 0);
                QVERIFY(output.contains("REMOTE HOST IDENTIFICATION HAS CHANGED"));
                QCOMPARE(confirmations, 0);
                QCOMPARE(passwords, 0);
            }
            else
            {
                QTRY_VERIFY_WITH_TIMEOUT(output.contains("SSH_TEST_AUTHENTICATED"), 10000);
                QCOMPARE(confirmations, phase == 1 ? 0 : 1);
                QCOMPARE(passwords, phase == 0 ? 2 : phase == 1 ? 0 : 1);
                QVERIFY(!output.contains("test-secret-42"));
                QVERIFY(!output.contains("wrong-password"));
            }
            process.stop();
            session.stop();
        }
    }
};

QTEST_MAIN(SshAuthTests)
#include "ssh_auth_tests.moc"
