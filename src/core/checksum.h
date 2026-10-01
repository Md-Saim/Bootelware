#pragma once
#include <windows.h>
#include <string>
#include <functional>

struct ChecksumResult {
    std::wstring md5;
    std::wstring sha1;
    std::wstring sha256;
    std::wstring sha512;
    bool success = false;
    std::wstring errorMessage;
};

typedef std::function<void(uint64_t bytesProcessed, uint64_t totalBytes, int percent)> ChecksumProgressCallback;

class ChecksumEngine {
public:
    static ChecksumResult ComputeFileHashes(const std::wstring& filePath, ChecksumProgressCallback callback = nullptr);
    static bool VerifyHash(const std::wstring& calculatedHash, const std::wstring& userProvidedHash);
};
