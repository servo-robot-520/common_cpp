//
// Created by greenhand520 on 2026/5/4.
//
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <termios.h>

namespace host_hw {

    /// @brief 具有可配置波特率和帧的 POSIX 串行端口包装器。
    class SerialPort {
    public:
        SerialPort();
        ~SerialPort();

        SerialPort(const SerialPort&) = delete;
        SerialPort& operator=(const SerialPort&) = delete;

        /// @brief 打开并配置串口。
        /// @param device   Device path, e.g. "/dev/ttyUSB0", "/dev/ttyAMA0"
        /// @param baud_rate  波特率 (9600, 19200, 38400, 57600, 115200, etc.)
        /// @return true on void
        std::expected<void, std::string> open(const std::string& device, int baud_rate);

        void close();
        [[nodiscard]] bool is_open() const;

        /// @brief 配置端口参数。
        [[nodiscard]] bool configure(int baud_rate, int data_bits = 8, int stop_bits = 1, char parity = 'N') const;

        /// @brief 写入原始字节。返回写入的字节数，如果出错则返回 -1。
        int write_data(const uint8_t* data, size_t length) const;

        /// @brief Read up to @p length bytes with a timeout.
        /// @param buffer     Destination buffer
        /// @param length     Maximum bytes to read
        /// @param timeout_ms Timeout in milliseconds (0 = non-blocking poll)
        /// @return 读取的字节数，超时为 0，错误为 -1
        int read_data(uint8_t* buffer, size_t length, int timeout_ms = 100) const;

        /// @brief 排空输入和输出缓冲区。
        void flush() const;

    private:
        int fd_;
        std::string device_;

        static speed_t baud_to_speed(int baud_rate);
    };

} // namespace host_hw
