//
// Created by greenhand520 on 2026/7/19.
//

#pragma once

#include <rclcpp_action/types.hpp>

namespace common_cpp::action_utils {

    std::string uuid_to_string(const rclcpp_action::GoalUUID& uuid);

    std::string uuid_to_format_string(const rclcpp_action::GoalUUID& uuid);

}