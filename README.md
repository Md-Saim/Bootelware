# Bootelware

<p align="center">
  <img src="resources/mascot_purple.png" alt="Bootelware Mascot" width="110" />
</p>

<p align="center">
  <strong>A fast, friendly, and foolproof bootable USB creator for Windows.</strong>
</p>

<p align="center">
  <a href="https://github.com/Md-Saim/Bootelware/blob/main/LICENSE"><img src="https://img.shields.io/badge/License-GPLv3-blue.svg" alt="License: GPL v3"></a>
  <img src="https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011-0078D6?logo=windows" alt="Platform">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599C?logo=cplusplus" alt="C++17">
  <img src="https://img.shields.io/badge/Size-%3C1%20MB%20Standalone-brightgreen" alt="Size">
</p>

---

## Overview

Bootelware is a lightweight, portable Windows utility designed to make creating bootable USB drives simple and stress-free. Built from scratch in pure C++ Win32 and GDI+, it combines a clean, playful interface with smart host hardware detection, Windows 11 setup customization, and high-speed write performance.

- **Friendly Design**: Rounded cards, tactile buttons, smooth animations, and Google Fonts **Fredoka** typography.
- **Smart Hardware Detection**: Inspects your PC's active disk to detect your partition style (`GPT` vs `MBR`) and boot mode (`UEFI` vs `BIOS`).
- **Windows 11 Customizer**: Skip TPM, Secure Boot, RAM requirements, and mandatory Microsoft accounts with a single click.
- **Built-in USB Formatter**: Easily format flash drives to FAT32, NTFS, or exFAT, overcoming Windows' default 32 GB FAT32 limitation.
- **Lightweight & Portable**: Fully self-contained under 1 MB with zero runtime dependencies.

---

## Screenshots

| **1. Choose Source** | **2. Select USB Drive** |
|:---:|:---:|
| ![1 Source](assets/screenshots/screen1_source.png) | ![2 Drive](assets/screenshots/screen2_drive.png) |
| Drag & drop your ISO file or browse local disks | Automatic USB detection with safety guards |

| **3. Tune Extras & Partition Scheme** | **3b. Partition Mismatch Warning** |
|:---:|:---:|
| ![3 Tune](assets/screenshots/screen3_tune.png) | ![3b Tune Mismatch](assets/screenshots/screen3_tune_mismatch.png) |
| Bypass Windows 11 limits & toggle GPT / MBR | Live warning pop banner if chosen scheme mismatches Host C: |

| **4. High-Speed Write** | **4b. Detailed Operation Log** |
|:---:|:---:|
| ![4 Write](assets/screenshots/screen4_write.png) | ![4b Write Details](assets/screenshots/screen4_write_details.png) |
| Live progress bar, speed indicator & checklist | Detailed unbuffered I/O log output |

| **5. Ready to Boot** | **Format USB Drive** |
|:---:|:---:|
| ![5 Done](assets/screenshots/screen5_done.png) | ![Format USB](assets/screenshots/screen_format_usb.png) |
| Safe ejection & straightforward boot instructions | Quick format utility with large FAT32 support (>32 GB) |

| **Menu** | **About Bootelware** | **About the Developer** |
|:---:|:---:|:---:|
| ![Menu](assets/screenshots/screen6_menu.png) | ![About App](assets/screenshots/screen7_about.png) | ![About Dev](assets/screenshots/screen8_developer.png) |
| Quick navigation & extra tools | Application details & version info | Developer profile & links |

---

## Features

### 🚀 Effortless Bootable Media Creation
- Works with Windows (11, 10, 8.1, 7) and Linux ISO distributions (Ubuntu, Debian, Fedora, Arch, etc.).
- Direct unbuffered disk writing (`FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH`) for maximum USB write speeds.
- Visual step checklist showing exact write stages (cleaning, copying, applying extras, double-checking).

### 🧠 Smart System Detection & Partition Guard
- Inspects your host Windows disk (`C:`) to determine whether your system runs **GPT (UEFI)** or **MBR (BIOS)**.
- Easily toggle between **GPT** and **MBR** directly on the Tune screen.
- Real-time mismatch detection: warns you if you choose an incompatible partition scheme for your host machine and offers a 1-click fix.

