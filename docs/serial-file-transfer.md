<!-- SPDX-License-Identifier: Apache-2.0 -->
# 串口文件传输

连接物理串口后，在收发工具栏点击文件传输图标，选择发送或接收、协议以及文件路径，点击“开始传输”。窗口可调整大小，显示进度、已处理字节数、平均速率、预计剩余时间和重试次数。

文件传输不会自动启动设备端程序。设备端必须使用相同协议、波特率和串口参数；传输期间停止设备端的文本日志输出，避免日志混入文件或协议控制字节。

## 原始字节

发送文件内容本身，不附加文件名、长度、帧头、换行或校验。每次最多缓存 4096 字节，上一块通过串口驱动发送完成后再发送下一块。进度表示驱动发送量，不能证明设备已成功保存文件。

接收时可填写已知文件大小，单位为字节；达到该大小后自动保存。大小留空时，进度条显示接收中，设备发送结束后点击“停止并保存”。“取消传输”会丢弃未完成文件。

## XMODEM-CRC

接收端发送 `C` 启动 CRC 模式。发送使用 128 字节数据块、CRC-16/XMODEM、ACK/NAK 确认及重传；接收兼容 128 字节和 1024 字节数据块。支持块序号循环、重复块确认、两阶段 EOT 结束及双 CAN 取消，不自动降级到无 CRC 的校验和模式。

发送进度按对端 ACK 更新，不按写入串口缓冲区的字节数计算。完整结束确认前不显示 100%。数据块确认超时后重传，连续重试超过 10 次终止；发送端等待启动最长 30 秒。接收后短暂保留协议处理以重新确认重复 EOT。

XMODEM 不携带文件名或真实文件长度，末块以 `0x1A` 填充。接收已知长度的二进制文件时填写接收大小，软件只按该长度保存，保留文件本身合法的末尾 `0x1A`。大小留空时保留所有填充，避免误删有效数据。文件提前结束或超过指定长度时不会替换原文件。

接收使用临时文件，成功完成后才替换目标路径；取消、校验失败或连接中断不会留下半个目标文件。文件传输期间停止当前页的循环发送，禁止界面及 MCP 的普通写入和串口参数修改。收到的字节保留在共享历史中，并标记 `message: "file-transfer"`，暂时绕过脚本、波形、JSON/XML 和终端控制序列解析。MCP `list_connections` 中的 `file_transfer_active` 表示当前是否在传输。

## 独立测试 Demo

开启 `BUILD_TESTING` 后构建 `FileTransferDemo.exe`。Demo 使用与正式界面相同的传输控制器和进度窗口，通过虚拟串口两端实际传输约 48 KiB 的二进制文件，拆分接收块，支持注入一次 CRC 错误，并比对完整内容与 SHA-256。可切换原始字节 / XMODEM-CRC、发送 / 接收。

```powershell
./build/native-release/bin/FileTransferDemo.exe
# 自动运行，输出校验报告和完成截图
./build/native-release/bin/FileTransferDemo.exe --auto --quit `
    --report build/file-transfer-demo.json --screenshot build/file-transfer-demo.png
# 原始字节接收示例
./build/native-release/bin/FileTransferDemo.exe --auto --raw --receive --quit
```

从构建目录运行需将 Qt / MinGW 运行库目录加入 PATH，或单独使用 `windeployqt` 部署 Demo。Demo 不连接真实串口，不包含在正式安装包，也不作为正式应用的数据来源。

协议参考：[XMODEM/YMODEM Protocol Reference](https://wiki.zubax.com/public/development/XMODEMYMODEM-protocol-reference)。
