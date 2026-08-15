//
// Created by greenhand520 on 2026/7/19.
//

#include "common_cpp/utils/action_utils.hpp"

using namespace common_cpp;

std::string action_utils::uuid_to_string(const rclcpp_action::GoalUUID& uuid) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    for (const unsigned char i : uuid) {
        ss << std::setw(2) << static_cast<int>(i);
    }
    return ss.str();
}

std::string action_utils::uuid_to_format_string(const rclcpp_action::GoalUUID& uuid) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    for (size_t i = 0; i < uuid.size(); ++i) {
        ss << std::setw(2) << static_cast<int>(uuid[i]);
        if (i == 3 || i == 5 || i == 7 || i == 9) {
            ss << "-";
        }
    }
    return ss.str();
}