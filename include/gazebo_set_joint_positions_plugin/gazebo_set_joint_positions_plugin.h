#ifndef GAZEBO_SET_JOINT_POSITIONS_PLUGIN_H
#define GAZEBO_SET_JOINT_POSITIONS_PLUGIN_H

#include <gz/sim/System.hh>
#include <gz/sim/Model.hh>

#include <gz/sim/Util.hh>
#include <gz/sim/Entity.hh>
#include <gz/sim/components/Joint.hh>
#include <gz/sim/components/JointPosition.hh>
#include <gz/sim/components/Link.hh>
#include <gz/sim/components/Name.hh>
#include <gz/sim/components/Pose.hh>
#include <sdf/Element.hh>

#include <atomic>

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/joint_state.hpp"
#include "std_msgs/msg/float32_multi_array.hpp"

namespace gazebo_set_joint_positions_plugin
{

class SetJointPositions : public gz::sim::System,
                          public gz::sim::ISystemConfigure,
                          public gz::sim::ISystemPreUpdate
{
  public:
    SetJointPositions();
    virtual ~SetJointPositions();

    void Configure(const gz::sim::Entity &_entity,
                  const std::shared_ptr<const sdf::Element> &_sdf,
                  gz::sim::EntityComponentManager &_ecm,
                  gz::sim::EventManager &_eventMgr) override;

    void PreUpdate(const gz::sim::UpdateInfo &_info,
                  gz::sim::EntityComponentManager &_ecm) override;

  private:
    void jointStateCallback(const sensor_msgs::msg::JointState msg);

    gz::sim::Entity model_entity_;
    gz::sim::Model model_;
    rclcpp::Node::SharedPtr nh_;
    rclcpp::Subscription<sensor_msgs::msg::JointState>::SharedPtr sub_;

    std::mutex lock_;
    sensor_msgs::msg::JointState joint_state_;

    std::string topic_name_;
    std::string robot_namespace_;

    std::atomic<bool> update_needed_;

    std::vector<gz::sim::Entity> joints_list_;
    std::vector<gz::sim::Entity> links_list_;
  };
}  // namespace gazebo_set_joint_positions_plugin

#endif
