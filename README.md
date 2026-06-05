# perovsat-app
PEROVSAT Flight Software Zephyr Application

## Setup
The `setup.sh` script does the majority of the setup by installing needed packages, setting up the workspace by cloning other repos, cloning Zephyr, setting up the Python environment.

The current setup only supports MacOS and Linux. If you're on Windows, your best bet is using [WSL](https://learn.microsoft.com/en-us/windows/wsl/about)
```sh
git clone git@github.com:PEROVSAT/perovsat-app.git
cd perovsat-app
./setup.sh
```

This should work for local builds and testing. However, to support `west flash` onto an STM32 development board, you'll need to install STM32CubeProgrammer:
1. Go to the [download page](https://www.st.com/en/development-tools/stm32cubeprog.html)
2. Select the 2.22.0 release for your machine
  - NOTE: DO NOT INSTALL THE MACOS ARM VERSION, even if you have an ARM Mac. It fails to install correctly.
3. Log in or download as guest and get the email link
4. Unzip the download and run the setup app
5. Step through all steps with default settings.
