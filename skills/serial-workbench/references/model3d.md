# 3D 模型与 MCP

先 `list_connections` 确认设备，`list_model_views` 查看已打开的 3D 页面。不要把其他串口或其他命名流的同名字段当成本设备姿态。文件路径属于运行工作台的电脑，不是远程 AI 的文件系统。

## 导入、配置、验证

1. `open_model_view(file_path="C:/models/part.stl", channel_id="实际连接ID")` 创建 GUI 共享页面。支持 STL、OBJ、PLY，最大 64 MiB / 50 万三角面；省略 channel_id 创建未绑定页面。
2. 保存返回的 `view_id`，调用 `get_model_state(view_id)`，等待 `loading=false`，并检查 `load_error`。接口返回成功只表示开始读取文件，不代表导入完成。失败时保留旧模型，`using_example_mesh=true` 表示仍是内置示例。
3. 用最新 `revision` 作为 `expected_revision` 调用 `configure_model_view`。`settings` 是局部补丁；省略字段保留原值，对象逐层合并，数组整体替换。无效参数整体拒绝。GUI 改动、相机改动或模型加载完成使 revision 改变；冲突后重新读取，不能重用旧 revision。
4. 配置后读取 `get_model_state`。检查 `model.position`、`model.model_matrix_rows`、`telemetry.errors`、`position_sequence` 和 `rotation_sequence`，再用 `capture_model_view(view_id, max_size=1024)` 查看渲染结果。截图是 MCP PNG image 内容；纯文本客户端应使用数值状态，不能声称看过图片。

导入和修改需要工作台的 AI 控制权限；列表、状态和截图只读。关闭某协议的 AI 访问后，相应绑定页面不能读取或修改。所有接口沿用 GUI 页面，不创建第二条串口连接，不消费任何读取游标。

后台运行时也可导入、配置、读状态及截图。截图使用与 GUI 相同的渲染器、模型、相机、配色和姿态，在不可见视口绘制，不弹出窗口；需要桌面 OpenGL 环境，无法渲染时明确报错，不返回假截图。GUI 打开后可看到相同页面。view_id 只在页面生命周期内有效，重启后重新列举。

## 配置示例

以下是 `configure_model_view` 的参数结构；替换示例 ID 和 revision：

```json
{
  "view_id": "从列表获取",
  "expected_revision": "从最新状态获取",
  "settings": {
    "feed": "wave",
    "peer": "default",
    "stream": "imu",
    "mapping": {
      "mode": "euler",
      "order": "ZYX",
      "intrinsic": true,
      "radians": false,
      "position": [
        {"field":"x","gain":1,"offset":0},
        {"field":"y","gain":1,"offset":0},
        {"field":"z","gain":1,"offset":0}
      ],
      "angles": [
        {"field":"roll","gain":1,"offset":0},
        {"field":"pitch","gain":1,"offset":0},
        {"field":"yaw","gain":1,"offset":0}
      ],
      "axes": [1, 2, 3]
    },
    "manual_position": [0, 0, 0],
    "scale": 0.001,
    "follow_theme": false,
    "color": "#54B8D1"
  },
  "fit": true
}
```

| 设置 | 含义 |
| --- | --- |
| feed | `wave` 读取波形，`script` 读取串口脚本结果；脚本字段支持点路径 |
| peer / stream | `default` 为本连接；TCP 服务端使用实际对端键。stream 区分大小写，`""` 只选旧未命名 @wave，`imu` 只选 @wave[imu]；脚本不使用 stream |
| mapping.position / angles | 按 X/Y/Z 顺序的 3 个绑定。field 留空使用 offset 常量；数值为字段 × gain + offset，gain 默认 1、offset 默认 0 |
| mapping.quaternion | 按 W/X/Y/Z 顺序的 4 个绑定，配合 mode=`quaternion`；自动归一化，零模无效。使用全常量时 W 的 offset 设为 1 |
| order / intrinsic / radians | 六种欧拉顺序；true 为内禀/随动轴，false 为外禀/固定轴；默认 ZYX 内禀角度制 |
| axes | 输入 X/Y/Z 对应世界轴的带符号排列，例如 `[1,-3,2]`；绝对值不能重复 |
| manual_position | 世界坐标平移，始终叠加到遥测位置上，范围 ±1e9 |
| scale / centered / alignment | 模型比例 1e-6..1e6；是否先减模型中心；初始对齐 X/Y/Z 角度（±360），按 ZYX 应用。模型比例不缩放通道位置 |
| follow_theme / color | 是否跟随主题；自定义颜色支持 #RGB 或 #RRGGBB |
| paused | 暂停消费新遥测；手动修改仍可生效 |
| camera | yaw（±360）、elevation（±89.9）、distance（0.001..1e12）、target（3 个数值，±1e12）；也可传工具参数 fit=true 自动取景 |

`channel_id`、`file_path`、`fit` 位于工具参数顶层。`channel_id=""` 解除绑定；`file_path` 替换模型并异步导入。加载期间不可继续修改或截图，先等待完成。

## 分析时区分事实

`model_matrix_rows` 为 4×4 行数组，乘列向量，已包含遥测、手动平移、轴转换、对齐、缩放和中心平移。`world_bounds_*` 是变换后局部包围盒八角的保守边界，不是精确的三角网格碰撞检测。

位置和姿态分别保留最近完整的有效帧，不跨帧拼接四元数。缺失、零四元数等情况保持对应上一有效值并报告 errors。`observed_sequence` 是对端历史扫描位置，可能包含别的 stream；`position_sequence` / `rotation_sequence` 才是采用的帧（-1 表示还没有采用帧）。检查是否暂停、断线或序号不再变化，不能把保留姿态当作新反馈。

截图和数值可用于判断绑定是否正确、模型是否在视野内、旋转方向和颜色是否符合要求。不能仅凭截图推断 MCU 已收到命令、真实机械位置、材料性能或结构安全；需要原始回包/协议或实测证据。
