//
// Created by greenhand520 on 2026/5/4.
//

#pragma once

#include <expected>
#include <string>

// libgpiod 类型的前向声明
struct gpiod_chip;
struct gpiod_line;

namespace host_hw {

    enum GPIOValue : int {
        LOW,
        HIGH
    };

    /// @brief 基于 libgpiod 的 GPIO 输出引脚控制器。
    class GpioPin {
    public:
        GpioPin();
        ~GpioPin();

        GpioPin(const GpioPin&) = delete;
        GpioPin& operator=(const GpioPin&) = delete;

        /// @brief 打开 GPIO 芯片并请求一个“line”作为输出。
        /// @param chip_path  e.g. "/dev/gpiochip0"
        /// @param line_num   GPIO line offset number
        /// @param consumer   debugfs 中显示的使用者名称
        /// @param default_val 初始输出值
        /// @return void on success
        std::expected<void, std::string> open(const std::string& chip_path, unsigned int line_num,
                                              const std::string& consumer = "ubtech_servo", GPIOValue default_val = LOW);

        /// 打开 GPIO 芯片并通过 GPIO 名称请求一个“line”作为输出
        /// @param gpio_name GPIO1_C4、GPIO3_B2
        /// @param consumer debugfs 中显示的使用者名称
        /// @param default_val 初始输出值
        /// @return void on success
        std::expected<void, std::string> open(const std::string& gpio_name,
                                              const std::string& consumer = "servo", GPIOValue default_val = LOW);

        void close();
        [[nodiscard]] bool is_open() const;

        /// @brief 设置输出值（0 = 低，1 = 高）。
        [[nodiscard]] bool set_value(GPIOValue value) const;

        /// @brief 读取当前值。出错时返回 -1。
        [[nodiscard]] int get_value() const;

    private:
        gpiod_chip* chip_;
        gpiod_line* line_;
        std::string chip_path_;
        unsigned int line_num_;
    };

} // namespace host_hw
