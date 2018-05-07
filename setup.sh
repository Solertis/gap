#!/bin/bash

# Source gazebo environment
source /usr/local/share/gazebo-9/setup.sh

# Libraries

# Export location of camera tools gazebo plugin
export GAZEBO_PLUGIN_PATH=${GAZEBO_PLUGIN_PATH}:`pwd`/build/camera_utils
# Export location of visual tools gazebo plugin
export GAZEBO_PLUGIN_PATH=${GAZEBO_PLUGIN_PATH}:`pwd`/build/visual_utils
# Export location of world utils gazebo plugin
export GAZEBO_PLUGIN_PATH=${GAZEBO_PLUGIN_PATH}:`pwd`/build/world_utils

# Resources

# Export location of custom resources
export GAZEBO_RESOURCE_PATH=${GAZEBO_RESOURCE_PATH}:`pwd`/media
# Export location of custom media
export GAZEBO_MEDIA_PATH=${GAZEBO_MEDIA_PATH}:`pwd`/media
# Export location of custom models
export GAZEBO_MODEL_PATH=${GAZEBO_MODEL_PATH}:`pwd`/models

echo "GAP was successfully initialised."