### 🪟 Windows 11 Setup Customization
- Bypass TPM 2.0, Secure Boot, and 4 GB RAM requirements automatically.
- Skip mandatory online Microsoft accounts (`BypassNRO`) to install with a clean local profile.
- Configure local administrator name in advance.
- Disable BitLocker automatic drive encryption.
- Turn off diagnostic telemetry collection.

### 💾 Built-in USB Formatting
- Quickly format flash drives directly to **FAT32**, **NTFS**, or **exFAT**.
- Custom cluster geometry unlocks FAT32 formatting on drives larger than 32 GB (64 GB, 128 GB, 256 GB+) for broader compatibility with UEFI hardware and embedded systems.
- 1-click quick format with custom volume labeling.

### 🛡️ Safety & Verification
- Host and non-removable internal drives are strictly locked to prevent accidental data loss.
- Cryptographic checksum verification (MD5, SHA-1, SHA-256) powered by Windows CNG (`bcrypt.dll`).
- Safe device ejection via `IOCTL_STORAGE_EJECT_MEDIA`.

---

## 🛠️ Project Structure

```
bootelware/
├── CMakeLists.txt              # CMake build config with static CRT
├── build.bat                   # Automated MSVC build script
├── assets/
│   ├── fonts/                  # Fredoka.ttf Google Font
│   └── screenshots/            # Exported UI screenshots for documentation
├── resources/
│   ├── app.manifest            # DPI awareness & Common Controls 6
│   ├── app_icon.ico            # High-res multi-layer application icon
│   ├── app_logo.bmp            # Mascot logo bitmap
│   ├── mascot_purple.png       # Purple mascot asset
│   ├── mascot_green.png        # Success smiling mascot asset
│   ├── dev_avatar.png          # Developer profile avatar
│   ├── Fredoka.ttf             # Embedded Fredoka variable font
│   ├── resource.h              # Resource identifiers (IDR_FONT_FREDOKA = 106)
│   └── bootelware.rc           # Windows resource definition (RCDATA embedded)
└── src/
    ├── main.cpp                # Application entry point & headless screenshot exporter
    ├── core/
    │   ├── system_detector.*   # System drive GPT/MBR & UEFI/BIOS detection
    │   ├── device_manager.*    # USB enumeration, volume mapping, safety guards
    │   ├── checksum.*          # Hardware-accelerated hash engine
    │   ├── iso_reader.*        # ISO 9660, UDF, El Torito, virtdisk mounting
    │   ├── win11_bypass.*      # autounattend.xml answer-file generator
    │   └── disk_writer.*       # Partitioning, Large FAT32 writer, unbuffered I/O
    └── ui/
        ├── dark_theme.*        # Neo-brutalist theme, Fredoka loader & tactile buttons
        ├── dialogs.*           # Checksums and helper modals
        └── main_window.*       # 8-screen wizard canvas, mascot animation & event pump
```

---

## 🔨 How to Build

**Requirements:** Visual Studio 2019+ with C++ Desktop Development workload, CMake 3.15+.

### Quick Build
```cmd
build.bat
```

### Manual Build
```cmd
cmake -B build -A x64
cmake --build build --config Release
```

The standalone executable is generated at:
```
build/Release/Bootelware.exe
```

### Export Documentation Screenshots
Bootelware includes a built-in headless renderer to capture high-res screenshots of all screens:
```cmd
build\Release\Bootelware.exe export-screens
```

---

## 📋 Technical Details

| Property | Value |
|---|---|
| **Language** | C++17 |
| **UI Framework** | Pure Win32 + GDI+ (zero webviews, zero heavy dependencies) |
| **Typography** | Fredoka (Google Fonts, embedded in binary) |
| **Disk I/O** | Direct unbuffered sector I/O, `SetupAPI`, `DeviceIoControl` |
| **Hashing Engine** | Windows CNG (`bcrypt.dll`) |
| **CRT Linkage** | Static (`/MT`) |
| **Binary Size** | < 1 MB standalone executable |
| **Target OS** | Windows 10 / 11 (x64) |
| **Portability** | Single portable `.exe` (no installation required) |

---

## 👨‍💻 Author

**Md Saim**  
Software Engineer & Creator  
- 🌐 [GitHub: @Md-Saim](https://github.com/Md-Saim)  
- 📧 [lakhvisaim@gmail.com](mailto:lakhvisaim@gmail.com)  

---

## 📄 License

This project is licensed under the **GNU General Public License v3.0** — see the [LICENSE](LICENSE) file for details.
