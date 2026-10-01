#include "device_manager.h"
#include <winioctl.h>
#include <map>
#include <iomanip>
#include <sstream>

std::vector<TargetDeviceInfo> DeviceManager::EnumerateTargetDrives(DWORD systemDiskIndex, bool listAllUsbDrives) {
    std::vector<TargetDeviceInfo> results;

    // 1. Build volume letter mapping to physical drives
    struct VolInfo {
        std::wstring letter;
        std::wstring label;
        std::wstring fs;
        uint64_t totalBytes = 0;
    };
    std::map<DWORD, std::vector<VolInfo>> driveToVolumes;

    wchar_t drives[512] = { 0 };
    if (GetLogicalDriveStringsW(512, drives)) {
        wchar_t* p = drives;
        while (*p) {
            std::wstring drive = p; // e.g. "E:\"
            std::wstring volPath = L"\\\\.\\" + drive.substr(0, 2);

            HANDLE h = CreateFileW(volPath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
            if (h != INVALID_HANDLE_VALUE) {
                STORAGE_DEVICE_NUMBER sdn = { 0 };
                DWORD bytes = 0;
                if (DeviceIoControl(h, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &sdn, sizeof(sdn), &bytes, NULL)) {
                    VolInfo vi;
                    vi.letter = drive.substr(0, 2);

                    wchar_t volName[MAX_PATH] = { 0 };
                    wchar_t fsName[MAX_PATH] = { 0 };
                    DWORD serial = 0, maxComponent = 0, flags = 0;
                    GetVolumeInformationW(drive.c_str(), volName, MAX_PATH, &serial, &maxComponent, &flags, fsName, MAX_PATH);
                    vi.label = volName;
                    vi.fs = fsName;

                    ULARGE_INTEGER freeBytes, totalBytes, totalFree;
                    if (GetDiskFreeSpaceExW(drive.c_str(), &freeBytes, &totalBytes, &totalFree)) {
                        vi.totalBytes = totalBytes.QuadPart;
                    }

                    driveToVolumes[sdn.DeviceNumber].push_back(vi);
                }
                CloseHandle(h);
            }
            p += wcslen(p) + 1;
        }
    }

    // 2. Iterate PhysicalDrives 0 through 32
    for (DWORD i = 0; i < 32; ++i) {
        // Strict safety check: NEVER list the system drive
        if (i == systemDiskIndex) {
            continue;
        }

        std::wstring physPath = L"\\\\.\\PhysicalDrive" + std::to_wstring(i);
        HANDLE hDisk = CreateFileW(physPath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (hDisk == INVALID_HANDLE_VALUE) {
            continue;
        }

        STORAGE_PROPERTY_QUERY spq = { StorageDeviceProperty, PropertyStandardQuery };
        std::vector<BYTE> descBuf(2048, 0);
        DWORD bytes = 0;
        if (DeviceIoControl(hDisk, IOCTL_STORAGE_QUERY_PROPERTY, &spq, sizeof(spq), descBuf.data(), (DWORD)descBuf.size(), &bytes, NULL)) {
            auto desc = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(descBuf.data());

            bool isUsb = (desc->BusType == BusTypeUsb || desc->BusType == BusTypeSd || desc->BusType == BusTypeMmc);
            bool isRemovable = (desc->RemovableMedia != FALSE);

            // Filter: by default, show USB drives and removable drives
            if (isUsb || isRemovable || listAllUsbDrives) {
                // If it's a fixed internal SATA/NVMe drive and not USB, skip it unless specifically requested
                if (!isUsb && !isRemovable) {
                    CloseHandle(hDisk);
                    continue;
                }

                TargetDeviceInfo info;
                info.physicalDriveIndex = i;
                info.devicePath = physPath;
                info.isUsb = isUsb;
                info.isRemovable = isRemovable;
                info.isSystemDisk = false;

                // Model name
                if (desc->ProductIdOffset != 0) {
                    const char* prod = reinterpret_cast<const char*>(descBuf.data() + desc->ProductIdOffset);
                    std::string s(prod);
                    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) s.pop_back();
                    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
                    int len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, NULL, 0);
                    if (len > 0) {
                        std::vector<wchar_t> wprod(len);
                        MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, wprod.data(), len);
                        info.model = wprod.data();
                    }
                }
                if (info.model.empty()) {
                    info.model = L"USB Drive";
                }

                // Total size
                uint64_t cap = 0;
                if (GetDriveCapacity(i, cap)) {
                    info.totalSizeBytes = cap;
                    info.totalSizeGb = (double)cap / (1024.0 * 1024.0 * 1024.0);
                }

                // Associate volumes
                if (driveToVolumes.find(i) != driveToVolumes.end()) {
                    const auto& vols = driveToVolumes[i];
                    std::wstring letters;
                    std::wstring labels;
                    for (size_t v = 0; v < vols.size(); ++v) {
                        if (v > 0) letters += L", ";
                        letters += vols[v].letter;
                        if (!vols[v].label.empty()) {
                            if (!labels.empty()) labels += L" / ";
                            labels += vols[v].label;
                        }
                        if (info.totalSizeBytes == 0 && vols[v].totalBytes > 0) {
                            info.totalSizeBytes = vols[v].totalBytes;
                            info.totalSizeGb = (double)vols[v].totalBytes / (1024.0 * 1024.0 * 1024.0);
                        }
                        if (!vols[v].fs.empty()) {
                            info.currentFileSystem = vols[v].fs;
                        }
                    }
                    info.assignedDriveLetters = letters;
                    info.currentVolumeLabel = labels;
                }

                // Build display name
                std::wstringstream ss;
                if (!info.assignedDriveLetters.empty()) {
                    ss << info.assignedDriveLetters << L" ";
                }
                ss << L"[" << std::fixed << std::setprecision(1) << info.totalSizeGb << L" GB] ";
                ss << info.model << L" (Drive " << i << L")";
                info.displayName = ss.str();

                results.push_back(info);
            }
        }
        CloseHandle(hDisk);
    }

    return results;
}

