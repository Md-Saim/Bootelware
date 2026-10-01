#include "iso_reader.h"
#include <virtdisk.h>
#include <winioctl.h>
#include <iostream>
#include <vector>
#include <algorithm>

#pragma comment(lib, "virtdisk.lib")

// Helper to snapshot active logical drive letters
static std::vector<std::wstring> GetCurrentDriveLetters() {
    std::vector<std::wstring> letters;
    wchar_t drives[512] = { 0 };
    if (GetLogicalDriveStringsW(512, drives)) {
        wchar_t* p = drives;
        while (*p) {
            letters.push_back(std::wstring(p, 2)); // e.g. "C:"
            p += wcslen(p) + 1;
        }
    }
    return letters;
}

IsoMetadata IsoReader::AnalyzeIso(const std::wstring& isoPath) {
    IsoMetadata meta;
    meta.filePath = isoPath;
    meta.isValid = false;

    // Extract file name
    size_t lastSlash = isoPath.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        meta.fileName = isoPath.substr(lastSlash + 1);
    } else {
        meta.fileName = isoPath;
    }

    HANDLE hFile = CreateFileW(isoPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        meta.errorDescription = L"Could not open ISO file.";
        return meta;
    }

    LARGE_INTEGER size;
    if (GetFileSizeEx(hFile, &size)) {
        meta.fileSizeBytes = size.QuadPart;
        meta.fileSizeGb = (double)size.QuadPart / (1024.0 * 1024.0 * 1024.0);
    }

    // Read ISO 9660 Primary Volume Descriptor at offset 0x8000 (32768)
    const DWORD ISO_PVD_OFFSET = 32768;
    LARGE_INTEGER liOffset;
    liOffset.QuadPart = ISO_PVD_OFFSET;
    if (SetFilePointerEx(hFile, liOffset, NULL, FILE_BEGIN)) {
        BYTE pvdBuf[2048] = { 0 };
        DWORD bytesRead = 0;
        if (ReadFile(hFile, pvdBuf, sizeof(pvdBuf), &bytesRead, NULL) && bytesRead == sizeof(pvdBuf)) {
            // Check for standard "CD001" or "BEA01"
            if (memcmp(pvdBuf + 1, "CD001", 5) == 0 || memcmp(pvdBuf + 1, "BEA01", 5) == 0 || memcmp(pvdBuf + 1, "NSR02", 5) == 0) {
                meta.isValid = true;
                // Volume Identifier is at offset 40 (32 chars)
                char volId[33] = { 0 };
                memcpy(volId, pvdBuf + 40, 32);
                // trim trailing spaces
                for (int i = 31; i >= 0 && (volId[i] == ' ' || volId[i] == '\0'); --i) volId[i] = '\0';
                int len = MultiByteToWideChar(CP_OEMCP, 0, volId, -1, NULL, 0);
                if (len > 0) {
                    std::vector<wchar_t> wvol(len);
                    MultiByteToWideChar(CP_OEMCP, 0, volId, -1, wvol.data(), len);
                    meta.volumeLabel = wvol.data();
                }
            }
        }
    }
    CloseHandle(hFile);

    if (meta.fileSizeBytes > 10 * 1024 * 1024) {
        meta.isValid = true; // Minimum sanity check
    }

    // Inspect files via quick temporary mount
    HANDLE hVhd = INVALID_HANDLE_VALUE;
    std::wstring mountedLetter;
    std::wstring err;
    if (MountIso(isoPath, hVhd, mountedLetter, err)) {
        std::wstring root = mountedLetter + L"\\";

        // Check for Windows Setup
        std::wstring bootWim = root + L"sources\\boot.wim";
        std::wstring installWim = root + L"sources\\install.wim";
        std::wstring installEsd = root + L"sources\\install.esd";
        std::wstring efiBoot = root + L"EFI\\BOOT\\BOOTX64.EFI";
        std::wstring efiBoot32 = root + L"EFI\\BOOT\\BOOTIA32.EFI";

        if (GetFileAttributesW(efiBoot.c_str()) != INVALID_FILE_ATTRIBUTES ||
            GetFileAttributesW(efiBoot32.c_str()) != INVALID_FILE_ATTRIBUTES) {
            meta.hasEfiBoot = true;
        }

        if (GetFileAttributesW(bootWim.c_str()) != INVALID_FILE_ATTRIBUTES) {
            meta.hasBiosBoot = true;
            // It's a Windows ISO!
            WIN32_FILE_ATTRIBUTE_DATA fad;
            if (GetFileAttributesExW(installWim.c_str(), GetFileExInfoStandard, &fad)) {
                meta.hasInstallWim = true;
                ULARGE_INTEGER ul;
                ul.HighPart = fad.nFileSizeHigh;
                ul.LowPart = fad.nFileSizeLow;
                meta.installWimSize = ul.QuadPart;
                if (meta.installWimSize >= 0xFFFFFFFFULL) { // >= 4GB
                    meta.requiresWimSplit = true;
                }
            } else if (GetFileAttributesExW(installEsd.c_str(), GetFileExInfoStandard, &fad)) {
                meta.hasInstallEsd = true;
            }

            // Detect Windows 11 vs 10
            std::wstring upperName = meta.fileName;
            std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::towlower);
            if (upperName.find(L"win11") != std::wstring::npos || 
                upperName.find(L"windows11") != std::wstring::npos ||
                upperName.find(L"22621") != std::wstring::npos ||
                upperName.find(L"22631") != std::wstring::npos ||
                upperName.find(L"26100") != std::wstring::npos ||
                meta.volumeLabel.find(L"CCCOMA_X64") != std::wstring::npos) {
                meta.detectedType = IsoType::Windows11;
                meta.osName = L"Windows 11 (Setup)";
            } else {
                meta.detectedType = IsoType::Windows10;
                meta.osName = L"Windows 10 / Setup";
            }
        } else if (GetFileAttributesW((root + L"isolinux").c_str()) != INVALID_FILE_ATTRIBUTES ||
                   GetFileAttributesW((root + L"casper").c_str()) != INVALID_FILE_ATTRIBUTES ||
                   GetFileAttributesW((root + L"live").c_str()) != INVALID_FILE_ATTRIBUTES ||
                   GetFileAttributesW((root + L"arch").c_str()) != INVALID_FILE_ATTRIBUTES) {
            meta.detectedType = IsoType::LinuxLive;
            meta.osName = L"Linux (Live / Installer)";
            meta.hasBiosBoot = true;
        } else if (meta.hasEfiBoot) {
            meta.detectedType = IsoType::GenericEfi;
            meta.osName = L"UEFI Bootable Image";
        } else {
            meta.detectedType = IsoType::Unknown;
            meta.osName = L"Standard ISO / Disk Image";
        }

        UnmountIso(hVhd, isoPath);
    } else {
        // Fallback detection based on filename if mount unavailable
        std::wstring upperName = meta.fileName;
        std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::towlower);
        if (upperName.find(L"win11") != std::wstring::npos || upperName.find(L"windows11") != std::wstring::npos) {
            meta.detectedType = IsoType::Windows11;
            meta.osName = L"Windows 11 (Inferred)";
            meta.hasEfiBoot = true;
            meta.requiresWimSplit = true;
        } else if (upperName.find(L"win10") != std::wstring::npos || upperName.find(L"windows10") != std::wstring::npos) {
            meta.detectedType = IsoType::Windows10;
            meta.osName = L"Windows 10 (Inferred)";
            meta.hasEfiBoot = true;
        } else if (upperName.find(L"ubuntu") != std::wstring::npos || upperName.find(L"linux") != std::wstring::npos || upperName.find(L"debian") != std::wstring::npos) {
            meta.detectedType = IsoType::LinuxLive;
            meta.osName = L"Linux Distribution (Inferred)";
            meta.hasEfiBoot = true;
        } else {
            meta.detectedType = IsoType::Unknown;
            meta.osName = L"Bootable Disc Image";
        }
    }

    if (meta.volumeLabel.empty()) {
        meta.volumeLabel = L"BOOTELWARE";
    }

    return meta;
}

