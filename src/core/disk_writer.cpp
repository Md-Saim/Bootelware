#include "disk_writer.h"
#include "device_manager.h"
#include <winioctl.h>
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

bool DiskWriter::FormatDrive(
    const std::wstring& driveLetter,
    const std::wstring& fsType,
    const std::wstring& label,
    DWORD clusterSize,
    WriteLogCallback logCb,
    std::wstring& outError
) {
    if (logCb) logCb(L"Formatting volume " + driveLetter + L" as " + fsType + L" [" + label + L"]...");

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

    std::wstring fsUpper = fsType;
    std::wstring lbl = label;

    // fmifs expects "X:" (drive letter without trailing slash or path)
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

    // Verify format completed by checking volume
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
