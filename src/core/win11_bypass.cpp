#include "win11_bypass.h"
#include <windows.h>
#include <sstream>

std::string Win11BypassGenerator::GenerateAutoUnattendXml(const Win11BypassOptions& opts) {
    std::stringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    ss << "<unattend xmlns=\"urn:schemas-microsoft-com:unattend\">\n";

    // 1. windowsPE pass (Bypass TPM, SecureBoot, RAM, CPU, Storage)
    if (opts.bypassTpmSecureBootRam || opts.bypassCpuStorage) {
        ss << "  <settings pass=\"windowsPE\">\n";
        ss << "    <component name=\"Microsoft-Windows-Setup\" processorArchitecture=\"amd64\" publicKeyToken=\"31bf3856ad364e35\" language=\"neutral\" versionScope=\"nonSxS\">\n";
        ss << "      <RunSynchronous>\n";
        int order = 1;
        if (opts.bypassTpmSecureBootRam) {
            ss << "        <RunSynchronousCommand wcm:action=\"add\" xmlns:wcm=\"http://schemas.microsoft.com/WMIConfig/2002/State\">\n";
            ss << "          <Order>" << order++ << "</Order>\n";
            ss << "          <Path>reg add HKLM\\SYSTEM\\Setup\\LabConfig /v BypassTPMCheck /t REG_DWORD /d 1 /f</Path>\n";
            ss << "        </RunSynchronousCommand>\n";

            ss << "        <RunSynchronousCommand wcm:action=\"add\" xmlns:wcm=\"http://schemas.microsoft.com/WMIConfig/2002/State\">\n";
            ss << "          <Order>" << order++ << "</Order>\n";
            ss << "          <Path>reg add HKLM\\SYSTEM\\Setup\\LabConfig /v BypassSecureBootCheck /t REG_DWORD /d 1 /f</Path>\n";
            ss << "        </RunSynchronousCommand>\n";

            ss << "        <RunSynchronousCommand wcm:action=\"add\" xmlns:wcm=\"http://schemas.microsoft.com/WMIConfig/2002/State\">\n";
            ss << "          <Order>" << order++ << "</Order>\n";
            ss << "          <Path>reg add HKLM\\SYSTEM\\Setup\\LabConfig /v BypassRAMCheck /t REG_DWORD /d 1 /f</Path>\n";
            ss << "        </RunSynchronousCommand>\n";
        }
        if (opts.bypassCpuStorage) {
            ss << "        <RunSynchronousCommand wcm:action=\"add\" xmlns:wcm=\"http://schemas.microsoft.com/WMIConfig/2002/State\">\n";
            ss << "          <Order>" << order++ << "</Order>\n";
            ss << "          <Path>reg add HKLM\\SYSTEM\\Setup\\LabConfig /v BypassCPUCheck /t REG_DWORD /d 1 /f</Path>\n";
            ss << "        </RunSynchronousCommand>\n";

            ss << "        <RunSynchronousCommand wcm:action=\"add\" xmlns:wcm=\"http://schemas.microsoft.com/WMIConfig/2002/State\">\n";
            ss << "          <Order>" << order++ << "</Order>\n";
            ss << "          <Path>reg add HKLM\\SYSTEM\\Setup\\LabConfig /v BypassStorageCheck /t REG_DWORD /d 1 /f</Path>\n";
            ss << "        </RunSynchronousCommand>\n";
        }
        ss << "      </RunSynchronous>\n";
        ss << "    </component>\n";
        ss << "  </settings>\n";
    }

    // 2. specialize pass (Disable BitLocker Auto-Encryption)
    if (opts.disableBitLocker || opts.disableTelemetry) {
        ss << "  <settings pass=\"specialize\">\n";
        ss << "    <component name=\"Microsoft-Windows-Deployment\" processorArchitecture=\"amd64\" publicKeyToken=\"31bf3856ad364e35\" language=\"neutral\" versionScope=\"nonSxS\">\n";
        ss << "      <RunSynchronous>\n";
        int order = 1;
        if (opts.disableBitLocker) {
            ss << "        <RunSynchronousCommand wcm:action=\"add\" xmlns:wcm=\"http://schemas.microsoft.com/WMIConfig/2002/State\">\n";
            ss << "          <Order>" << order++ << "</Order>\n";
            ss << "          <Path>reg add HKLM\\SYSTEM\\CurrentControlSet\\Control\\BitLocker /v PreventDeviceEncryption /t REG_DWORD /d 1 /f</Path>\n";
            ss << "        </RunSynchronousCommand>\n";
        }
        if (opts.disableTelemetry) {
            ss << "        <RunSynchronousCommand wcm:action=\"add\" xmlns:wcm=\"http://schemas.microsoft.com/WMIConfig/2002/State\">\n";
            ss << "          <Order>" << order++ << "</Order>\n";
            ss << "          <Path>reg add HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection /v AllowTelemetry /t REG_DWORD /d 0 /f</Path>\n";
            ss << "        </RunSynchronousCommand>\n";
        }
        ss << "      </RunSynchronous>\n";
        ss << "    </component>\n";
        ss << "  </settings>\n";
    }

    // 3. oobeSystem pass (Local Account & Bypass Online Account)
    ss << "  <settings pass=\"oobeSystem\">\n";
    ss << "    <component name=\"Microsoft-Windows-Shell-Setup\" processorArchitecture=\"amd64\" publicKeyToken=\"31bf3856ad364e35\" language=\"neutral\" versionScope=\"nonSxS\">\n";
    ss << "      <OOBE>\n";
    if (opts.bypassOnlineAccount) {
        ss << "        <HideOnlineAccountScreens>true</HideOnlineAccountScreens>\n";
    }
    if (opts.skipPrivacyQuestions) {
        ss << "        <ProtectYourPC>3</ProtectYourPC>\n";
    }
    ss << "      </OOBE>\n";

    if (opts.createLocalAccount && !opts.localAccountUsername.empty()) {
        char user[128] = { 0 };
        WideCharToMultiByte(CP_UTF8, 0, opts.localAccountUsername.c_str(), -1, user, sizeof(user), NULL, NULL);
        ss << "      <UserAccounts>\n";
        ss << "        <LocalAccounts>\n";
        ss << "          <LocalAccount wcm:action=\"add\" xmlns:wcm=\"http://schemas.microsoft.com/WMIConfig/2002/State\">\n";
        ss << "            <Name>" << user << "</Name>\n";
        ss << "            <Group>Administrators</Group>\n";
        ss << "            <DisplayName>" << user << "</DisplayName>\n";
        ss << "            <Description>Bootelware Local Admin User</Description>\n";
        ss << "            <Password>\n";
        ss << "              <Value></Value>\n";
        ss << "              <PlainText>true</PlainText>\n";
        ss << "            </Password>\n";
        ss << "          </LocalAccount>\n";
        ss << "        </LocalAccounts>\n";
        ss << "      </UserAccounts>\n";
    }

    ss << "    </component>\n";
    ss << "  </settings>\n";
    ss << "</unattend>\n";

    return ss.str();
}

bool Win11BypassGenerator::WriteAutoUnattendFile(const std::wstring& targetDriveRoot, const Win11BypassOptions& opts, std::wstring& outError) {
    std::wstring outPath = targetDriveRoot;
    if (outPath.back() != L'\\') outPath += L'\\';
    outPath += L"autounattend.xml";

    std::string xmlContent = GenerateAutoUnattendXml(opts);

    HANDLE hFile = CreateFileW(outPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        outError = L"Could not create autounattend.xml on target drive. Error: " + std::to_wstring(GetLastError());
        return false;
    }

    DWORD written = 0;
    if (!WriteFile(hFile, xmlContent.data(), (DWORD)xmlContent.size(), &written, NULL) || written != xmlContent.size()) {
        CloseHandle(hFile);
        outError = L"Failed writing autounattend.xml content.";
        return false;
    }

    CloseHandle(hFile);
    return true;
}