bool IsoReader::MountIso(const std::wstring& isoPath, HANDLE& outVhdHandle, std::wstring& outMountedDriveLetter, std::wstring& outError) {
    outVhdHandle = INVALID_HANDLE_VALUE;
    outMountedDriveLetter.clear();

    static const GUID s_VirtualStorageVendorMicrosoft = { 0xEC984AEC, 0xA0F9, 0x47e9, { 0x90, 0x1F, 0x71, 0xAA, 0xC2, 0x9A, 0x16, 0x36 } };
    VIRTUAL_STORAGE_TYPE storageType;
    storageType.DeviceId = VIRTUAL_STORAGE_TYPE_DEVICE_ISO;
    storageType.VendorId = s_VirtualStorageVendorMicrosoft;

    OPEN_VIRTUAL_DISK_PARAMETERS openParams;
    ZeroMemory(&openParams, sizeof(openParams));
    openParams.Version = OPEN_VIRTUAL_DISK_VERSION_1;
    openParams.Version1.RWDepth = OPEN_VIRTUAL_DISK_RW_DEPTH_DEFAULT;

    auto preDrives = GetCurrentDriveLetters();

    DWORD openResult = OpenVirtualDisk(&storageType, isoPath.c_str(), 
                                       VIRTUAL_DISK_ACCESS_READ, 
                                       OPEN_VIRTUAL_DISK_FLAG_NONE, 
                                       &openParams, &outVhdHandle);
    if (openResult != ERROR_SUCCESS) {
        outError = L"OpenVirtualDisk failed. Error: " + std::to_wstring(openResult);
        return false;
    }

    ATTACH_VIRTUAL_DISK_PARAMETERS attachParams;
    ZeroMemory(&attachParams, sizeof(attachParams));
    attachParams.Version = ATTACH_VIRTUAL_DISK_VERSION_1;

    DWORD attachResult = AttachVirtualDisk(outVhdHandle, NULL, 
                                           ATTACH_VIRTUAL_DISK_FLAG_READ_ONLY | ATTACH_VIRTUAL_DISK_FLAG_PERMANENT_LIFETIME, 
                                           0, &attachParams, NULL);
    if (attachResult != ERROR_SUCCESS) {
        CloseHandle(outVhdHandle);
        outVhdHandle = INVALID_HANDLE_VALUE;
        outError = L"AttachVirtualDisk failed. Error: " + std::to_wstring(attachResult);
        return false;
    }

    // Wait for drive letter to appear
    for (int retry = 0; retry < 25; ++retry) {
        Sleep(100);
        auto postDrives = GetCurrentDriveLetters();
        for (const auto& d : postDrives) {
            bool foundInPre = false;
            for (const auto& pd : preDrives) {
                if (pd == d) { foundInPre = true; break; }
            }
            if (!foundInPre) {
                outMountedDriveLetter = d;
                return true;
            }
        }
    }

    // If no new letter found, check CD-ROM drives
    wchar_t drives[512] = { 0 };
    if (GetLogicalDriveStringsW(512, drives)) {
        wchar_t* p = drives;
        while (*p) {
            if (GetDriveTypeW(p) == DRIVE_CDROM) {
                outMountedDriveLetter = std::wstring(p, 2);
                return true;
            }
            p += wcslen(p) + 1;
        }
    }

    return true;
}

bool IsoReader::UnmountIso(HANDLE hVhd, const std::wstring& isoPath) {
    if (hVhd != INVALID_HANDLE_VALUE) {
        DetachVirtualDisk(hVhd, DETACH_VIRTUAL_DISK_FLAG_NONE, 0);
        CloseHandle(hVhd);
        return true;
    }
    return false;
}
