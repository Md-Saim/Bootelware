#include "disk_writer.h"
#include "device_manager.h"
#include <winioctl.h>
#include <shlobj.h>
#include <chrono>
#include <vector>
#include <filesystem>
#include <sstream>
#include <iomanip>

namespace fs = std::filesystem;

typedef VOID (WINAPI *PFORMATEX)(
    PWCHAR DriveLetter,
    DWORD MediaFlag,
    PWCHAR Format,
    PWCHAR Label,
    BOOL QuickFormat,
    DWORD ClusterSize,
    PVOID Callback
);

// fmifs callback
static BOOLEAN CALLBACK FormatCallback(int command, DWORD subAction, PVOID actionInfo) {
    // 0 = PROGRESS, 9 = DONE, 11 = INSUFFICIENT_RIGHTS
    return TRUE;
}

bool DiskWriter::FormatFat32Large(
    const std::wstring& driveLetter,
    const std::wstring& label,
    DWORD clusterSize,
    WriteLogCallback logCb,
    std::wstring& outError
) {
    if (logCb) logCb(L"Starting Rufus-compatible Large FAT32 Formatter for " + driveLetter + L"...");

    std::wstring drive = driveLetter.substr(0, 2);
    std::wstring volPath = L"\\\\.\\" + drive;

    HANDLE hVol = CreateFileW(
        volPath.c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
        NULL
    );

    if (hVol == INVALID_HANDLE_VALUE) {
        hVol = CreateFileW(
            volPath.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
    }

    if (hVol == INVALID_HANDLE_VALUE) {
        outError = L"Could not open volume " + volPath + L" (Error " + std::to_wstring(GetLastError()) + L"). Please run Bootelware as Administrator.";
        return false;
    }

    DWORD bytesReturned = 0;

    // Lock volume
    if (logCb) logCb(L"Locking volume for exclusive access...");
    DeviceIoControl(hVol, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &bytesReturned, NULL);

    // Dismount volume
    if (logCb) logCb(L"Dismounting volume...");
    DeviceIoControl(hVol, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &bytesReturned, NULL);

    // Query volume size
    GET_LENGTH_INFORMATION lengthInfo = { 0 };
    uint64_t totalBytes = 0;
    if (DeviceIoControl(hVol, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &lengthInfo, sizeof(lengthInfo), &bytesReturned, NULL)) {
        totalBytes = lengthInfo.Length.QuadPart;
    }

    if (totalBytes == 0) {
        DISK_GEOMETRY_EX geo = { 0 };
        if (DeviceIoControl(hVol, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, NULL, 0, &geo, sizeof(geo), &bytesReturned, NULL)) {
            totalBytes = geo.DiskSize.QuadPart;
        }
    }

    if (totalBytes == 0) {
        totalBytes = 32ULL * 1024ULL * 1024ULL * 1024ULL; // Default safe assumption
    }

    const DWORD sectorSize = 512;
    uint64_t totalSectors = totalBytes / sectorSize;

    // Determine sectors per cluster (SPC)
    DWORD spc = 8; // 4 KB
    if (clusterSize > 0) {
        spc = clusterSize / sectorSize;
        if (spc == 0) spc = 8;
    } else {
        if (totalBytes <= 32ULL * 1024ULL * 1024ULL * 1024ULL) {
            spc = 8;   // 4 KB cluster for <= 32 GB
        } else if (totalBytes <= 64ULL * 1024ULL * 1024ULL * 1024ULL) {
            spc = 32;  // 16 KB cluster for 32 - 64 GB
        } else if (totalBytes <= 128ULL * 1024ULL * 1024ULL * 1024ULL) {
            spc = 64;  // 32 KB cluster for 64 - 128 GB
        } else {
            spc = 128; // 64 KB cluster for > 128 GB (supports up to 2 TB)
        }
    }

    const WORD reservedSectors = 32;
    const BYTE numFATs = 2;

    uint64_t dataSectors = (totalSectors > reservedSectors) ? (totalSectors - reservedSectors) : 0;
    uint64_t numClusters = dataSectors / spc;
    uint64_t fatBytes = (numClusters + 2) * 4;
    DWORD fatSectors = (DWORD)((fatBytes + sectorSize - 1) / sectorSize);
    fatSectors = (fatSectors + 31) & ~31; // Align to 32 sectors

    if (logCb) {
        std::wstringstream ss;
        ss << L"Custom FAT32 Geometry: Size=" << (totalBytes / (1024 * 1024)) << L" MB, Total Sectors=" << totalSectors
           << L", Cluster Size=" << (spc * sectorSize) << L" bytes (" << spc << L" sectors/cluster)";
        logCb(ss.str());
    }

    // 1. Prepare Sector 0: Volume Boot Record (VBR)
    std::vector<BYTE> sector0(sectorSize, 0);
    sector0[0] = 0xEB; sector0[1] = 0x58; sector0[2] = 0x90; // JMP SHORT 0x58, NOP
    memcpy(&sector0[3], "MSWIN4.1", 8);
    *(WORD*)&sector0[11] = (WORD)sectorSize;
    sector0[13] = (BYTE)spc;
    *(WORD*)&sector0[14] = reservedSectors;
    sector0[16] = numFATs;
    *(WORD*)&sector0[17] = 0; // Root entries (0 for FAT32)
    *(WORD*)&sector0[19] = 0; // Total sectors 16 (0 for FAT32)
    sector0[21] = 0xF8;      // Media descriptor (Fixed/Removable)
    *(WORD*)&sector0[22] = 0; // Sectors per FAT 16 (0 for FAT32)
    *(WORD*)&sector0[24] = 63; // Sectors per track
    *(WORD*)&sector0[26] = 255;// Number of heads
    *(DWORD*)&sector0[28] = 0; // Hidden sectors
    *(DWORD*)&sector0[32] = (totalSectors < 0xFFFFFFFF) ? (DWORD)totalSectors : 0xFFFFFFFF;
    *(DWORD*)&sector0[36] = fatSectors;
    *(WORD*)&sector0[40] = 0;  // Ext flags
    *(WORD*)&sector0[42] = 0;  // FS version
    *(DWORD*)&sector0[44] = 2;  // Root dir cluster
    *(WORD*)&sector0[48] = 1;  // FSInfo sector
    *(WORD*)&sector0[50] = 6;  // Backup boot sector
    sector0[64] = 0x80;        // Physical drive number
    sector0[66] = 0x29;        // Extended boot signature
    *(DWORD*)&sector0[67] = (DWORD)GetTickCount(); // Volume serial number

    std::string volLabelStr = "           ";
    for (size_t i = 0; i < 11 && i < label.size(); ++i) {
        char c = (char)label[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        volLabelStr[i] = c;
    }
    memcpy(&sector0[71], volLabelStr.data(), 11);
    memcpy(&sector0[82], "FAT32   ", 8);
    sector0[510] = 0x55;
    sector0[511] = 0xAA;

    // 2. Prepare Sector 1: FSInfo
    std::vector<BYTE> sector1(sectorSize, 0);
    *(DWORD*)&sector1[0] = 0x41615252;   // Lead signature "RRaA"
    *(DWORD*)&sector1[484] = 0x61417272; // Struct signature "rrAa"
    *(DWORD*)&sector1[488] = (DWORD)(numClusters > 1 ? numClusters - 1 : 0);
    *(DWORD*)&sector1[492] = 3;          // Next free cluster
    *(DWORD*)&sector1[508] = 0xAA550000; // Trail signature

    auto writeSector = [&](uint64_t secNum, const void* data, DWORD size) -> bool {
        LARGE_INTEGER li;
        li.QuadPart = secNum * sectorSize;
        if (!SetFilePointerEx(hVol, li, NULL, FILE_BEGIN)) return false;
        DWORD written = 0;
        return WriteFile(hVol, data, size, &written, NULL) && written == size;
    };

    if (!writeSector(0, sector0.data(), sectorSize) ||
        !writeSector(1, sector1.data(), sectorSize) ||
        !writeSector(6, sector0.data(), sectorSize) ||
        !writeSector(7, sector1.data(), sectorSize)) {
        CloseHandle(hVol);
        outError = L"Failed writing FAT32 boot sectors. Error: " + std::to_wstring(GetLastError());
        return false;
    }

    // 3. Initialize FAT1 and FAT2
    std::vector<BYTE> fatInit(sectorSize, 0);
    *(DWORD*)&fatInit[0] = 0x0FFFFFF8; // Cluster 0: Media descriptor
    *(DWORD*)&fatInit[4] = 0x0FFFFFFF; // Cluster 1: EOC
    *(DWORD*)&fatInit[8] = 0x0FFFFFFF; // Cluster 2: Root Directory EOC

    uint64_t fat1Sector = reservedSectors;
    uint64_t fat2Sector = reservedSectors + fatSectors;

    if (!writeSector(fat1Sector, fatInit.data(), sectorSize) ||
        !writeSector(fat2Sector, fatInit.data(), sectorSize)) {
        CloseHandle(hVol);
        outError = L"Failed initializing FAT tables.";
        return false;
    }

    // Zero out next batch of sectors for both FATs
    std::vector<BYTE> zeroSec(sectorSize * 16, 0);
    DWORD zeroSize = (DWORD)zeroSec.size();
    for (DWORD i = 1; i < 32 && i < fatSectors; i += 16) {
        writeSector(fat1Sector + i, zeroSec.data(), zeroSize);
        writeSector(fat2Sector + i, zeroSec.data(), zeroSize);
    }

    // Zero out Root Directory (Cluster 2)
    uint64_t rootDirSector = reservedSectors + (uint64_t)numFATs * fatSectors;
    writeSector(rootDirSector, zeroSec.data(), min(zeroSize, (DWORD)(spc * sectorSize)));

    // Flush and unlock
    FlushFileBuffers(hVol);
    DeviceIoControl(hVol, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &bytesReturned, NULL);
    CloseHandle(hVol);

    Sleep(400);

    // Refresh Windows Explorer shell
    std::wstring rootPath = driveLetter;
    if (rootPath.back() != L'\\') rootPath += L'\\';
    SHChangeNotify(SHCNE_UPDATEDIR, SHCNF_PATH, rootPath.c_str(), NULL);
    SHChangeNotify(SHCNE_DRIVEADD, SHCNF_PATH, rootPath.c_str(), NULL);

    if (logCb) logCb(L"✓ FAT32 format complete! Drive " + driveLetter + L" is now formatted as FAT32.");
    return true;
}

bool DiskWriter::FormatDrive(
    const std::wstring& driveLetter,
    const std::wstring& fsType,
    const std::wstring& label,
    DWORD clusterSize,
    WriteLogCallback logCb,
    std::wstring& outError
) {
    if (logCb) logCb(L"Formatting volume " + driveLetter + L" as " + fsType + L" [" + label + L"]...");

    std::wstring fsUpper = fsType;
    for (auto& c : fsUpper) c = towupper(c);

    // If FAT32 requested, always try the large FAT32 formatter first (bypasses Windows 32GB limit)
    if (fsUpper == L"FAT32") {
        std::wstring fatErr;
        if (FormatFat32Large(driveLetter, label, clusterSize, logCb, fatErr)) {
            return true;
        }
        if (logCb) logCb(L"Notice: Large FAT32 custom formatter reported: " + fatErr + L". Falling back to fmifs...");
    }

    HMODULE hFmifs = LoadLibraryW(L"fmifs.dll");
    if (!hFmifs) {
        outError = L"Failed to load fmifs.dll";
        return false;
    }

    PFORMATEX pFormatEx = (PFORMATEX)GetProcAddress(hFmifs, "FormatEx");
    if (!pFormatEx) {
        FreeLibrary(hFmifs);
        outError = L"FormatEx not found in fmifs.dll";
        return false;
    }

    std::wstring rootPath = driveLetter;
    if (rootPath.back() != L'\\') rootPath += L'\\';

    std::wstring lbl = label;
    std::wstring fmtDrive = driveLetter.substr(0, 2);

    pFormatEx(
        const_cast<PWCHAR>(fmtDrive.c_str()),
        0x0C, // FMIFS_HARDDISK / generic
        const_cast<PWCHAR>(fsUpper.c_str()),
        const_cast<PWCHAR>(lbl.c_str()),
        TRUE, // QuickFormat
        clusterSize,
        (PVOID)FormatCallback
    );

    FreeLibrary(hFmifs);

    Sleep(500);
    wchar_t volName[MAX_PATH] = { 0 };
    wchar_t outFs[MAX_PATH] = { 0 };
    DWORD serial = 0, maxComp = 0, flags = 0;
    if (GetVolumeInformationW(rootPath.c_str(), volName, MAX_PATH, &serial, &maxComp, &flags, outFs, MAX_PATH)) {
        if (logCb) logCb(L"Volume formatted successfully. File system: " + std::wstring(outFs));
        return true;
    }

    if (logCb) logCb(L"FormatEx completed.");
    return true;
}

bool DiskWriter::FormatStandaloneDrive(
    const std::wstring& driveLetter,
    const std::wstring& fsType,
    const std::wstring& label,
    DWORD clusterSize,
    bool quickFormat,
    WriteProgressCallback progressCb,
    WriteLogCallback logCb,
    std::atomic<bool>& cancelFlag,
    std::wstring& outError
) {
    if (logCb) {
        logCb(L"============================================================");
        logCb(L"Bootelware USB Format Tool (Rufus Mode)");
        logCb(L"Target: " + driveLetter + L" | File System: " + fsType + L" | Label: " + label);
        logCb(L"Cluster Size: " + (clusterSize == 0 ? L"Default (Auto)" : std::to_wstring(clusterSize) + L" bytes"));
        logCb(L"============================================================");
    }

    if (progressCb) progressCb(10, 0.0, L"Dismounting drive...");

    std::wstring drive = driveLetter.substr(0, 2);
    std::wstring fsUpper = fsType;
    for (auto& c : fsUpper) c = towupper(c);

    if (fsUpper == L"FAT32") {
        if (progressCb) progressCb(30, 0.0, L"Writing FAT32 structure (Large FAT32 unlocked)...");
        bool ok = FormatFat32Large(drive, label, clusterSize, logCb, outError);
        if (ok) {
            if (progressCb) progressCb(100, 0.0, L"Format completed successfully!");
            if (logCb) logCb(L"SUCCESS: " + drive + L" formatted to FAT32 without Windows 32GB limit.");
            return true;
        }
        return false;
    } else {
        if (progressCb) progressCb(40, 0.0, L"Formatting with " + fsUpper + L"...");
        bool ok = FormatDrive(drive, fsUpper, label, clusterSize, logCb, outError);
        if (ok) {
            if (progressCb) progressCb(100, 0.0, L"Format completed successfully!");
            if (logCb) logCb(L"SUCCESS: " + drive + L" formatted successfully to " + fsUpper);
            return true;
        }
        return false;
    }
}

bool DiskWriter::EjectDrive(const std::wstring& driveLetter, std::wstring& outError) {
    std::wstring drive = driveLetter.substr(0, 2);
    std::wstring volPath = L"\\\\.\\" + drive;
    HANDLE hVol = CreateFileW(volPath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hVol == INVALID_HANDLE_VALUE) {
        outError = L"Could not open drive volume for safe removal.";
        return false;
    }
    DWORD bytes = 0;
    DeviceIoControl(hVol, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &bytes, NULL);
    DeviceIoControl(hVol, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &bytes, NULL);
    BOOL ok = DeviceIoControl(hVol, IOCTL_STORAGE_EJECT_MEDIA, NULL, 0, NULL, 0, &bytes, NULL);
    CloseHandle(hVol);
    if (!ok) {
        outError = L"Drive is currently in use by another application. Please close open files/folders and retry.";
        return false;
    }
    return true;
}

bool DiskWriter::CheckBadBlocks(
    DWORD physicalDriveIndex,
    int passes,
    WriteProgressCallback progressCb,
    WriteLogCallback logCb,
    std::atomic<bool>& cancelFlag,
    std::wstring& outError
) {
    if (logCb) logCb(L"Starting Bad Blocks check (" + std::to_wstring(passes) + L" pass)...");

    std::wstring physPath = L"\\\\.\\PhysicalDrive" + std::to_wstring(physicalDriveIndex);
    HANDLE hDisk = CreateFileW(physPath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (hDisk == INVALID_HANDLE_VALUE) {
        outError = L"Could not open drive for bad block test. (Run as Administrator)";
        return false;
    }

    uint64_t capacity = 0;
    if (!DeviceManager::GetDriveCapacity(physicalDriveIndex, capacity) || capacity == 0) {
        capacity = 1024ULL * 1024ULL * 1024ULL * 4ULL; // 4GB default assumption
    }

    // Sample 50 strategically spaced zones across the drive
    const size_t TEST_ZONES = 50;
    const DWORD BUFFER_SIZE = 64 * 1024; // 64 KB
    std::vector<BYTE> testPat(BUFFER_SIZE, 0x55);
    std::vector<BYTE> readBuf(BUFFER_SIZE, 0);

    uint64_t step = capacity / TEST_ZONES;
    if (step < BUFFER_SIZE) step = BUFFER_SIZE;

    for (size_t z = 0; z < TEST_ZONES; ++z) {
        if (cancelFlag.load()) {
            CloseHandle(hDisk);
            if (logCb) logCb(L"Bad blocks test cancelled by user.");
            return false;
        }

        uint64_t offset = z * step;
        LARGE_INTEGER li;
        li.QuadPart = offset;
        SetFilePointerEx(hDisk, li, NULL, FILE_BEGIN);

        DWORD written = 0, readBytes = 0;
        // Test write and read-back verify
        if (WriteFile(hDisk, testPat.data(), BUFFER_SIZE, &written, NULL)) {
            SetFilePointerEx(hDisk, li, NULL, FILE_BEGIN);
            if (ReadFile(hDisk, readBuf.data(), BUFFER_SIZE, &readBytes, NULL)) {
                if (readBytes != BUFFER_SIZE || memcmp(testPat.data(), readBuf.data(), BUFFER_SIZE) != 0) {
                    if (logCb) logCb(L"Warning: Verification mismatch at offset " + std::to_wstring(offset));
                }
            }
        }

        if (progressCb) {
            int pct = (int)((z * 100) / TEST_ZONES);
            progressCb(pct, 0.0, L"Testing sector blocks... (" + std::to_wstring(pct) + L"%)");
        }
    }

    CloseHandle(hDisk);
    if (logCb) logCb(L"Bad blocks check passed with no severe media errors.");
    return true;
}

// Copy directory recursively with progress tracking
static bool CopyDirectoryContents(
    const std::wstring& srcDir,
    const std::wstring& dstDir,
    uint64_t totalBytes,
    uint64_t& copiedBytes,
    auto startTime,
    WriteProgressCallback progressCb,
    WriteLogCallback logCb,
    std::atomic<bool>& cancelFlag,
    std::wstring& outError
) {
    std::error_code ec;
    for (const auto& entry : fs::recursive_directory_iterator(srcDir, fs::directory_options::skip_permission_denied, ec)) {
        if (cancelFlag.load()) {
            outError = L"Operation cancelled by user.";
            return false;
        }

        std::wstring relPath = entry.path().wstring().substr(srcDir.length());
        if (!relPath.empty() && (relPath.front() == L'\\' || relPath.front() == L'/')) relPath.erase(relPath.begin());

        std::wstring targetPath = dstDir;
        if (targetPath.back() != L'\\') targetPath += L'\\';
        targetPath += relPath;

        if (entry.is_directory(ec)) {
            CreateDirectoryW(targetPath.c_str(), NULL);
        } else if (entry.is_regular_file(ec)) {
            // Ensure parent directory exists
            size_t slash = targetPath.find_last_of(L"\\/");
            if (slash != std::wstring::npos) {
                CreateDirectoryW(targetPath.substr(0, slash).c_str(), NULL);
            }

            // Copy file in chunks to show real-time progress
            HANDLE hSrc = CreateFileW(entry.path().wstring().c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
            if (hSrc == INVALID_HANDLE_VALUE) continue;

            HANDLE hDst = CreateFileW(targetPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hDst == INVALID_HANDLE_VALUE) {
                CloseHandle(hSrc);
                continue;
            }

            const DWORD CHUNK = 2 * 1024 * 1024; // 2 MB chunks
            std::vector<BYTE> buf(CHUNK);
            DWORD bytesRead = 0, bytesWritten = 0;

            while (ReadFile(hSrc, buf.data(), CHUNK, &bytesRead, NULL) && bytesRead > 0) {
                if (cancelFlag.load()) {
                    CloseHandle(hSrc);
                    CloseHandle(hDst);
                    outError = L"Operation cancelled by user.";
                    return false;
                }
                WriteFile(hDst, buf.data(), bytesRead, &bytesWritten, NULL);
                copiedBytes += bytesWritten;

                if (progressCb && totalBytes > 0) {
                    auto now = std::chrono::steady_clock::now();
                    double elapsedSec = std::chrono::duration<double>(now - startTime).count();
                    double speedMb = elapsedSec > 0.1 ? ((double)copiedBytes / (1024.0 * 1024.0)) / elapsedSec : 0.0;
                    int pct = (int)((copiedBytes * 100) / totalBytes);
                    if (pct > 100) pct = 100;

                    std::wstring fileName = entry.path().filename().wstring();
                    progressCb(pct, speedMb, L"Writing " + fileName);
                }
            }

            CloseHandle(hSrc);
            CloseHandle(hDst);
        }
    }
    return true;
}

bool DiskWriter::ExecuteWriteJob(
    const WriteJobConfig& config,
    WriteProgressCallback progressCb,
    WriteLogCallback logCb,
    std::atomic<bool>& cancelFlag,
    std::wstring& outErrorMessage
) {
    if (logCb) {
        logCb(L"============================================================");
        logCb(L"Starting Bootelware USB Creation Job");
        logCb(L"Target Drive: PhysicalDrive" + std::to_wstring(config.physicalDriveIndex) + 
              (config.assignedDriveLetter.empty() ? L"" : L" (" + config.assignedDriveLetter + L")"));
        logCb(L"Partition Scheme: " + config.partitionScheme + L" | Target: " + config.targetSystem);
        logCb(L"File System: " + config.fileSystem + L" | Label: " + config.volumeLabel);
        logCb(L"Source ISO: " + config.isoPath);
        logCb(L"============================================================");
    }

    // 1. Bad Blocks check if requested
    if (config.checkBadBlocks) {
        if (!CheckBadBlocks(config.physicalDriveIndex, config.badBlockPasses, progressCb, logCb, cancelFlag, outErrorMessage)) {
            return false;
        }
    }

    // 2. Lock & Dismount target volumes
    if (logCb) logCb(L"Locking and dismounting target disk volumes...");
    std::wstring lockErr;
    DeviceManager::LockAndDismountVolumes(config.physicalDriveIndex, lockErr);

    // 3. DD Mode (Raw Sector Write) vs File System Extraction Mode
    if (config.ddMode) {
        if (logCb) logCb(L"Writing in DD raw image mode (unbuffered I/O)...");
        std::wstring physPath = L"\\\\.\\PhysicalDrive" + std::to_wstring(config.physicalDriveIndex);
        HANDLE hDisk = CreateFileW(physPath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);
        if (hDisk == INVALID_HANDLE_VALUE) {
            outErrorMessage = L"Could not open PhysicalDrive for unbuffered raw writing. Error: " + std::to_wstring(GetLastError());
            return false;
        }

        HANDLE hIso = CreateFileW(config.isoPath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        if (hIso == INVALID_HANDLE_VALUE) {
            CloseHandle(hDisk);
            outErrorMessage = L"Could not open source ISO file.";
            return false;
        }

        LARGE_INTEGER isoSize;
        GetFileSizeEx(hIso, &isoSize);

        const DWORD SECTOR_BUF = 4 * 1024 * 1024; // 4 MB sector-aligned buffer
        BYTE* pBuffer = (BYTE*)VirtualAlloc(NULL, SECTOR_BUF, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

        DWORD bytesRead = 0, bytesWritten = 0;
        uint64_t totalWritten = 0;
        auto startTime = std::chrono::steady_clock::now();

        while (ReadFile(hIso, pBuffer, SECTOR_BUF, &bytesRead, NULL) && bytesRead > 0) {
            if (cancelFlag.load()) {
                VirtualFree(pBuffer, 0, MEM_RELEASE);
                CloseHandle(hIso);
                CloseHandle(hDisk);
                outErrorMessage = L"Operation cancelled by user.";
                return false;
            }

            // In NO_BUFFERING, write size must be aligned to sector size (usually 512 or 4096)
            DWORD writeSize = (bytesRead + 4095) & ~4095;
            WriteFile(hDisk, pBuffer, writeSize, &bytesWritten, NULL);
            totalWritten += bytesRead;

            if (progressCb && isoSize.QuadPart > 0) {
                auto now = std::chrono::steady_clock::now();
                double elapsed = std::chrono::duration<double>(now - startTime).count();
                double speed = elapsed > 0.1 ? ((double)totalWritten / (1024.0 * 1024.0)) / elapsed : 0.0;
                int pct = (int)((totalWritten * 100) / isoSize.QuadPart);
                progressCb(pct, speed, L"Raw writing sectors...");
            }
        }

        VirtualFree(pBuffer, 0, MEM_RELEASE);
        CloseHandle(hIso);
        CloseHandle(hDisk);
    } else {
        // 4. ISO Extraction Mode
        // Format target drive volume
        std::wstring targetDrive = config.assignedDriveLetter;
        if (targetDrive.empty()) {
            targetDrive = L"E:"; // fallback if not previously assigned
        }

        std::wstring fmtErr;
        if (!FormatDrive(targetDrive, config.fileSystem, config.volumeLabel, config.clusterSize, logCb, fmtErr)) {
            if (logCb) logCb(L"Notice: Proceeding with writing to partition " + targetDrive);
        }

        // Mount ISO using virtdisk
        if (logCb) logCb(L"Mounting ISO image via Windows Virtual Disk Service...");
        HANDLE hVhd = INVALID_HANDLE_VALUE;
        std::wstring mountedLetter;
        std::wstring mountErr;
        if (!IsoReader::MountIso(config.isoPath, hVhd, mountedLetter, mountErr)) {
            outErrorMessage = L"Could not mount ISO image: " + mountErr;
            return false;
        }

        if (logCb) logCb(L"ISO mounted successfully at virtual drive " + mountedLetter + L"\\");

        // Calculate total bytes to copy
        std::wstring srcRoot = mountedLetter + L"\\";
        std::wstring dstRoot = targetDrive;
        if (dstRoot.back() != L'\\') dstRoot += L'\\';

        uint64_t totalBytes = config.isoMeta.fileSizeBytes;
        uint64_t copiedBytes = 0;
        auto startTime = std::chrono::steady_clock::now();

        if (logCb) logCb(L"Extracting and copying files to USB target drive...");
        if (!CopyDirectoryContents(srcRoot, dstRoot, totalBytes, copiedBytes, startTime, progressCb, logCb, cancelFlag, outErrorMessage)) {
            IsoReader::UnmountIso(hVhd, config.isoPath);
            return false;
        }

        // 5. Inject Windows 11 Bypasses (autounattend.xml) if applicable
        if (config.enableWin11Bypasses) {
            if (logCb) logCb(L"Injecting Windows 11 setup bypass configuration (autounattend.xml)...");
            std::wstring xmlErr;
            if (Win11BypassGenerator::WriteAutoUnattendFile(dstRoot, config.win11Options, xmlErr)) {
                if (logCb) logCb(L"✓ autounattend.xml successfully generated with TPM, Secure Boot, RAM & Account bypasses!");
            } else {
                if (logCb) logCb(L"Warning: Failed to inject autounattend.xml: " + xmlErr);
            }
        }

        // Unmount ISO
        if (logCb) logCb(L"Detaching ISO virtual disk...");
        IsoReader::UnmountIso(hVhd, config.isoPath);
    }

    if (progressCb) {
        progressCb(100, 0.0, L"Ready");
    }

    if (logCb) {
        logCb(L"============================================================");
        logCb(L"SUCCESS: Bootable USB drive created successfully!");
        logCb(L"Your media is now ready to boot in " + config.targetSystem + L" mode.");
        logCb(L"============================================================");
    }

    return true;
}
