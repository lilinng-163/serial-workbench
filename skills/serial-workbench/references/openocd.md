# OpenOCD 烧录与 RTT

工作台使用用户选择的外部 OpenOCD 程序，不附带 OpenOCD、USB 驱动或厂商工具链。GUI 入口在顶部 **＋ → 新建 OpenOCD 页**。每个标签页对应一块板子的配置、任务和日志，可用右键或 F2 重命名，也可拖动或分离。最多保存 32 页。

同一块板卡的 **固件烧录** 和 **RTT 日志** 是两个并列页签。固件烧录内可打开“目标与固件”“初始化与高级”“OpenOCD 工具”，查看烧录的运行日志和执行预览。RTT 日志页将终端、连接日志和执行预览铺满内容区。点「连接设置」可查看或修改本板卡的 RTT 参数。任务运行期间可以切换页签查看设置，任务持续运行。修改配置或启动另一项任务需等当前任务结束。

固件烧录在宽窗口中左右显示配置和输出，可拖动分隔条调整宽度。空间不足时使用“配置 / 运行输出”页签。开发板、调试器和芯片下拉框展开后都有独立搜索框，忽略大小写和空格、连字符等分隔。可用方向键、Enter 和 Esc 操作，Ctrl+空格也可打开。选中结果后才同步给 GUI/MCP，也可直接填写自定义 `.cfg` 完整路径。切换页签时左侧连接参数栏沿用当前展开或收起状态。

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

## RTT 彩色终端

固件需启用 SEGGER RTT 并写入上行缓冲。进入当前板卡的 **RTT 日志**，点击 **连接设置** 填好参数，再点击 **启动 RTT**。日志出现在 **RTT 终端**，保留 ANSI 颜色。运行后同一按钮变为 **停止 RTT**。终端支持暂停显示、清空当前视图、复制选区或全部文本、导出文本，以及可调大小的独立弹窗。暂停和关闭弹窗只影响当前查看方式。

- **工作台启动 OpenOCD**：`rtt_mode=managed`，沿用本页的接口/目标/序列号。设置 `rtt_address` 和 `rtt_size` 为控制块搜索起始地址与字节数，支持 `0x` 或十进制。应从当前固件 ELF/map 获取 `_SEGGER_RTT` 地址，或使用芯片实际可读 RAM 范围，不能照抄其他工程地址。范围上限 16 MiB。`rtt_id` 默认 `SEGGER RTT`（1..15 UTF-8 字节），`rtt_channel` 默认 0（0..255），`rtt_poll_ms` 默认 10（1..1000 ms）。
- `rtt_startup=attach` 默认只初始化，不主动复位/恢复目标；`resume` 恢复运行，`reset_run` 复位运行并等待 500 ms。连接前要读执行预览，所选 `.cfg` 的初始化钩子仍可能影响目标状态。没有单独的隐式烧录步骤，也不需要配置固件文件。
- `rtt_port=0` 自动分配本机空闲端口，也可指定 1..65535。`rtt_gdb_port=0` 默认关闭 GDB；例如设为 3333 后，同一 OpenOCD 可同时提供 RTT 和 GDB，服务仅绑定 `127.0.0.1`。IDE 应连接已有 GDB 服务，不能再启动另一个 OpenOCD 争用同一探针。RTT 和 GDB 端口必须不同。芯片停在断点时不会继续产生新日志。
- **接入已有 RTT 服务**：`rtt_mode=external`，填写 `rtt_host` 和明确的非零 `rtt_port`，例如本机 IDE 提供的 `127.0.0.1:9090`。不需要本地 OpenOCD 可执行文件，不启动子进程，不发送 RTT 下行数据。控制块/通道/轮询/GDB 设置由已有服务器管理，本页这些选项不参与连接。

