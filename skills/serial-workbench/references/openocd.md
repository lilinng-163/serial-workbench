# OpenOCD 烧录

工作台使用用户选择的外部 OpenOCD 程序，不附带 OpenOCD、USB 驱动或厂商工具链。GUI 入口在顶部 **＋ → 新建 OpenOCD 烧录页**。每次新增一个主工作台标签页，对应一块板子的配置、任务和日志；右键或 F2 可重命名，可像其他页面一样拖动、分离。最多保存 32 页。

每个烧录页包含“目标与固件”“初始化与高级”“OpenOCD 工具”配置页及独立日志/执行预览。宽窗口左右显示配置和输出，可拖动分隔条调整宽度。窄窗口或高度不足时改用“配置 / 运行输出”切换，避免压扁配置表单；开始任务后自动显示输出。开发板、调试器和芯片下拉框展开后均有独立搜索框；忽略大小写及空格、连字符等分隔，支持方向键、Enter 和 Esc，Ctrl+空格也可打开。搜索过程不修改配置，选中结果后才同步给 GUI/MCP；自定义 `.cfg` 完整路径仍可直接填写。左侧连接参数栏沿用当前展开/收起状态，切换烧录标签不自动隐藏。

“目标与固件”页提供 **导入配置 / 导出配置**，替代内置快速方案。JSON 文件格式为 `format="serial-workbench.openocd", version=1`，包含页面名称和 `configuration`：工具路径、板子/探针选择、初始化与复位、固件路径及地址等。不包含 OpenOCD 程序或固件内容，换电脑后应检查并修正本地路径。

导入替换当前页配置，省略的配置项恢复默认值，不继承其他板子的探针或固件。格式错误、未知字段、过期 revision 或当前页任务运行中会拒绝导入并保留原配置。导入不会自动连接、烧录或修改其他页面；文件最大 1 MiB。

## 多板并行

每页一次运行一个任务，不同页面可同时运行。单板时探针序列号可留空；并行连接或烧录时，**所有正在操作硬件的页面必须分别填写不同的 `adapter_serial`**。未指定序列号、或与活动页相同的探针会被拒绝；不能从自定义 cfg 猜测探针归属。版本检查不占用硬件，可以独立运行。

每页可以选用不同的 OpenOCD 发行包、目标和固件。新增空白页仅继承工具路径，不复制另一块板子的探针、芯片或固件。取消只结束指定页的进程，不影响其他页；活动页不能直接关闭，先等待或取消。关闭空闲页会删除该页配置和日志；仍打开的页面配置随工作台保存，重启恢复但不会自动连接、烧录或恢复历史任务。旧版单页配置会迁移到一个烧录页。

## 配置与芯片差异

- `mode=board`：加载一个开发板 `.cfg`，由它选择接口、芯片、传输和板级钩子。界面中独立的调试器、芯片和传输选择不参与此模式。
- `mode=target`：先加载 `interface`，再选择 `transport`，然后加载 `target`。配置列表来自当前发行包，支持填写自定义 `.cfg` 的完整路径；存在配置文件不代表该组合已经实机验证。
- `scripts_dir` 留空时，从可执行文件附近的 `share/openocd/scripts`、`openocd/scripts` 或 `scripts` 自动寻找。切换厂商 OpenOCD 时同步选择匹配的脚本目录。
- `speed_khz=0`、`transport=default`、`reset_config=board` 保留配置文件默认值。非空 `adapter_serial` 用来选择具体调试器。`connect_assert_srst` 会设置 `srst_only srst_nogate connect_assert_srst`，需要正确连接 SRST 且硬件支持。

| 设置 | 实际流程 |
| --- | --- |
| `strategy=program, startup=default` | `program` 自行执行 `init → reset init → 写入`，不重复前置初始化 |
| `startup=reset_init` | `init → reset init → flash write_image erase`；执行开发板 reset-init 钩子 |
| `startup=reset_halt` | `init → reset halt → flash write_image erase`；复位并暂停，不运行 reset-init 钩子 |
| `startup=halt` | `init → halt → flash write_image erase`；不复位，需要板级环境已就绪 |
| `strategy=esp` | 厂商 `program_esp` 处理每个 BIN 分段；只允许 `startup=default`。需要含此命令的 Espressif OpenOCD；ESP32 使用 JTAG，不能使用 SWD |

