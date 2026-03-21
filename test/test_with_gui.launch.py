#!/usr/bin/env python3
"""
Launch file for GUI testing of the SetJointPositions plugin.

This launch file:
1. Launches Gazebo Fortress with GUI
2. Spawns the test robot model with the plugin
3. Launches robot_state_publisher (provides robot description)
4. Launches joint_state_publisher_gui for manual control

To test:
- Move the sliders in the joint_state_publisher_gui
- Observe the robot moving in Gazebo
"""

import os
from os.path import join
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    AppendEnvironmentVariable
)
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node
from launch.substitutions import Command


def generate_launch_description():
    pkg_dir = get_package_share_directory('gazebo_set_joint_positions_plugin')
    gz_sim_share = get_package_share_directory('ros_gz_sim')
    
    world_file = join(pkg_dir, 'test', 'test.world')
    urdf_file = join(pkg_dir, 'test', 'robot.urdf')
    
    gz_sim = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(join(gz_sim_share, 'launch', 'gz_sim.launch.py')),
        launch_arguments={
            'gz_args': PythonExpression(["'", world_file, " -r -v 4'"])
        }.items()
    )
    
    # Robot State Publisher (provides robot description for joint_state_publisher_gui)
    robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher',
        output='screen',
        parameters=[{
            'robot_description': Command(['cat ', urdf_file])
        }]
    )
    
    # Spawn the robot model using SDF file
    gz_spawn_entity = Node(
        package="ros_gz_sim",
        executable="create",
        arguments=[
            "-topic", "/robot_description",
            "-name", "test_robot",
            "-allow_renaming", "true",
            '-x', '0',
            '-y', '0',
            '-z', '2.5'
        ]
    )
    
    # ROS-Gazebo bridge for clock
    gz_ros2_bridge = Node(
        package='ros_gz_bridge',
        executable='parameter_bridge',
        arguments=[
            '/clock@rosgraph_msgs/msg/Clock[ignition.msgs.Clock'
        ],
        output='screen'
    )
    
    # Joint State Publisher GUI (for manual control)
    joint_state_publisher_gui = Node(
        package='joint_state_publisher_gui',
        executable='joint_state_publisher_gui',
        name='joint_state_publisher_gui',
        output='screen',
        parameters=[{
        }]
    )
    
    return LaunchDescription([
        gz_sim,
        robot_state_publisher,
        gz_spawn_entity,
        gz_ros2_bridge,
        joint_state_publisher_gui,
    ])
