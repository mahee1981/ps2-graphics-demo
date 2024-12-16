# PS2 GRAPHICS DEMO

This repository has been set up to track code changes to my personal PS2 graphics C++ interface. It can serve also as a reference for learning graphics on PS2.

Milestones:
 - Successfully initialized PS2 Graphics Synthesizer
 - Rendered a Gouraud shaded triangle
 - TODO: Math on EE core and VU0
 - TODO: Model, View and Projection matrices
 - TODO: Texture loading
 - TODO: Model loading


# Project Setup

This guide will walk you through setting up the project, including prerequisites, building the `ps2dev` development tools, and running the project in a development environment.

---

## **Prerequisites**

Before starting, ensure your system has the following installed:

- **Git**: [Download Git](https://git-scm.com/)
- **Docker**: [Download Docker](https://www.docker.com/)
- **Visual Studio Code**: [Download VS Code](https://code.visualstudio.com/)
- **Dev Containers Extension** for VS Code:
  - Install the [Dev Containers extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers).

---

## **Project Setup Options**

You have two options to set up and build this project:

1. **Using the Dev Containers Extension in VS Code (Recommended)**  
2. **Building PS2SDK from Source on Linux**

---

## **Option 1: Using the Dev Containers Extension in VS Code (Recommended)**

This method uses the **Dev Containers** extension to set up a pre-configured development environment with all necessary dependencies.

### **Steps**:

1. **Install the Dev Containers Extension**:

   In VS Code, go to the Extensions view (`Ctrl+Shift+X` or `Cmd+Shift+X` on Mac), search for `Dev Containers`, and install it.

2. **Clone the Repository**:

   Open a terminal and run:
   ```bash
   git clone https://github.com/mahee1981/ps2-graphics-demo.git
   cd ps2-graphics-demo
   ```

3. **Open the Project in VS Code**:

   Open the folder containing the project in VS Code:
   ```bash
   code .
   ```

4. **Set up intellisense** 

    Define the version of the PS2SDK that you are running in `c_cpp_properties.json` as:
    ```json
    "ps2sdk_version" : "14.2.0"
    ```
    This will ensure that the intellisene is configured properly inside the container

5. **Open the Dev Container**

   When the project opens in VS Code, you should see a notification asking if you want to reopen the project in a Dev Container. Click **Reopen in Container**.  
   If the notification doesn’t appear, open the Command Palette (`Ctrl+Shift+P` or `Cmd+Shift+P` on Mac), then run **Dev Containers: Reopen in Container**.

   This will:
   - Build the Dev Container using the provided `devcontainer.json` file.
   - Install all necessary dependencies, including pulling the latest build of ps2sdk.

6. **Build the Project**:

   Once the container is set up and running, open a terminal inside VS Code (`Ctrl+`` or `Cmd+`` on Mac) and run:
   ```bash
   cd project
   make
   ```

   Replace `make` with your project's specific build commands if needed.

---

## **Option 2: Building PS2SDK from Source on Linux**

This method involves manually setting up the `ps2dev` environment on your Linux system and building the project natively.

### **Steps**:

1. **Set Up the Environment**:

   Ensure your user account has permissions to build the `ps2dev` environment:
   ```bash
   export PS2DEV=/usr/local/ps2dev
   sudo mkdir -p $PS2DEV
   sudo chown -R $USER: $PS2DEV
   ```

   Add the following lines to your `.bashrc` or `.zshrc` to persist them across sessions.
   ```bash
   export PS2DEV=/usr/local/ps2dev
   export PS2SDK=$PS2DEV/ps2sdk
   export GSKIT=$PS2DEV/gsKit
   export PATH=$PATH:$PS2DEV/bin:$PS2DEV/ee/bin:$PS2DEV/iop/bin:$PS2DEV/dvp/bin:$PS2SDK/bin
   ```


2. **Install Required Packages**:
   On Debian/Ubuntu, run:
   ```bash
   sudo apt update
   sudo apt -y install gcc g++ make cmake patch git texinfo flex bison gettext libgsl-dev libgmp3-dev libmpfr-dev libmpc-dev zlib1g-dev autopoint
   ```
   On Arch-based systems run:
   ```bash
   sudo pacman -Syu
   sudo pacman -S gcc make cmake patch git texinfo flex bison gettext gsl gmp mpfr libmpc zlib boost
   ```

3. **Clone the PS2DEV Repository**:
   ```bash
   git clone https://github.com/ps2dev/ps2dev.git
   cd ps2dev
   ```

3. **Build PS2SDK**:
   ```bash
   ./build-all.sh
   ```

   This will build the `ps2dev` libraries and tools required for the project. The build takes approximately 40-50 minutes. Build success may vary across different distributions, hence why [Option 1](#option-1-using-the-dev-containers-extension-in-vs-code-recommended) is recommended 

5. **Clone and Build the Project**:
   ```bash
   cd /path/to/your/workspace
   git clone https://github.com/mahee1981/ps2-graphics-demo.git
   cd ps2-graphics-demo
   make
   ```

   Replace `make` with the specific build commands for your project if necessary.

---

## **Testing Your Setup**

After building the project using either method:

- Run the compiled binaries or scripts as per your project’s requirements.
- If applicable, deploy to your target PS2 hardware or emulator.

---

## **Additional Notes**

- If you encounter any issues, ensure your paths (e.g., `PS2SDK`) are correctly set.
- For more details on the `ps2dev` and `ps2sdk`, refer to the official [PS2DEV Documentation](https://github.com/ps2dev/ps2dev).

---

## **Contributing**

We welcome contributions! Please follow these steps:

1. Fork the repository.
2. Create a feature branch: `git checkout -b my-feature`.
3. Commit your changes: `git commit -am 'Add my feature'`.
4. Push to the branch: `git push origin my-feature`.
5. Open a pull request.

---

## **License**

This project is licensed under the MIT License. See the `LICENSE` file for details.

---

### **What’s in the Dev Container?**

The `devcontainer.json` file is configured to:
- Install `ps2env` image using the `ps2dev/ps2dev:latest` as the base.
- Set up an environment with all necessary tools for building and running the project.
- Provide consistent and isolated builds across systems.

---