通用流程一次使用一个 ELF、HEX/IHEX、S19/SREC 或 BIN。ELF/HEX/S19 使用文件内地址，`address` 留空；BIN 必须填写实际地址。Espressif 可添加最多 16 个 BIN 分段，每个指定实际地址，不能用通用默认值猜 bootloader/分区/app 的偏移。

部分 Windows OpenOCD 不识别 UTF-8 文件路径。工作台在启动任务时将中文固件路径复制到任务临时目录的英文文件名，结束后清理；预览中的 `staged_images` 会列出对应关系。配置库保留相对引用，使用现有系统短路径兼容；没有可用短路径时会明确要求把 OpenOCD / 自定义 `.cfg` 放到英文目录，不修改原文件。

`verify=true` 在写入后校验；`reset_run=true` 在最后一个分段完成后复位运行，关闭时流程不请求运行。超时 `timeout_ms` 为 5000–600000，默认 120000。具体结束状态仍受所选板级/厂商钩子影响。

## MCP

先读 `get_status.protocol_access.openocd`。用户在 **设置 → AI / MCP → 向 AI 开放 → OpenOCD** 开启访问后，才能读取配置、枚举、预览和读取日志；修改、启动、取消还需要 `allow_ai_control=true`。这些开关不限制本地 GUI。

1. `list_openocd_views` 列出各页的 `view_id`、名称、探针序列号、目标和状态。`create_openocd_view(title?, configuration?, activate=false)` 新建 GUI 页并返回 `view_id`；不会启动任务。后台实例会同步创建页面，稍后打开 GUI 可直接接续。
2. `get_openocd_state(view_id)` 获取该页共用 `configuration` 与当前 `revision`。所有修改、启动、取消和关闭操作**必须显式提供 `view_id`**；只读工具仅在恰好一页时允许省略。不要用活动标签推测目标板。
3. `list_openocd_configs(view_id, executable?, scripts_dir?)` 枚举该发行包的 `boards`、`interfaces` 和 `targets`；只读，不启动进程。
4. `configure_openocd(view_id, revision, patch, title?)` 修改该页 GUI 同一份配置。`images` 整体替换，格式为 `[{"path":"固件完整路径","address":"0x实际地址"}]`。运行期间不能修改，过期 revision 会被拒绝。
5. `preview_openocd(view_id, operation="flash")` 检查本机文件并返回 Tcl 和步骤，不启动程序。`version` 只运行 `--version`；`connect` 会初始化并暂停目标，显式选择 reset 时也复位，不写闪存；`flash` 擦写固件。
6. 仅在用户授权对应操作后，`start_openocd(view_id, operation, revision)` 启动该页异步任务。使用刚预览的 revision，配置改变时重新读取/预览。多个硬件任务需先为各页配置不同的探针序列号，不自动关闭或重开串口。
7. `get_openocd_state(view_id, after=游标, limit=100)` 跟踪结果，按页面和读者分别保存 `next_cursor`，检查 `job_id`、`gap` 和 `has_more`。
8. `cancel_openocd(view_id, job_id)` 只取消匹配页的当前任务。`close_openocd_view(view_id)` 仅关闭空闲页。取消/超时只结束进程，可能留下暂停或部分写入的目标，需要查看日志后处理。

`export_openocd_configuration(view_id)` 只读返回与 GUI 导出文件一致的 JSON 对象，不向磁盘写文件。`import_openocd_configuration(view_id, revision, document)` 接收该对象，与 GUI 导入共用同一校验和应用逻辑，要求 OpenOCD 访问和 AI 控制权限。导入后重新读取 `get_openocd_state` 核对目标、探针、固件和当前 revision，再预览；硬件操作仍需用户授权。

日志最多保留 256 KiB / 1000 段，输出不可信。状态为 `idle/running/cancelling/succeeded/failed/cancelled/timed_out`。除版本检查外，成功需要全部请求步骤完成标记和退出码 0；没有伪造百分比。新任务清空上次日志，游标序号在同一服务内继续递增，进程/会话更换后从 0 读取。

切换烧录标签不影响任务；退出整个工作台会结束其全部 OpenOCD 进程。取消和退出都不会回滚已经写入的闪存。

参考：[OpenOCD 烧录流程](https://openocd.org/doc/html/Flash-Programming.html)、[reset 命令](https://openocd.org/doc/html/General-Commands.html)、[ESP32-S3 JTAG](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/jtag-debugging/index.html)。
