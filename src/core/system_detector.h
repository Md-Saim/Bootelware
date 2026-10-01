#pragma once
#include <windows.h>
#include <string>

struct SystemDriveInfo {
    std::wstring systemDriveLetter;      // e.g. L"C:"
    DWORD physicalDriveIndex = 0;         // e.g. 2 for PhysicalDrive2
    bool isGpt = true;
    bool isMbr = false;
    std::wstring partitionStyleName;     // L"GPT" or L"MBR"
    std::wstring firmwareBootType;       // L"UEFI" or L"BIOS"
    std::wstring diskModel;              // e.g. L"HS-SSD-Desire(S) 128G"
    std::wstring recommendedScheme;      // L"GPT" or L"MBR"
    std::wstring recommendedTarget;      // L"UEFI (non-CSM)" or L"BIOS or UEFI-CSM"
    std::wstring summaryMessage;
    bool detectedSuccessfully = false;
};

class SystemDetector {
public:
    static SystemDriveInfo DetectSystemDrive();
};
