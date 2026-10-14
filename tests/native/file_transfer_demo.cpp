// SPDX-License-Identifier: Apache-2.0
// Standalone test-only virtual UART; deliberately excluded from install().
#include "file_transfer_dialog.h"
#include "controls.h"
#include <QtWidgets>

using sw::SerialTransfer;

static QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { return {}; }
    return file.readAll();
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    app.setFont(sw::Theme::interfaceFont());
    app.setStyleSheet(sw::Theme::make(false).stylesheet());
    QCommandLineParser options;
    options.addHelpOption();
    options.addOption({"auto", "Start automatically."});
    options.addOption({"raw", "Use raw bytes."});
    options.addOption({"receive", "Test receiving."});
    options.addOption({"screenshot", "Save completed screenshot.", "path"});
    options.addOption({"report", "Write integrity JSON.", "path"});
    options.addOption({"quit", "Exit after verification."});
    options.process(app);
    QTemporaryDir directory;
    if (!directory.isValid()) { return 2; }
    const QString source = directory.filePath("demo-source.bin");
    const QString destination = directory.filePath("demo-received.bin");
    QByteArray fixture(48 * 1024 + 23, 0);
    for (int i = 0; i < fixture.size(); ++i) { fixture[i] = char((i * 37 + i / 19) & 255); }
    fixture[fixture.size() - 1] = char(0x1a);
    QFile file(source);
    if (!file.open(QIODevice::WriteOnly) || file.write(fixture) != fixture.size()) { return 2; }
    file.close();
    SerialTransfer local;
    SerialTransfer peer;
    sw::FileTransferDialog dialog(&local);
    dialog.setWindowTitle("串口文件传输 Demo · 虚拟串口");
    dialog.resize(720, 565);
    auto layout = qobject_cast<QVBoxLayout *>(dialog.layout());
    auto description = new QLabel("独立测试程序 · 模拟串口两端 · 不连接真实设备");
    layout->insertWidget(1, description);
    auto inject = new QCheckBox("模拟一次 CRC 错误，观察校验和重传");
    inject->setChecked(true);
    layout->insertWidget(layout->count() - 2, inject);
    auto integrity = new QLabel("传输后会逐字节比对文件，并显示 SHA-256 校验结果。");
    integrity->setWordWrap(true);
    integrity->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->insertWidget(layout->count() - 2, integrity);
    dialog.protocol->setCurrentIndex(options.isSet("raw") ? 0 : 1);
    dialog.direction->setCurrentIndex(options.isSet("receive") ? 1 : 0);
    const auto updatePath = [&]
    {
        dialog.path->setText(dialog.direction->currentIndex() == 0 ? source : destination);
        dialog.expectedSize->setText(QString::number(fixture.size()));
    };
    updatePath();
    QObject::connect(dialog.direction, &QComboBox::currentIndexChanged, &dialog, updatePath);
    bool corrupt = false;
    bool checking = false;
    bool verified = false;
    QString reference;
    QString result;
    const auto deliver = [&](SerialTransfer *from, SerialTransfer *to, QByteArray bytes)
    {
        if (corrupt && bytes.size() == 133) { bytes[40] ^= char(0x20); corrupt = false; }
        QTimer::singleShot(dialog.protocol->currentIndex() == 0 ? 55 : 8, &dialog, [from, to, bytes]
        {
            for (int i = 0; i < bytes.size(); i += 23) { to->feed(bytes.mid(i, 23)); }
            from->drained();
        });
        return true;
    };
    local.write = [&](const QByteArray &bytes) { return deliver(&local, &peer, bytes); };
    peer.write = [&](const QByteArray &bytes) { return deliver(&peer, &local, bytes); };
    QObject::connect(&dialog, &sw::FileTransferDialog::starting, &dialog, [&]
    {
        corrupt = inject->isChecked() && dialog.protocol->currentIndex() == 1;
        checking = true;
        verified = false;
        integrity->setText("正在通过虚拟串口传输真实文件…");
        const auto mode = dialog.protocol->currentIndex() == 0 ? SerialTransfer::Protocol::Raw : SerialTransfer::Protocol::XmodemCrc;
        if (dialog.direction->currentIndex() == 0)
        {
            reference = dialog.path->text();
            result = destination;
            peer.startReceive(result, mode, QFileInfo(reference).size());
        }
        else
        {
            reference = source;
            result = dialog.path->text();
            peer.startSend(reference, mode);
        }
    });
    QObject::connect(&local, &SerialTransfer::finished, &dialog, [&](bool success)
    {
        if (!success) { peer.cancel(); checking = false; integrity->setText("传输未完成，尚未验证文件。"); }
    });
    QTimer refresh;
    refresh.setInterval(100);
    QObject::connect(&refresh, &QTimer::timeout, &dialog, [&]
    {
        if (local.active()) { dialog.refresh(); }
        inject->setEnabled(!local.active() && !peer.active());
        if (!checking || local.active() || peer.active()) { return; }
        checking = false;
        const auto original = readFile(reference);
        const auto received = readFile(result);
        const auto hash = QCryptographicHash::hash(received, QCryptographicHash::Sha256).toHex();
        verified = original == received;
        integrity->setText((verified ? "✓ 文件完整一致" : "文件校验失败")
            + QString(" · %1 字节\nSHA-256: %2").arg(received.size()).arg(QString::fromLatin1(hash)));
        if (options.isSet("report"))
        {
            const QJsonObject report {{"verified", verified}, {"bytes", received.size()}, {"sha256", QString::fromLatin1(hash)},
                {"source_sha256", QString::fromLatin1(QCryptographicHash::hash(original, QCryptographicHash::Sha256).toHex())},
                {"protocol", dialog.protocol->currentIndex() == 0 ? "raw" : "xmodem-crc"},
                {"direction", dialog.direction->currentIndex() == 0 ? "send" : "receive"},
                {"local_retries", local.retries()}, {"peer_retries", peer.retries()}};
            QFile reportFile(options.value("report"));
            if (reportFile.open(QIODevice::WriteOnly)) { reportFile.write(QJsonDocument(report).toJson()); }
        }
        QTimer::singleShot(150, &dialog, [&]
        {
            if (options.isSet("screenshot")) { dialog.grab().save(options.value("screenshot")); }
            if (options.isSet("quit")) { app.exit(verified ? 0 : 3); }
        });
    });
    refresh.start();
    dialog.show();
    if (options.isSet("auto")) { QTimer::singleShot(200, &dialog, [&] { dialog.begin(); }); }
    return app.exec();
}
