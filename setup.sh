#!/bin/bash

# --- Size Warning and Confirmation ---
echo "This setup will take up ~2GB, and may take a while to install."
read -p "Enter 'y' to proceed, or anything else to cancel: " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
  echo "Setup cancelled."
  exit 1
fi
echo "Starting PEROVSAT Workspace Setup..."

# --- 1. Install System Dependencies ---
if [[ "$OSTYPE" == "darwin"* ]]; then
  echo "macOS detected. Installing brew dependencies..."
  brew install cmake ninja gperf qemu dtc wget libmagic
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
  echo "Linux detected. Installing apt dependencies..."
  sudo apt update
  sudo apt install -y git cmake ninja-build gperf ccache dfu-util device-tree-compiler wget python3-dev python3-venv python3-tk xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1
else
  echo "Unsupported OS. Please install CMake, Ninja, and Python manually."
fi

# --- 2. Directory Restructuring ---
echo "Restructuring directories..."

# Get the absolute path of the directory this script lives in (e.g., .../perovsat-app)
APP_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
APP_DIR_NAME=$(basename "$APP_DIR")

# Get the parent directory
PARENT_DIR=$(dirname "$APP_DIR")

# Define the target workspace directory
WORKSPACE_DIR="$PARENT_DIR/perovsat-workspace"

# Check if we are already inside a workspace folder (prevents breaking if run twice)
if [[ "$(basename "$PARENT_DIR")" == "perovsat-workspace" ]]; then
  echo "Directory is already structured. Proceeding..."
  cd "$PARENT_DIR"
else
  echo "Creating workspace wrapper..."
  mkdir -p "$WORKSPACE_DIR"

  # Move the app repository inside the new workspace wrapper
  mv "$APP_DIR" "$WORKSPACE_DIR/"

  # Navigate our terminal session into the new workspace
  cd "$WORKSPACE_DIR"
fi

# --- 3. Setup Zephyr Environment via West ---
echo "Initializing West workspace..."
# We use the dynamic variable in case they cloned the folder with a custom name
west init -l "$APP_DIR_NAME"

echo "Pulling repositories..."
west update

echo "========================================="
echo "PEROVSAT workspace setup complete!"
echo "Note: If you do not have NDA access, you may have seen cloning errors for the drivers. This is expected."
echo "IMPORTANT: Your application has been moved!"
echo "Please run:  cd ../perovsat-workspace/perovsat-app  to continue."
echo "========================================="
