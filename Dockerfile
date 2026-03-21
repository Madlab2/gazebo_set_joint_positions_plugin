FROM ros:humble

# Install Gazebo Fortress (gz-fortress) and ros-gz bridge packages
RUN apt-get update && apt-get install -y \
    ros-humble-ros-gz \
    ros-humble-ros-gz-sim \
    ros-humble-ros-gz-bridge \
    ros-humble-ros-gz-interfaces

# Install other ROS 2 Humble dependencies
RUN apt-get update && apt-get install -y \
    ros-humble-sensor-msgs \
    ros-humble-std-msgs \
    ros-humble-rclcpp \
    ros-humble-ament-cmake \
    ros-humble-robot-state-publisher \
    ros-humble-joint-state-publisher \
    ros-humble-joint-state-publisher-gui \
    ros-humble-xacro \
    python3-pip \
    python3-colcon-common-extensions

# # Install Python packages for ament
# RUN pip3 install ament_package

# Create workspace
WORKDIR /workspace

# Copy plugin source code
COPY . /workspace/src/gazebo_set_joint_positions_plugin/

# Install rosdep dependencies
RUN . /opt/ros/humble/setup.sh && \
    rosdep update && \
    rosdep install --from-paths src --ignore-src -r -y

# Build the plugin
RUN . /opt/ros/humble/setup.sh && \
    colcon build

# Source ROS in bashrc
RUN echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc
RUN echo "source /workspace/install/setup.bash" >> ~/.bashrc

WORKDIR /workspace

CMD ["/bin/bash"]
