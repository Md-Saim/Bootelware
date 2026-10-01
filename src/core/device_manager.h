#pragma once
#include <windows.h>
#include <string>
#include <vector>

struct TargetDeviceInfo {
    DWORD physicalDriveIndex = 0;       // e.g. 3 for PhysicalDrive3
    std::wstring devicePath;            // L"\\\\.\\PhysicalDrive3"
    std::wstring displayName;           // e.g. L"E: [32 GB] SanDisk Ultra (PhysicalDrive3)"
    std::wstring model;                 // e.g. L"SanDisk Ultra"
    std::wstring assignedDriveLetters;  // e.g. L"E:"
    std::wstring currentFileSystem;     // e.g. L"FAT32"
    std::wstring currentVolumeLabel;    // e.g. L"BOOTELWARE"
    uint64_t totalSizeBytes = 0;
    double totalSizeGb = 0.0;
    bool isUsb = false;
    bool isRemovable = false;
    bool isSystemDisk = false;
};

class DeviceManager {
public:
    static std::vector<TargetDeviceInfo> EnumerateTargetDrives(DWORD systemDiskIndex, bool listAllUsbDrives = true);
    static bool LockAndDismountVolumes(DWORD physicalDriveIndex, std::wstring& outError);
    static bool GetDriveCapacity(DWORD physicalDriveIndex, uint64_t& outBytes);
};
