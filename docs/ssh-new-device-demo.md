# SSH 新设备 Demo（仅开发测试）

双击 `scripts/ssh-new-device-demo.cmd`。启动器按需编译测试目标，并打开独立的测试台。
依赖仅保存在 `build/ssh-test-deps`，首次缺少依赖时会安装 `tests/demo/requirements.txt`。
不需要 Windows 的 sshd 服务或额外的云服务器。

启动失败时，命令窗口会保留错误信息；运行日志位于 `build/ssh-new-device-demo.log`。

Demo 使用系统 OpenSSH、工作台实际的认证窗口和终端控件，连接只监听 `127.0.0.1` 的虚拟 SSH 设备。
用户名为 `demo`，初始测试密码为 `demo-password-42`；地址、随机端口、当前密码和指纹显示在窗口顶部。

## 手动检查

1. 点击「连接测试设备」，确认首次指纹，输入上方测试密码。也可以故意输入错误密码，检查重新询问。
2. 勾选「记住密码」后连接成功，再次连接，检查自动使用缓存。
3. 点击「更换设备指纹」，再连接应因指纹不一致被拒绝。
4. 点击「清除测试缓存」，按需选择密码和指纹；重新连接后再次确认身份和输入密码。
5. 点击「修改设备密码」，当前密码会在窗口中更新；重连时旧密码失效，应重新弹出输入框。

终端提供简化的 `help`、`status`、`uptime`、`echo 文本`、`clear`、`exit`，用于检查收发；它不执行电脑上的系统命令，也不是完整 Linux 环境。

每次运行都在 `build/ssh-device-demo-*` 创建隔离目录。指纹、指纹备份和加密密码只存这里，
不修改用户的 `~/.ssh/config`、`known_hosts` 或正式软件的密码缓存；正常关闭窗口后会停止服务并清理临时目录。

## 自动检查

```powershell
.venv/Scripts/python.exe scripts/run-ssh-demo.py --self-test
```

自动操作实际认证窗口，依次检查首次认证和错误重试、记住密码、指纹变化拒绝、清除缓存后重新配置、密码变化重新询问，以及终端命令收发。
结果保存在 `build/ssh-new-device-demo-report.json`，完成截图保存在 `build/ssh-new-device-demo.png`。

`SshNewDeviceDemo` 只在 `BUILD_TESTING=ON` 下构建，没有 `install()` 规则。
正式打包脚本只复制明确列出的应用、MCP 桥和认证助手，不会复制 Demo、Python 测试服务或 Paramiko 依赖。
