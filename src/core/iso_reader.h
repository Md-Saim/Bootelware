#pragma once
#include <windows.h>
#include <string>
#include <vector>

enum class IsoType {
    Unknown,
    Windows11,
    Windows10,
    WindowsLegacy,
    LinuxLive,
    FreeDOS,
    GenericEfi
};

struct IsoMetadata {
    std::wstring filePath;
    std::wstring fileName;
    uint64_t fileSizeBytes = 0;
    double fileSizeGb = 0.0;
    std::wstring volumeLabel;
    IsoType detectedType = IsoType::Unknown;
    std::wstring osName;
    bool hasEfiBoot = false;
    bool hasBiosBoot = false;
    bool hasInstallWim = false;
    bool hasInstallEsd = false;
    uint64_t installWimSize = 0;
    bool requiresWimSplit = false; // true if install.wim > 4GB on FAT32
    bool isValid = false;
    std::wstring errorDescription;
};

class IsoReader {
public:
    static IsoMetadata AnalyzeIso(const std::wstring& isoPath);
    static bool MountIso(const std::wstring& isoPath, HANDLE& outVhdHandle, std::wstring& outMountedDriveLetter, std::wstring& outError);
    static bool UnmountIso(HANDLE hVhd, const std::wstring& isoPath);
};
