# SSH 密码与缓存验证

日期：2026-10-15。本地版本：0.15.14，Windows 11 / Qt 6.11.1 / MinGW 13.1.0。

## 行为

- 使用系统 OpenSSH 的 Askpass 入口，密码、私钥口令和主机指纹确认显示在工作台内。
- 密码通过带随机令牌、限当前 Windows 用户访问的本机 IPC 返回 OpenSSH，不进入终端字节流、配置 JSON、收发日志或 MCP。
- 「记住密码」默认关闭，启用后使用 Windows DPAPI 加密，按主机和认证提示区分缓存。密码被拒绝后重新询问；取消窗口终止当前连接。
- 「清除 SSH 缓存」可分别选择密码和主机指纹。按 `ssh -G` 解析别名、端口、HostKeyAlias 和用户指纹文件；支持 hashed known_hosts，保留同一行中的其他主机。
- 清除前备份指纹文件，断开当前主机的工作台连接，保留其他主机、SSH 配置和私钥。
- 认证助手随安装器、便携包和单文件运行库部署。卸载默认保留设置及加密密码；选择删除个人设置时一并清除密码缓存，不改动系统 known_hosts。

## 验证

- 六组 Qt 测试通过；`ssh_auth_tests` 覆盖加密往返、定向清除、指纹备份、别名和端口、失效密码重输、密码框遮盖、确认和取消、GUI 清除及共享日志不含密码。
- 使用本机 Paramiko SSH 服务与系统 `ssh.exe` 验证四次连接：首次信任并在错误密码后重输；使用已保存密码；服务器更换密钥时拒绝连接；清除指纹和密码后重新信任、登录。
- 同一真实测试使用含空格路径下的认证助手。
- 打包认证助手在移除 Qt SDK 和编译器 PATH 的环境中通过密码、指纹和缓存交互测试。
- 打包程序通过 HTTP / STDIO MCP 联调；密码框、指纹确认框和缓存清除窗口已渲染检查。
- 安装器通过自定义中文目录安装、自动查找原路径升级、无 SDK 环境启动、运行中保护及卸载保留个人设置的验证，安装载荷包含认证助手且不包含测试服务。

本机 SSH 测试服务、临时密钥和 Paramiko 依赖不进入正式安装包。测试没有访问云端 SSH 或修改用户实际的 known_hosts。

```powershell
ctest --test-dir build/native-release --output-on-failure
python -m pip install --target build/ssh-test-deps paramiko
python tests/integration/smoke_ssh_password.py --paramiko-dir build/ssh-test-deps --qt-bin <Qt>/bin --compiler-bin <MinGW>/bin
```

认证入口依据：[OpenSSH Askpass 实现](https://github.com/PowerShell/openssh-portable/blob/latestw_all/readpass.c)。
指纹索引依据：[OpenSSH 主机验证实现](https://github.com/PowerShell/openssh-portable/blob/latestw_all/sshconnect.c)。
