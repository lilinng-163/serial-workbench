# Docker 开发环境

以下命令均在项目根目录执行，构建上下文为整个项目。

提供 [Dockerfile](Dockerfile)，包含 Ubuntu 26.04、Qt、CMake、Ninja、GCC、GDB 与源码；构建镜像时会编译桌面程序、MCP 桥和 SSH 认证助手。

```sh
docker build -f docker/Dockerfile -t serial-workbench-dev .
docker run --rm -it serial-workbench-dev
# 进入容器后，源码在 /workspace，已编译程序在 /opt/serial-workbench-build/bin
```

上面的命令默认使用 Docker Hub 官方基础镜像 `ubuntu:26.04`。同一个 Dockerfile 也支持轩辕镜像源，通过构建参数选择，无需修改文件。

```sh
# 轩辕公共域：需要已有登录态，已登录无需重复登录
docker build -f docker/Dockerfile --build-arg BASE_IMAGE=docker.xuanyuan.run/library/ubuntu:26.04 -t serial-workbench-dev .

# 轩辕专属域：替换占位符为自己的域名，无需登录
docker build -f docker/Dockerfile --build-arg BASE_IMAGE="<你的专属域名>/library/ubuntu:26.04" -t serial-workbench-dev .
```

认证规则见[镜像源使用规范](https://xuanyuan.cloud/agents.md)。两种来源由命令显式选择，构建失败时不会自动换源。APT 源默认使用中科大 Ubuntu 镜像，可通过 `--build-arg APT_MIRROR=<Ubuntu软件源地址>` 覆盖。请不要将个人专属域名或凭据提交到仓库。

编辑宿主机代码时，可在项目根目录挂载源码进入容器：

```sh
docker run --rm -it --mount type=bind,source=.,target=/workspace serial-workbench-dev
# Linux 宿主机可加 --user "$(id -u):$(id -g)"，让编译产物归当前用户所有
cmake -S . -B build/docker -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=OFF
cmake --build build/docker --parallel 2
```

这是 Linux 开发环境，生成 Linux 可执行文件；Windows EXE 与安装包仍使用[项目中的 Windows 构建流程](../README.zh-CN.md#从源码构建)。无图形显示的环境可用 `QT_QPA_PLATFORM=offscreen` 做启动检查；显示桌面窗口需要另行连接宿主机的图形显示服务。容器不会自动映射串口设备或开放 MCP 端口。
