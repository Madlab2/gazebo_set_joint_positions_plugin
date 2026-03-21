#!/bin/bash

IMAGE_NAME="gazebo_set_joint_positions_plugin:humble-fortress"
CONTAINER_NAME="gazebo_plugin_container"
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

function build() {
    echo "Building Docker image..."
    docker build -t ${IMAGE_NAME} ${SCRIPT_DIR}
}

function devel() {
    echo "Starting container with volume mounted..."
    
    # Grant X11 access to docker group
    xhost +local:docker > /dev/null 2>&1
    
    # Ensure xhost access is removed when container exits
    trap "xhost -local:docker > /dev/null 2>&1" EXIT
    
    docker run -it --rm \
        --name ${CONTAINER_NAME} \
        -v ${SCRIPT_DIR}:/workspace/src/gazebo_set_joint_positions_plugin:rw \
        -v /tmp/.X11-unix:/tmp/.X11-unix:rw \
        -e DISPLAY=${DISPLAY} \
        --network host \
        ${IMAGE_NAME}
    
    # Remove X11 access after container exits
    xhost -local:docker > /dev/null 2>&1
}

function enter() {
    if [ -z "$2" ]; then
        echo "Opening shell in running container..."
        docker exec -it ${CONTAINER_NAME} /bin/bash
    else
        echo "Executing command in running container: $2"
        docker exec -it ${CONTAINER_NAME} /bin/bash -c "$2"
    fi
}

function run() {
    echo "Starting container and launching GUI test..."
    
    # Grant X11 access to docker group
    xhost +local:docker > /dev/null 2>&1
    
    # Ensure xhost access is removed when container exits
    trap "xhost -local:docker > /dev/null 2>&1" EXIT
    
    docker run -it --rm \
        --name ${CONTAINER_NAME} \
        -v ${SCRIPT_DIR}:/workspace/src/gazebo_set_joint_positions_plugin:rw \
        -v /tmp/.X11-unix:/tmp/.X11-unix:rw \
        -e DISPLAY=${DISPLAY} \
        --network host \
        ${IMAGE_NAME} \
        /bin/bash -c "source /workspace/install/setup.bash && ros2 launch gazebo_set_joint_positions_plugin test_with_gui.launch.py"
    
    # Remove X11 access after container exits
    xhost -local:docker > /dev/null 2>&1
}

function stop() {
    echo "Stopping container..."
    docker stop ${CONTAINER_NAME}
    
    # Remove X11 access
    xhost -local:docker > /dev/null 2>&1
    echo "X11 access revoked"
}

function help() {
    echo "Usage: $0 {build|devel|run|enter [command]|stop}"
    echo ""
    echo "Commands:"
    echo "  build         - Build the Docker image"
    echo "  devel         - Start the container with volume mounted"
    echo "  run           - Start the container and run the GUI test launch file"
    echo "  enter [cmd]   - Open a shell in the running container, or execute [cmd] if provided"
    echo "  stop          - Stop the running container"
    echo ""
    echo "Examples:"
    echo "  $0 run                      # Launch Gazebo with GUI test"
    echo "  $0 enter                    # Open interactive shell"
    echo "  $0 enter 'colcon build'     # Run colcon build in container"
    echo ""
}

# Main script logic
case "$1" in
    build)
        build
        ;;
    devel)
        devel
        ;;
    run)
        run
        ;;
    enter)
        enter "$@"
        ;;
    stop)
        stop
        ;;
    *)
        help
        exit 1
        ;;
esac
