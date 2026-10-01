#include "system_detector.h"
#include <winioctl.h>
#include <vector>

SystemDriveInfo SystemDetector::DetectSystemDrive() {
    SystemDriveInfo info;
    info.detectedSuccessfully = false;

    // 1. Get Windows system directory
    wchar_t sysDir[MAX_PATH] = { 0 };
    if (!GetSystemDirectoryW(sysDir, MAX_PATH)) {
        info.systemDriveLetter = L"C:";
    } else {
        info.systemDriveLetter = std::wstring(1, sysDir[0]) + L":";
    }

    // 2. Open volume to retrieve physical device number
    std::wstring volPath = L"\\\\.\\" + info.systemDriveLetter;
    HANDLE hVol = CreateFileW(volPath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hVol != INVALID_HANDLE_VALUE) {
        STORAGE_DEVICE_NUMBER sdn = { 0 };
        DWORD bytesReturned = 0;
        if (DeviceIoControl(hVol, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &sdn, sizeof(sdn), &bytesReturned, NULL)) {
            info.physicalDriveIndex = sdn.DeviceNumber;
        }
        CloseHandle(hVol);
    }

    // 3. Open physical drive and query layout
    std::wstring physPath = L"\\\\.\\PhysicalDrive" + std::to_wstring(info.physicalDriveIndex);
    HANDLE hDisk = CreateFileW(physPath.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hDisk == INVALID_HANDLE_VALUE) {
        hDisk = CreateFileW(physPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    }

    if (hDisk != INVALID_HANDLE_VALUE) {
        std::vector<BYTE> buffer(4096, 0);
        DWORD bytesReturned = 0;
        if (DeviceIoControl(hDisk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, NULL, 0, buffer.data(), (DWORD)buffer.size(), &bytesReturned, NULL)) {
            auto layout = reinterpret_cast<DRIVE_LAYOUT_INFORMATION_EX*>(buffer.data());
            if (layout->PartitionStyle == PARTITION_STYLE_GPT) {
                info.isGpt = true;
                info.isMbr = false;
                info.partitionStyleName = L"GPT";
            } else if (layout->PartitionStyle == PARTITION_STYLE_MBR) {
                info.isGpt = false;
                info.isMbr = true;
                info.partitionStyleName = L"MBR";
            } else {
                info.isGpt = true;
                info.isMbr = false;
                info.partitionStyleName = L"GPT (Default)";
            }
            info.detectedSuccessfully = true;
        }

        // Query model / product ID
        STORAGE_PROPERTY_QUERY spq = { StorageDeviceProperty, PropertyStandardQuery };
        std::vector<BYTE> descBuf(2048, 0);
        if (DeviceIoControl(hDisk, IOCTL_STORAGE_QUERY_PROPERTY, &spq, sizeof(spq), descBuf.data(), (DWORD)descBuf.size(), &bytesReturned, NULL)) {
            auto desc = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(descBuf.data());
            if (desc->ProductIdOffset != 0) {
                const char* prod = reinterpret_cast<const char*>(descBuf.data() + desc->ProductIdOffset);
                std::string s(prod);
                // trim whitespace
                while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) s.pop_back();
                while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
                int len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, NULL, 0);
                if (len > 0) {
                    std::vector<wchar_t> wprod(len);
                    MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, wprod.data(), len);
                    info.diskModel = wprod.data();
                }
            }
        }
        CloseHandle(hDisk);
    }

    // 4. Query firmware type
    FIRMWARE_TYPE fwType = FirmwareTypeUnknown;
    typedef BOOL(WINAPI* PFN_GetFirmwareType)(PFIRMWARE_TYPE);
    PFN_GetFirmwareType pfnGetFirmwareType = (PFN_GetFirmwareType)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetFirmwareType");
    if (pfnGetFirmwareType && pfnGetFirmwareType(&fwType)) {
        if (fwType == FirmwareTypeUefi) {
            info.firmwareBootType = L"UEFI";
        } else if (fwType == FirmwareTypeBios) {
            info.firmwareBootType = L"BIOS";
        } else {
            info.firmwareBootType = L"Unknown";
        }
    } else {
        info.firmwareBootType = info.isGpt ? L"UEFI" : L"BIOS";
    }

    // 5. Formulate recommendations
    if (info.isGpt) {
        info.recommendedScheme = L"GPT";
        info.recommendedTarget = L"UEFI (non-CSM)";
    } else {
        info.recommendedScheme = L"MBR";
        info.recommendedTarget = L"BIOS or UEFI-CSM";
    }

    if (info.diskModel.empty()) {
        info.diskModel = L"System Disk";
    }

    info.summaryMessage = L"Detected: Windows is installed on a " + info.partitionStyleName + 
                          L" disk (" + info.firmwareBootType + L") -> Recommended settings applied";

    return info;
}
