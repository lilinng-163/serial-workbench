# C 风格串口帧解析脚本

这是工作台内置解释器的 C 风格子集，不是可任意编译的 C/C++ 或设备固件代码。唯一入口是：

`void main(const uint8_t data[], int length)`

`data` 是本次收到的原始字节，`length` 为字节数。一次回调可能包含半帧、多帧，或混有日志。`data[i]`、`buffer[i]` 为只读的 0～255 字节。每个串口独立保留缓存和 `static` 变量，重新应用代码、停用后启用、重新连接会重置它们。脚本只处理启用后新到的字节，不追溯历史。

## 分帧和重同步

先 `buffer_append(data, length)`；检查缓存长度，寻找帧头，检查帧长度、帧尾与校验。数据不足时等待下一次回调；帧错误时丢一个字节继续搜索。只有确认完整有效帧后才 `emit`，然后消费完整帧。每轮循环都应消费数据或退出，避免原地死循环。

下面是示例协议，不代表用户设备的实际协议：8 字节，`AA 55` 帧头，2 字节小端有符号温度除以 100，2 字节小端无符号电压除以 1000，前 6 字节 SUM8，`0D` 帧尾。

```c
void main(const uint8_t data[], int length)
{
    buffer_append(data, length);

    while (buffer_size() >= 8)
    {
        if (buffer[0] != 0xAA || buffer[1] != 0x55)
        {
            buffer_drop(1);
            continue;
        }

        if (buffer[7] != 0x0D || sum8(buffer, 0, 6) != buffer[6])
        {
            buffer_drop(1);
            continue;
        }

        emit("温度", read_i16_le(buffer, 2) / 100.0);
        emit("电压", read_u16_le(buffer, 4) / 1000.0);
        buffer_drop(8);
    }
}
```

HEX `AA 55 C4 09 E4 0C BC 0D` 应输出温度 25、电压 3.3。在编辑器的「试解析」中每行 HEX 代表一次接收：分别输入 `AA 55 C4`、`09 E4 0C BC 0D` 可验证半帧；把两帧放同一行验证连帧；在前后插入日志、噪声、坏校验帧后再发送有效帧，验证能重新找到帧头。试解析不向硬件发送字节，也不改变在线脚本的状态。

## 可用函数

- `buffer_append(array, count)`：追加指定字节数，省略 count 时追加整个数组；`buffer_size()` 返回缓存字节数。
- `buffer_drop(count)`：提交当前尚未提交的字段，再消费头部字节；`buffer_clear()` 清空缓存，不提交字段。
- `emit("字段名", value)`：设置当前帧的有限数值字段，同名覆盖。`emit_frame()` 提交当前帧；main 返回时也提交尚未提交的字段。没有字段不产生记录。不要重复提交同一帧。
- `read_u8(array, offset)`、`read_i8(array, offset)`。
- `read_u16_le`、`read_u16_be`、`read_i16_le`、`read_i16_be`、`read_u32_le`、`read_u32_be`、`read_i32_le`、`read_i32_be`、`read_f32_le`、`read_f32_be`，参数均为 `(array, offset)`；浮点读取拒绝 NaN 和无穷值。
- `sum8(array, start, count)`、`xor8(array, start, count)`、`crc16_modbus(array, start, count)`。SUM8 为低 8 位；Modbus CRC 初值 0xFFFF、反射多项式 0xA001，比较时按设备约定选择字节序。
- `min(a,b)`、`max(a,b)`、`abs(a)`。

array 只能是 `data` 或 `buffer`，偏移与长度必须是非负整数且不越界。不要使用 `memcpy`、`printf`、`malloc`、`cJSON`、`send` 等不存在的函数。

## 语法与数值

支持块作用域、每条声明一个数值变量、`const`、`static`、赋值/复合赋值、自增自减、`if/else`、`for`、`while`、`break`、`continue`、`return;`、三元表达式、类型转换和注释。只有固定 main，没有头文件、宏、结构体、用户函数、用户数组、可写指针或动态内存，也没有文件、网络、系统命令和发送接口。

类型包括 `int/short/long/char/byte/bool/float/double/size_t`，`uint8_t/int8_t/uint16_t/int16_t/uint32_t/int32_t`，及 signed/unsigned 的 char/short/int/long。char 有符号 8 位，byte 无符号 8 位，int/long 有符号 32 位，size_t 无符号 32 位。转换和赋值按位宽截断，float 按单精度舍入。整数除法向零截断：需要小数时用 `100.0` 或 `(double)value`。位运算按无符号 32 位，右移补零，位移 0～31；不要假设完整 C 的整数提升和溢出规则。内部使用 double，整数精确范围到 `2^53-1`，超限报错。

源码与缓存各最多 64 KiB；单次 main 最多 200,000 步或约 100 ms，以先到者为准；每批最多 1,024 帧，每帧最多 16 个字段，字段名 1～64 字符。后台队列最多 128 批或 256 KiB。越界、除零、超限等错误会停用脚本，原始收发继续工作。

## 交付与读取结果

给用户可粘贴的完整 main，并注明字段偏移、字节序、倍率、校验算法与覆盖范围。提供有效、分块、连帧、混合日志、坏校验、坏帧尾的 HEX 样本及预期字段值；协议资料不足时说明哪些规则待确认。不要把示例的校验算法强加到未知设备。

用户在串口收发页保存并启用后，AI 通过 `list_connections` 核对串口 ID 与 `script_enabled`，再 `read_script_results(channel_id=目标ID, cursors={"default":0}, limit=100)`。读取 `sources.default.records[].value`，保存其 `next_cursor`，用记录的 `event_seq` 关联原始数据。JSON/XML 和波形使用各自工具与游标，脚本字段不会自动显示到那些页面。MCP 当前没有修改或启用脚本的接口。
