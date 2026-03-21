#include <gazebo_set_joint_positions_plugin/gazebo_set_joint_positions_plugin.h>

#include <algorithm>
#include <string>
#include <vector>

#include <gz/plugin/Register.hh>
#include <gz/sim/components/JointPosition.hh>
#include <gz/sim/components/JointPositionReset.hh>
#include <gz/sim/components/JointVelocityReset.hh>
#include <gz/sim/components/Name.hh>
#include <gz/sim/components/ParentEntity.hh>

#include "rclcpp/rclcpp.hpp"

namespace gazebo_set_joint_positions_plugin
{

SetJointPositions::SetJointPositions() : update_needed_(false)
{
}

SetJointPositions::~SetJointPositions()
{
}

// cppcheck-suppress unusedFunction
void SetJointPositions::Configure(const gz::sim::Entity &_entity,
                                  const std::shared_ptr<const sdf::Element> &_sdf,
                                  gz::sim::EntityComponentManager &_ecm,
                                  gz::sim::EventManager &_eventMgr)
{
    model_entity_ = _entity;
    model_ = gz::sim::Model(_entity);

    if (_sdf->HasElement("topic_name"))
    {
        topic_name_ = _sdf->Get<std::string>("topic_name");
    }
    else
    {
        topic_name_ = "/joint_states";
    }

    // Create ROS 2 node
    if (!rclcpp::ok())
    {
        rclcpp::init(0, nullptr);
    }
    
    std::string node_name = "set_joint_positions_" + std::to_string(_entity);
    nh_ = rclcpp::Node::make_shared(node_name);
    
    sub_ = nh_->create_subscription<sensor_msgs::msg::JointState>(
        topic_name_, 1, std::bind(&SetJointPositions::jointStateCallback, this, std::placeholders::_1));

    // Get all joints from the model
    joints_list_ = model_.Joints(_ecm);
    
    // Get all links from the model
    links_list_ = model_.Links(_ecm);

    // Disable links (set kinematic mode by not applying physics)
    for (const auto& link : links_list_)
    {
        // Note: In Gazebo Sim, we typically don't disable links directly
        // The kinematic behavior is controlled by setting joint positions
    }

    RCLCPP_INFO(nh_->get_logger(), "Loaded SetJointPositions gazebo plugin. Watching topic: %s", topic_name_.c_str());
}

void SetJointPositions::jointStateCallback(const sensor_msgs::msg::JointState msg)
{
    {  // Start lock
        std::lock_guard<std::mutex> lock(lock_);
        joint_state_ = msg;
    }  // End lock
    update_needed_ = true;
}

void SetJointPositions::PostUpdate(const gz::sim::UpdateInfo &_info,
                                   const gz::sim::EntityComponentManager &_ecm)
{
    // Spin ROS 2 node to process callbacks
    rclcpp::spin_some(nh_);

    if (update_needed_)  // Only update if there is a change
    {
        sensor_msgs::msg::JointState last_joint_state;  // Local copy of callback value
        {                                               // Lock scope, keep the locks tight for speed
            std::lock_guard<std::mutex> lock(lock_);
            last_joint_state = joint_state_;
        }

        if (last_joint_state.header.stamp == rclcpp::Time(0))
            return;

        if (last_joint_state.position.empty())
            return;

        for (std::size_t i = 0; i < last_joint_state.name.size(); ++i)
        {
            const std::string& name = last_joint_state.name.at(i);

            // Find joint with matching name
            gz::sim::Entity joint_entity = gz::sim::kNullEntity;
            for (const auto& jt : joints_list_)
            {
                auto joint_name = _ecm.Component<gz::sim::components::Name>(jt);
                if (joint_name && joint_name->Data() == name)
                {
                    joint_entity = jt;
                    break;
                }
            }

            if (joint_entity == gz::sim::kNullEntity)
            {
                RCLCPP_WARN_STREAM_THROTTLE(nh_->get_logger(), *nh_->get_clock(), 1000,
                                            "Could not find JointState message joint " << name
                                                                                       << " in gazebo joint models");
            }
            else
            {
                double position = last_joint_state.position[i];
                
                // Get joint limits - in Gazebo Sim, we need to access the joint's SDF or component
                // For simplicity, we'll use a large range as default
                double upper_limit = 1e16;
                double lower_limit = -1e16;
                
                // TODO: Implement proper joint limit checking if needed
                // This would require accessing joint axis components

                // Bounds checks
                if (position > upper_limit)
                {
                    RCLCPP_WARN_STREAM_THROTTLE(nh_->get_logger(), *nh_->get_clock(), 1000,
                                                "Joint " << name << " is above upper limit " << position
                                                         << " > " << upper_limit);
                    position = upper_limit;
                }
                else if (position < lower_limit)
                {
                    RCLCPP_WARN_STREAM_THROTTLE(nh_->get_logger(), *nh_->get_clock(), 1000,
                                                "Joint " << name << " is below lower limit " << position
                                                         << " < " << lower_limit);
                    position = lower_limit;
                }

                // Set joint position using JointPositionReset component
                // This is the ECM way of setting joint positions
                auto* pos_cmd = const_cast<gz::sim::EntityComponentManager&>(_ecm).Component<gz::sim::components::JointPositionReset>(joint_entity);
                if (!pos_cmd)
                {
                    const_cast<gz::sim::EntityComponentManager&>(_ecm).CreateComponent(
                        joint_entity, gz::sim::components::JointPositionReset({position}));
                }
                else
                {
                    *pos_cmd = gz::sim::components::JointPositionReset({position});
                }

                // Also reset velocity to zero
                auto* vel_cmd = const_cast<gz::sim::EntityComponentManager&>(_ecm).Component<gz::sim::components::JointVelocityReset>(joint_entity);
                if (!vel_cmd)
                {
                    const_cast<gz::sim::EntityComponentManager&>(_ecm).CreateComponent(
                        joint_entity, gz::sim::components::JointVelocityReset({0.0}));
                }
                else
                {
                    *vel_cmd = gz::sim::components::JointVelocityReset({0.0});
                }

                // Handle mimic joints
                for (const auto& jt_mimic : joints_list_)
                {
                    auto mimic_joint_name = _ecm.Component<gz::sim::components::Name>(jt_mimic);
                    if (!mimic_joint_name)
                        continue;

                    bool set_mimic = false;
                    double mimic_position = position;
                    
                    if (mimic_joint_name->Data() == name + "_mimic")
                    {
                        set_mimic = true;
                    }
                    else if (mimic_joint_name->Data() == name + "_mimic_inverted")
                    {
                        set_mimic = true;
                        mimic_position = -position;
                    }

                    if (set_mimic)
                    {
                        // Get current position for debug logging
                        auto old_pos_comp = _ecm.Component<gz::sim::components::JointPosition>(jt_mimic);
                        double old_angle = 0.0;
                        if (old_pos_comp && !old_pos_comp->Data().empty())
                        {
                            old_angle = old_pos_comp->Data()[0];
                        }

                        // Bounds checks (using same large defaults)
                        if (mimic_position > upper_limit)
                        {
                            RCLCPP_WARN_STREAM_THROTTLE(nh_->get_logger(), *nh_->get_clock(), 1000,
                                                        "Joint " << mimic_joint_name->Data() << " is above upper limit "
                                                                 << mimic_position << " > " << upper_limit);
                            mimic_position = upper_limit;
                        }
                        else if (mimic_position < lower_limit)
                        {
                            RCLCPP_WARN_STREAM_THROTTLE(nh_->get_logger(), *nh_->get_clock(), 1000,
                                                        "Joint " << mimic_joint_name->Data() << " is below lower limit "
                                                                 << mimic_position << " < " << lower_limit);
                            mimic_position = lower_limit;
                        }

                        RCLCPP_DEBUG_STREAM_THROTTLE(nh_->get_logger(), *nh_->get_clock(), 1000,
                                                     "Updating joint " << mimic_joint_name->Data() << " from "
                                                                       << old_angle << " to " << mimic_position);

                        // Set mimic joint position
                        auto* mimic_pos_cmd = const_cast<gz::sim::EntityComponentManager&>(_ecm).Component<gz::sim::components::JointPositionReset>(jt_mimic);
                        if (!mimic_pos_cmd)
                        {
                            const_cast<gz::sim::EntityComponentManager&>(_ecm).CreateComponent(
                                jt_mimic, gz::sim::components::JointPositionReset({mimic_position}));
                        }
                        else
                        {
                            *mimic_pos_cmd = gz::sim::components::JointPositionReset({mimic_position});
                        }

                        // Reset velocity
                        auto* mimic_vel_cmd = const_cast<gz::sim::EntityComponentManager&>(_ecm).Component<gz::sim::components::JointVelocityReset>(jt_mimic);
                        if (!mimic_vel_cmd)
                        {
                            const_cast<gz::sim::EntityComponentManager&>(_ecm).CreateComponent(
                                jt_mimic, gz::sim::components::JointVelocityReset({0.0}));
                        }
                        else
                        {
                            *mimic_vel_cmd = gz::sim::components::JointVelocityReset({0.0});
                        }
                    }
                }
            }
        }
        update_needed_ = false;  // Flag we are done
    }
}

// Register the plugin
IGNITION_ADD_PLUGIN(
    gazebo_set_joint_positions_plugin::SetJointPositions,
    gz::sim::System,
    gazebo_set_joint_positions_plugin::SetJointPositions::ISystemConfigure,
    gazebo_set_joint_positions_plugin::SetJointPositions::ISystemPostUpdate)

IGNITION_ADD_PLUGIN_ALIAS(
    gazebo_set_joint_positions_plugin::SetJointPositions,
    "gazebo_set_joint_positions_plugin::SetJointPositions")

}  // namespace gazebo_set_joint_positions_plugin