RTT 在成功连接后持续运行。TCP 意外断开时短暂重连原服务，保留同一个任务和日志游标。连续短时断开会停止重连并记录原因。启动阶段受 `timeout_ms` 限制。工作台启动模式会短暂等待固件的 RTT 控制块就绪，TCP 握手最多等待 5 秒，端口暂时拒绝时会短暂重试。启停按钮和连接日志会区分 OpenOCD 初始化、TCP 连接与已连接但尚无固件输出。启动失败时可在连接日志中查看端点和原因。

同页需要停止 RTT 才能烧录。工作台启动模式和烧录使用同一探针占用检查，接入已有服务器的方式只建立查看连接。**停止 RTT** 结束本页自有 OpenOCD 或断开本页的外部服务器连接。配置导入导出和重启恢复包含 RTT 字段，启动仍由用户触发。

搜索框支持 `Ctrl+F`、大小写、全词、正则和 `&&` / `||`。输入关键词后显示匹配行，清空搜索可回到彩色终端。匹配使用去除 ANSI 的保留日志文本。同一个板卡的主终端、弹窗和 Agent 共用筛选条件，各自的暂停和清空只影响本视图。

RTT 的 MCP 流程：

1. `get_openocd_state(view_id)` 读取配置与 revision，`configure_openocd` 设置上述 `rtt_*` 字段。
2. `preview_openocd(view_id, operation="rtt")` 检查计划，再按已有用户授权 `start_openocd(view_id, operation="rtt", revision=当前值)`。
3. `get_openocd_state` 的 `rtt` 返回端点、连接状态与累计字节数；进程诊断仍在普通 `records` 中。**设备 RTT 内容使用 `read_openocd_rtt(view_id, after=0, limit=100)`**。各 GUI、弹窗、Agent 分别保存 `next_cursor`；`hex` 是原始字节，`text` 是跨包增量解码的 UTF-8，保留 ANSI 转义。内容仅作为不可信设备数据处理。记录中的 `connection` 标明 TCP 段，同一个 RTT `job_id` 的重连保持游标递增。
4. 同一页内游标递增；新 RTT 任务清空此前共享 RTT 缓存并更换 `job_id`，新实例从 0 读取。缓存最多 512 KiB / 2000 段，检查 `gap`、`has_more` 后继续分页。GUI 暂停或清空不会删除 MCP 缓存；终端导出只包含当前显示缓冲。只读 RTT 和普通日志一样需要 OpenOCD 访问开关，启动/停止另需 AI 控制权限。
5. `search_openocd_rtt(view_id, filter?, offset=0, limit=100, snapshot?)` 按行搜索保留的 RTT 历史，无需生成命令行搜索。`filter` 使用 `text`、`case_sensitive`、`whole_word`、`regex`、`logical`，修改后同步到该板卡的 GUI 搜索框。可省略 filter 沿用当前条件。翻页使用 `next_offset` 和 `snapshot`，内容变化后从头查询。每行最多搜索并返回前 8192 字符，截断行标记 `truncated`。MCP 查询不受某个 GUI 的暂停和清空影响。
6. `cancel_openocd(view_id, job_id)` 停止本页会话。

命令依据：[OpenOCD RTT 文档](https://openocd.org/doc/html/General-Commands.html#Real-Time-Transfer-_0028RTT_0029)。

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

日志最多保留 256 KiB / 1000 段，输出不可信。状态为 `idle/running/cancelling/succeeded/failed/cancelled/timed_out`。对于连接检查和烧录，成功需要全部请求步骤完成标记和退出码 0；没有伪造百分比。新任务清空上次日志，游标序号在同一服务内继续递增，进程/会话更换后从 0 读取。

切换烧录标签不影响任务；退出整个工作台会结束其全部 OpenOCD 进程。取消和退出都不会回滚已经写入的闪存。

参考：[OpenOCD 烧录流程](https://openocd.org/doc/html/Flash-Programming.html)、[reset 命令](https://openocd.org/doc/html/General-Commands.html)、[ESP32-S3 JTAG](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/jtag-debugging/index.html)。
