// SPDX-License-Identifier: Apache-2.0

// 帧：AA 55 | int16 温度 / 100 | uint16 电压 / 1000 | SUM8 | 0D
// 示例：AA 55 C4 09 E4 0C BC 0D -> 温度 25，电压 3.3
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

        if (buffer[7] != 0x0D
            || sum8(buffer, 0, 6) != buffer[6])
        {
            buffer_drop(1);
            continue;
        }

        emit("温度", read_i16_le(buffer, 2) / 100.0);
        emit("电压", read_u16_le(buffer, 4) / 1000.0);
        buffer_drop(8);
    }
}
