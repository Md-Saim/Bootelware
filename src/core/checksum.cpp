#include "checksum.h"
#include <bcrypt.h>
#include <vector>
#include <algorithm>

#pragma comment(lib, "bcrypt.lib")

struct HashContext {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    DWORD hashLength = 0;
    std::wstring algName;

    bool Init(const wchar_t* algId) {
        algName = algId;
        if (BCryptOpenAlgorithmProvider(&hAlg, algId, NULL, 0) != 0) return false;
        DWORD cbData = 0;
        if (BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&hashLength, sizeof(DWORD), &cbData, 0) != 0) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            hAlg = NULL;
            return false;
        }
        if (BCryptCreateHash(hAlg, &hHash, NULL, 0, NULL, 0, 0) != 0) {
            BCryptCloseAlgorithmProvider(hAlg, 0);
            hAlg = NULL;
            return false;
        }
        return true;
    }

    void Update(const BYTE* data, DWORD size) {
        if (hHash) {
            BCryptHashData(hHash, (PBYTE)data, size, 0);
        }
    }

    std::wstring Finalize() {
        if (!hHash) return L"";
        std::vector<BYTE> hash(hashLength);
        BCryptFinishHash(hHash, hash.data(), hashLength, 0);
        BCryptDestroyHash(hHash);
        hHash = NULL;
        BCryptCloseAlgorithmProvider(hAlg, 0);
        hAlg = NULL;

        wchar_t hex[3];
        std::wstring result;
        for (BYTE b : hash) {
            swprintf_s(hex, L"%02x", b);
            result += hex;
        }
        return result;
    }

    void Cleanup() {
        if (hHash) { BCryptDestroyHash(hHash); hHash = NULL; }
        if (hAlg) { BCryptCloseAlgorithmProvider(hAlg, 0); hAlg = NULL; }
    }
};

ChecksumResult ChecksumEngine::ComputeFileHashes(const std::wstring& filePath, ChecksumProgressCallback callback) {
    ChecksumResult res;
    res.success = false;

    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        res.errorMessage = L"Failed to open file for checksum computation.";
        return res;
    }

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        res.errorMessage = L"Failed to query file size.";
        return res;
    }

    HashContext ctxMd5, ctxSha1, ctxSha256, ctxSha512;
    if (!ctxMd5.Init(BCRYPT_MD5_ALGORITHM) ||
        !ctxSha1.Init(BCRYPT_SHA1_ALGORITHM) ||
        !ctxSha256.Init(BCRYPT_SHA256_ALGORITHM) ||
        !ctxSha512.Init(BCRYPT_SHA512_ALGORITHM)) {
        ctxMd5.Cleanup();
        ctxSha1.Cleanup();
        ctxSha256.Cleanup();
        ctxSha512.Cleanup();
        CloseHandle(hFile);
        res.errorMessage = L"Failed to initialize Windows Cryptographic Providers.";
        return res;
    }

    const DWORD CHUNK_SIZE = 1024 * 1024; // 1 MB buffer
    std::vector<BYTE> buffer(CHUNK_SIZE);
    DWORD bytesRead = 0;
    uint64_t totalRead = 0;

    while (ReadFile(hFile, buffer.data(), CHUNK_SIZE, &bytesRead, NULL) && bytesRead > 0) {
        ctxMd5.Update(buffer.data(), bytesRead);
        ctxSha1.Update(buffer.data(), bytesRead);
        ctxSha256.Update(buffer.data(), bytesRead);
        ctxSha512.Update(buffer.data(), bytesRead);

        totalRead += bytesRead;
        if (callback && fileSize.QuadPart > 0) {
            int pct = (int)((totalRead * 100) / fileSize.QuadPart);
            callback(totalRead, fileSize.QuadPart, pct);
        }
    }

    CloseHandle(hFile);

    res.md5 = ctxMd5.Finalize();
    res.sha1 = ctxSha1.Finalize();
    res.sha256 = ctxSha256.Finalize();
    res.sha512 = ctxSha512.Finalize();
    res.success = true;

    if (callback) {
        callback(fileSize.QuadPart, fileSize.QuadPart, 100);
    }

    return res;
}

bool ChecksumEngine::VerifyHash(const std::wstring& calculatedHash, const std::wstring& userProvidedHash) {
    if (userProvidedHash.empty()) return false;
    std::wstring h1 = calculatedHash;
    std::wstring h2 = userProvidedHash;
    // Lowercase and trim
    std::transform(h1.begin(), h1.end(), h1.begin(), ::towlower);
    std::transform(h2.begin(), h2.end(), h2.begin(), ::towlower);
    while (!h2.empty() && (h2.back() == L' ' || h2.back() == L'\t' || h2.back() == L'\r' || h2.back() == L'\n')) h2.pop_back();
    while (!h2.empty() && (h2.front() == L' ' || h2.front() == L'\t')) h2.erase(h2.begin());
    return h1 == h2;
}
