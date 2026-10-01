#pragma once
#include <string>

struct Win11BypassOptions {
    bool bypassTpmSecureBootRam = true;
    bool bypassCpuStorage = true;
    bool bypassOnlineAccount = true;
    bool createLocalAccount = true;
    std::wstring localAccountUsername = L"User";
    bool disableBitLocker = true;
    bool disableTelemetry = true;
    bool skipPrivacyQuestions = true;
};

class Win11BypassGenerator {
public:
    static std::string GenerateAutoUnattendXml(const Win11BypassOptions& opts);
    static bool WriteAutoUnattendFile(const std::wstring& targetDriveRoot, const Win11BypassOptions& opts, std::wstring& outError);
};
