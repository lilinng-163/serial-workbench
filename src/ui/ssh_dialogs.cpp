// SPDX-License-Identifier: Apache-2.0
#include "ssh_dialogs.h"

namespace sw
{
void showSshAuthPrompt(SshAuthPrompt *prompt, QWidget *parent, const Theme &theme)
{
    auto dialog = new QDialog(parent);
    dialog->setObjectName("sshAuthenticationDialog");
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(prompt->confirmation ? "确认 SSH 主机指纹" : "SSH 身份验证");
    dialog->setStyleSheet(theme.stylesheet());
    dialog->setMinimumWidth(440);
    dialog->resize(540, 300);
    auto body = new QVBoxLayout(dialog);
    body->setContentsMargins(20, 18, 20, 18);
    body->setSpacing(12);
    auto target = new QLabel("连接主机：" + prompt->host);
    target->setTextFormat(Qt::PlainText);
    target->setWordWrap(true);
    body->addWidget(target);
    auto description = new QLabel(prompt->confirmation
        ? "请核对服务器指纹，确认这是要连接的设备。信任后，OpenSSH 会保存这台主机的指纹。"
        : prompt->retry ? "上次认证未通过，请重新输入。已失效的密码缓存已清除。"
                        : "输入密码以继续连接。也支持私钥口令和交互式认证。");
    description->setWordWrap(true);
    body->addWidget(description);
    auto challenge = new QPlainTextEdit(prompt->message);
    challenge->setObjectName("sshAuthenticationChallenge");
    challenge->setReadOnly(true);
    challenge->setMinimumHeight(80);
    challenge->setMaximumHeight(140);
    body->addWidget(challenge);
    auto password = new QLineEdit();
    password->setObjectName("sshPassword");
    password->setEchoMode(QLineEdit::Password);
    password->setPlaceholderText(prompt->message.contains("passphrase", Qt::CaseInsensitive)
        ? "输入私钥口令" : "输入登录密码或认证响应");
    password->setMaxLength(1023);
    password->setVisible(!prompt->confirmation);
    body->addWidget(password);
    auto remember = new QCheckBox("记住密码（使用 Windows 加密保存）");
    remember->setObjectName("sshRememberPassword");
    remember->setVisible(prompt->canRemember);
    body->addWidget(remember);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(prompt->confirmation ? "信任并继续" : "连接");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    if (prompt->confirmation)
    {
        buttons->button(QDialogButtonBox::Ok)->setAutoDefault(false);
        buttons->button(QDialogButtonBox::Ok)->setDefault(false);
        buttons->button(QDialogButtonBox::Cancel)->setDefault(true);
    }
    body->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dialog, [dialog, prompt, password, remember, parent]
    {
        prompt->answer(prompt->confirmation ? QStringLiteral("yes") : password->text(), remember->isChecked());
        password->clear();
        const auto cacheError = prompt->cacheError;
        dialog->accept();
        if (!cacheError.isEmpty())
        {
            QTimer::singleShot(0, parent, [parent, cacheError]
            {
                QMessageBox::information(parent, "密码未保存", cacheError + "\n本次连接仍会继续。");
            });
        }
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    QObject::connect(dialog, &QDialog::rejected, prompt, &SshAuthPrompt::cancel);
    QObject::connect(prompt, &SshAuthPrompt::finished, dialog, [dialog]
    {
        QTimer::singleShot(0, dialog, &QDialog::close);
    });
    dialog->open();
    if (!prompt->confirmation) { password->setFocus(); }
}

void clearSshCache(const Config &config, Engine *engine, QWidget *parent, const Theme &theme)
{
    try
    {
        const auto records = SshHostCache::resolve(config, engine->sshExecutable);
        QDialog dialog(parent);
        dialog.setObjectName("sshCacheDialog");
        dialog.setWindowTitle("清除当前主机的 SSH 缓存");
        dialog.setStyleSheet(theme.stylesheet());
        dialog.setMinimumWidth(440);
        auto body = new QVBoxLayout(&dialog);
        body->setContentsMargins(20, 18, 20, 18);
        body->setSpacing(12);
        auto target = new QLabel(QString("%1 → %2:%3").arg(config.sshHost, records.hostname).arg(records.port));
        target->setTextFormat(Qt::PlainText);
        target->setWordWrap(true);
        body->addWidget(target);
        auto passwords = new QCheckBox("忘记此主机在工作台中保存的密码和私钥口令");
        passwords->setObjectName("sshClearPasswords");
        passwords->setChecked(true);
        body->addWidget(passwords);
        auto fingerprints = new QCheckBox("清除这台主机的指纹记录（设备重装或更换时使用）");
        fingerprints->setObjectName("sshClearFingerprints");
        fingerprints->setChecked(true);
        body->addWidget(fingerprints);
        auto hint = new QLabel("清除会断开工作台中此主机的 SSH 连接，下次连接可重新确认指纹、输入密码。SSH 配置和私钥文件会保留。修改指纹文件前会自动备份。");
        hint->setWordWrap(true);
        body->addWidget(hint);
        auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        buttons->button(QDialogButtonBox::Ok)->setText("清除并断开连接");
        buttons->button(QDialogButtonBox::Cancel)->setText("取消");
        body->addWidget(buttons);
        QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted || (!passwords->isChecked() && !fingerprints->isChecked())) { return; }
        for (auto channel : engine->channels)
        {
            if (channel->config.kind == "ssh" && channel->config.sshHost.compare(config.sshHost, Qt::CaseInsensitive) == 0)
            {
                channel->close();
            }
        }
        if (passwords->isChecked())
        {
            SshCredentialCache::remove(engine->sshCacheDirectory, config.sshHost);
        }
        int removed = 0;
        if (fingerprints->isChecked())
        {
            for (const auto &file : records.files) { removed += SshHostCache::remove(file, records.identifiers); }
        }
        QMessageBox::information(parent, "SSH 缓存已清除",
            QString("已清除所选缓存，移除 %1 条主机指纹。\n请重新建立连接。").arg(removed));
    }
    catch (const std::exception &error)
    {
        QMessageBox::warning(parent, "无法清除 SSH 缓存", QString::fromUtf8(error.what()));
    }
}
}