bool DeviceManager::GetDriveCapacity(DWORD physicalDriveIndex, uint64_t& outBytes) {
    std::wstring physPath = L"\\\\.\\PhysicalDrive" + std::to_wstring(physicalDriveIndex);
    HANDLE hDisk = CreateFileW(physPath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hDisk == INVALID_HANDLE_VALUE) {
        return false;
    }

    DISK_GEOMETRY_EX geom = { 0 };
    DWORD bytes = 0;
    if (DeviceIoControl(hDisk, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &geom, sizeof(geom), &bytes, NULL)) {
        outBytes = geom.DiskSize.QuadPart;
        CloseHandle(hDisk);
        return true;
    }

    // Try GET_LENGTH_INFORMATION
    GET_LENGTH_INFORMATION gli = { 0 };
    if (DeviceIoControl(hDisk, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &gli, sizeof(gli), &bytes, NULL)) {
        outBytes = gli.Length.QuadPart;
        CloseHandle(hDisk);
        return true;
    }

    CloseHandle(hDisk);
    return false;
}

bool DeviceManager::LockAndDismountVolumes(DWORD physicalDriveIndex, std::wstring& outError) {
    wchar_t drives[512] = { 0 };
    if (!GetLogicalDriveStringsW(512, drives)) {
        return true;
    }

    wchar_t* p = drives;
    while (*p) {
        std::wstring drive = p;
        std::wstring volPath = L"\\\\.\\" + drive.substr(0, 2);

        HANDLE h = CreateFileW(volPath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            STORAGE_DEVICE_NUMBER sdn = { 0 };
            DWORD bytes = 0;
            if (DeviceIoControl(h, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &sdn, sizeof(sdn), &bytes, NULL)) {
                if (sdn.DeviceNumber == physicalDriveIndex) {
                    // Lock volume
                    DWORD dwBytesReturned = 0;
                    if (!DeviceIoControl(h, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dwBytesReturned, NULL)) {
                        // Volume might be in use
                    }
                    // Dismount volume
                    DeviceIoControl(h, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &dwBytesReturned, NULL);
                }
            }
            CloseHandle(h);
        }
        p += wcslen(p) + 1;
    }
    return true;
}
