#pragma once
#include <windows.h>
#include <string>
#include <functional>
#include <atomic>
#include "iso_reader.h"
#include "win11_bypass.h"

struct WriteJobConfig {
    DWORD physicalDriveIndex = 0;
    std::wstring assignedDriveLetter; // e.g. L"E:"
    std::wstring isoPath;
    IsoMetadata isoMeta;
    std::wstring partitionScheme;     // L"GPT" or L"MBR"
    std::wstring targetSystem;        // L"UEFI (non-CSM)" or L"BIOS or UEFI-CSM"
    std::wstring fileSystem;          // L"FAT32", L"NTFS", L"exFAT"
    DWORD clusterSize = 4096;
    std::wstring volumeLabel = L"BOOTELWARE";
    bool checkBadBlocks = false;
    int badBlockPasses = 1;
    bool enableWin11Bypasses = true;
    Win11BypassOptions win11Options;
    bool ddMode = false;
};

typedef std::function<void(int percent, double speedMbSec, const std::wstring& taskDesc)> WriteProgressCallback;
typedef std::function<void(const std::wstring& logLine)> WriteLogCallback;

class DiskWriter {
public:
    static bool ExecuteWriteJob(
        const WriteJobConfig& config,
        WriteProgressCallback progressCb,
        WriteLogCallback logCb,
        std::atomic<bool>& cancelFlag,
        std::wstring& outErrorMessage
    );

    static bool FormatDrive(
        const std::wstring& driveLetter,
        const std::wstring& fs,
        const std::wstring& label,
        DWORD clusterSize,
        WriteLogCallback logCb,
        std::wstring& outError
    );

    static bool CheckBadBlocks(
        DWORD physicalDriveIndex,
        int passes,
        WriteProgressCallback progressCb,
        WriteLogCallback logCb,
        std::atomic<bool>& cancelFlag,
        std::wstring& outError
    );
};
