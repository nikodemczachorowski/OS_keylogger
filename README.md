# Win_Kernel_Input_Driver

## Overview
Win_Kernel_Input_Driver is a Windows keyboard device driver designed to log keystrokes. Developed using the Kernel-Mode Driver Framework (KMDF), this project intercepts hardware interrupts and communicates data between kernel space and user space.

**For a more detailed description of the project and development process see [Full Project Report (PL)](sprawozdanie/sprawozdanie.pdf).**

## Architecture
The project is divided into two main components:

* **Kernel-Mode Driver (`OS_keylogger.sys`):** Acts as an upper filter for the keyboard device. It intercepts the `IOCTL_INTERNAL_KEYBOARD_CONNECT` request to inject a custom callback function into the keyboard driver stack. Captured keystrokes are placed into a circular buffer protected by a spinlock, allowing safe access without suspending interrupt threads. A secondary control device exposes a symbolic link (`\\.\OS_keyloggerLink`) to communicate with the user-mode application.
* **User-Mode Application (`OS_keyloggerUserMode.exe`):** Polls the kernel control device using `DeviceIoControl` to retrieve stored keystrokes. It translates raw Scan Codes into readable text using a lookup table and writes the results to local text files.

## Prerequisites
* Windows environment (Windows 11 recommended; not tested on older systems.).
* Test Mode (`testsigning`) must be enabled on the target machine.
* A Virtual Machine with snapshot capabilities is highly recommended for safe testing, as kernel panic (BSOD) or keyboard lockouts can easily occur during development.

## Installation & Usage
To install and run the project, follow these steps:

1. Place all necessary files (`OS_keylogger.sys`, `OS_keylogger.inf`, `OS_keylogger.cat`, and `OS_keyloggerUserMode.exe`) into a single directory on the target machine.
2. Right-click the `OS_keylogger.inf` file and select **Install**.
3. Restart the computer for the driver stack changes to take effect.
4. Launch `OS_keyloggerUserMode.exe`. You can also add this executable to the Windows Startup folder manually to ensure it runs automatically.

## Output
Once installed and running, the application generates two files in User's Documents directory:
* `OS_keylogger_codes.txt`: Contains raw key data formatted as pairs of Scan Codes and flags.
* `OS_keylogger_text.txt`: Contains the human-readable text translated from the captured Scan Codes.