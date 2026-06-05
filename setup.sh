#!/bin/bash

# Enforce strict error handling
set -eo pipefail

# --- Configuration & Globals ---
SDK_VERSION="1.0.0"
APP_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)
APP_DIR_NAME=$(basename "$APP_DIR")
PARENT_DIR=$(dirname "$APP_DIR")
WORKSPACE_DIR="$PARENT_DIR/perovsat-workspace"
TMP_DIR=""

# --- Cleanup Handler ---
# Automatically cleans up temporary directories if the script fails or gets Ctrl+C'd
cleanup() {
  if [ -n "$TMP_DIR" ] && [ -d "$TMP_DIR" ]; then
    echo -e "\n[Cleanup] Removing incomplete temporary downloads..."
    rm -rf "$TMP_DIR"
  fi
}
trap cleanup EXIT INT TERM

# --- Functions ---

confirm_execution() {
  echo "This setup will take up ~5GB, and may take a while to install."
  read -p "Enter 'y' to proceed, or anything else to cancel: " -n 1 -r
  echo
  if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "Setup cancelled."
    # Disable trap before intentional exit so it doesn't print the cleanup message unnecessarily
    trap - EXIT
    exit 0
  fi
  echo "Starting PEROVSAT Workspace Setup..."
}

install_system_deps() {
  echo "--- 1. Checking System Dependencies ---"
  if [[ "$OSTYPE" == "darwin"* ]]; then
    echo "macOS detected. Installing brew dependencies..."
    brew install cmake ninja gperf qemu dtc wget libmagic python3 python-tk ccache openocd
  elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    echo "Linux detected. Installing apt dependencies..."
    sudo apt update
    sudo apt install -y git cmake ninja-build gperf ccache dfu-util \
      device-tree-compiler wget python3-dev python3-venv python3-tk \
      xz-utils file make gcc gcc-multilib g++-multilib libsdl2-dev libmagic1
  else
    echo "Unsupported OS. Please install CMake, Ninja, and Python manually."
  fi
}

install_zephyr_sdk() {
  echo "--- 2. Checking Zephyr SDK v${SDK_VERSION} ---"
  local dest_dir="$HOME/zephyr-sdk-${SDK_VERSION}"

  # Check if the setup script exists, meaning it was fully extracted previously
  if [ -x "$dest_dir/setup.sh" ]; then
    echo "Zephyr SDK already fully installed."
    return
  fi

  echo "Zephyr SDK missing or incomplete. Downloading..."

  # Determine OS and Architecture for correct download URL
  local os_type arch sdk_file
  os_type=$(uname -s | tr '[:upper:]' '[:lower:]')
  arch=$(uname -m)

  if [[ "$os_type" == *"darwin"* ]]; then
    [[ "$arch" == "arm64" ]] && sdk_file="zephyr-sdk-${SDK_VERSION}_macos-aarch64_minimal.tar.xz" || sdk_file="zephyr-sdk-${SDK_VERSION}_macos-x86_64_minimal.tar.xz"
  elif [[ "$os_type" == *"linux"* ]]; then
    sdk_file="zephyr-sdk-${SDK_VERSION}_linux-x86_64_minimal.tar.xz"
  else
    echo "Error: Unrecognized platform for SDK download."
    exit 1
  fi

  # Download and extract in a temporary directory to ensure atomicity
  TMP_DIR=$(mktemp -d 2>/dev/null || mktemp -d -t 'zephyr_sdk')
  cd "$TMP_DIR"

  wget -q --show-progress "https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${SDK_VERSION}/${sdk_file}"
  echo "Extracting..."
  tar xf "$sdk_file"

  # Move to final location only after successful extraction
  rm "$sdk_file"
  # If a broken partial directory exists in home, overwrite it
  rm -rf "$dest_dir"
  mv "zephyr-sdk-${SDK_VERSION}" "$HOME/"

  # Clear tracking variable so trap doesn't try to delete it
  TMP_DIR=""

  echo "Registering Zephyr SDK (ARM only)..."
  cd "$dest_dir"
  ./setup.sh -t arm-zephyr-eabi -c
}

setup_directories() {
  echo "--- 3. Restructuring Directories ---"
  if [[ "$(basename "$PARENT_DIR")" == "perovsat-workspace" ]]; then
    echo "Directory is already structured. Proceeding..."
    WORKSPACE_DIR="$PARENT_DIR"
  else
    echo "Creating workspace wrapper..."
    mkdir -p "$WORKSPACE_DIR"
    mv "$APP_DIR" "$WORKSPACE_DIR/"

    # Update APP_DIR to reflect its new location
    APP_DIR="$WORKSPACE_DIR/$APP_DIR_NAME"
  fi
  cd "$WORKSPACE_DIR"
}

setup_python_and_west() {
  echo "--- 4. Setting up Python venv & West ---"

  if [ ! -d ".venv" ]; then
    python3 -m venv .venv
  fi

  # Activate virtual environment
  source .venv/bin/activate

  # Install west inside venv before initializing
  pip install --upgrade pip wheel #quiet # was unable to find quiet, trying without it
  pip install west

  # Initialize West only if it hasn't been done yet
  if [ ! -d ".west" ]; then
    echo "Initializing West workspace..."
    west init -l "$APP_DIR_NAME"
  else
    echo "West workspace already initialized."
  fi

  echo "Pulling repositories (this may take a moment)..."
  west update

  # Install Zephyr dependencies
  if [ -f "zephyr/scripts/requirements.txt" ]; then
    echo "Installing Zephyr Python dependencies..."
    pip install -r zephyr/scripts/requirements.txt
  else
    echo "Warning: zephyr/scripts/requirements.txt not found. Skipping."
  fi
}

cleanup_legacy_builds() {
  if [ -d "$APP_DIR/build" ]; then
    echo "Clearing legacy build directory to prevent Python path caching conflicts..."
    rm -rf "$APP_DIR/build"
  fi
}

finish_setup() {
  echo "========================================="
  echo "PEROVSAT workspace setup complete!"
  echo "Note: If you do not have NDA access, you may have seen cloning errors for the drivers. This is expected."
  echo "IMPORTANT: Your application lives in: $APP_DIR"
  echo "To start developing, run the following commands:"
  echo "  cd $WORKSPACE_DIR/$APP_DIR_NAME"
  echo "  source ../.venv/bin/activate"
  echo "========================================="

  # Disable the trap before successful exit
  trap - EXIT
}

# --- Main Execution Flow ---
confirm_execution
install_system_deps
install_zephyr_sdk
setup_directories
setup_python_and_west
cleanup_legacy_builds
finish_setup
