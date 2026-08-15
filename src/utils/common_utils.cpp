//
// Created by greenhand520 on 2026/6/29.
//

#include <algorithm>
#include <chrono>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>

#include "common_cpp/utils/common_utils.hpp"


namespace fs = std::filesystem;

namespace {

std::string format_local_time(const std::time_t time, const char* format) {
    std::tm local_time{};
    // 时间超出系统可表示范围时 localtime_r 返回 nullptr，不能继续传给 put_time。
    if (::localtime_r(&time, &local_time) == nullptr) {
        return "";
    }

    std::stringstream ss;
    ss << std::put_time(&local_time, format);
    return ss.str();
}

} // namespace

std::string common_utils::to_lower_case(std::string str) {
    for (auto& c : str) {
        c = std::tolower(c);
    }
    return str;
}

long common_utils::get_current_sys_nanoseconds() {
    const auto now = std::chrono::system_clock::now();
    const auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

long common_utils::get_current_sys_millis() {
    const auto now = std::chrono::system_clock::now();
    const auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

long common_utils::get_app_started_millis() {
    const auto now = std::chrono::high_resolution_clock::now();
    const auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

std::string common_utils::get_today_data_str(const char* fmt) {
    const auto now = std::chrono::system_clock::now();
    const auto time_t = std::chrono::system_clock::to_time_t(now);
    return format_local_time(time_t, fmt);
}

char* common_utils::string_2_char_ptr(const std::string& str) {
    const auto c_str = new char[str.length() + 1];
    strcpy(c_str, str.c_str());
    return c_str;
}

std::string common_utils::simple_uuid() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;

    for (int i = 0; i < 32; i++) {
        // 在标准 UUID 位置插入版本号和变体号
        if (i == 12) {
            ss << "4"; // UUID version 4
        }
        else if (i == 16) {
            ss << std::hex << dis2(gen); // variant
        }
        else {
            ss << std::hex << dis(gen);
        }
    }

    return ss.str();
}

bool common_utils::is_ascii_whitespace(const char character) noexcept {
    return character == ' ' || (character >= '\t' && character <= '\r');
}

bool common_utils::is_empty_or_ascii_whitespace(const std::string_view value) noexcept {
    if (value.empty()) {
        return true;
    }

    return std::ranges::all_of(value.begin(), value.end(), is_ascii_whitespace);
}

// ========== 时间相关函数 ==========

int64_t common_utils::get_current_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::seconds>(duration).count();
}

std::string common_utils::timestamp_to_string(int64_t timestamp) {
    const auto time_t = static_cast<std::time_t>(timestamp);
    return format_local_time(time_t, "%Y-%m-%d %H:%M:%S");
}

std::string common_utils::timestamp_to_date(int64_t timestamp) {
    const auto time_t = static_cast<std::time_t>(timestamp);
    return format_local_time(time_t, "%Y-%m-%d");
}

std::string common_utils::get_current_time_string() {
    return timestamp_to_string(get_current_timestamp());
}

std::string common_utils::generate_uuid() {
    return simple_uuid();
}
