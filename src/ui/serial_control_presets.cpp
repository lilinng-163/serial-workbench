// SPDX-License-Identifier: Apache-2.0
#include "serial_control_panel.h"

namespace sw
{
namespace
{
QJsonObject numeric(const QString &type, const QString &title, const QString &key,
    double minimum, double maximum, double step, int decimals, const QString &value)
{
    auto spec = defaultControl(type);
    spec["title"] = title;
    spec["key"] = key;
    spec["minimum"] = minimum;
    spec["maximum"] = maximum;
    spec["step"] = step;
    spec["decimals"] = decimals;
    spec["value"] = value;
    return spec;
}

QJsonObject action(const QString &type, const QString &title, const QString &key,
    const QString &value = "0", const QString &options = {})
{
    auto spec = defaultControl(type);
    spec["title"] = title;
    spec["key"] = key;
    spec["value"] = value;
    if (!options.isEmpty()) { spec["options"] = options; }
    return spec;
}
}

QJsonArray controlGroupPresets()
{
    // Editable command examples. The device firmware implements their meaning.
    return {
        QJsonObject {{"id", "pid"}, {"name", "PID 调参"},
            {"description", "比例、积分、微分系数，目标值、输出限幅、使能与积分清零"},
            {"controls", QJsonArray {
                numeric("number", "比例系数 Kp", "kp", 0, 10000, 0.01, 3, "1.000"),
                numeric("number", "积分系数 Ki", "ki", 0, 10000, 0.01, 3, "0.000"),
                numeric("number", "微分系数 Kd", "kd", 0, 10000, 0.01, 3, "0.000"),
                numeric("number", "目标值", "setpoint", -100000, 100000, 0.1, 2, "0.00"),
                numeric("number", "输出下限", "output_min", -10000, 10000, 1, 2, "0.00"),
                numeric("number", "输出上限", "output_max", -10000, 10000, 1, 2, "100.00"),
                action("switch", "启用 PID", "pid_enable"),
                action("button", "清零积分", "pid_reset", "1")}}},
        QJsonObject {{"id", "motor"}, {"name", "电机调速"},
            {"description", "目标转速、占空比、加速度、转向、使能与停止"},
            {"controls", QJsonArray {
                numeric("slider", "目标转速（rpm）", "speed", 0, 6000, 10, 0, "0"),
                numeric("slider", "占空比（%）", "duty", 0, 100, 0.5, 1, "0.0"),
                numeric("number", "加速度（rpm/s）", "acceleration", 0, 10000, 10, 0, "100"),
                action("select", "转动方向", "direction", "0", "正转=0\n反转=1"),
                action("switch", "电机使能", "motor_enable"),
                action("button", "停止电机", "motor_stop", "1")}}},
        QJsonObject {{"id", "temperature"}, {"name", "温度控制"},
            {"description", "目标温度、回差、采样周期、控制模式与加热使能"},
            {"controls", QJsonArray {
                numeric("number", "目标温度（℃）", "temperature", -40, 300, 0.5, 1, "25.0"),
                numeric("number", "温度回差（℃）", "hysteresis", 0, 50, 0.1, 1, "1.0"),
                numeric("number", "采样周期（ms）", "sample_ms", 10, 60000, 10, 0, "1000"),
                action("select", "控制模式", "temperature_mode", "0", "恒温=0\n手动=1"),
                action("switch", "加热使能", "heater_enable")}}},
        QJsonObject {{"id", "pwm"}, {"name", "PWM 输出"},
            {"description", "频率、占空比、输出通道、极性与输出使能"},
            {"controls", QJsonArray {
                numeric("number", "输出频率（Hz）", "pwm_frequency", 1, 1000000, 100, 0, "1000"),
                numeric("slider", "占空比（%）", "pwm_duty", 0, 100, 0.1, 1, "0.0"),
                action("select", "输出通道", "pwm_channel", "0", "通道 1=0\n通道 2=1\n通道 3=2\n通道 4=3"),
                action("select", "输出极性", "pwm_polarity", "0", "正常=0\n反相=1"),
                action("switch", "输出使能", "pwm_enable")}}}
    };
}
}
