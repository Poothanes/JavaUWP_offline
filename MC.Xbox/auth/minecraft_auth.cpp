#include "minecraft_auth.h"
#include "launcher_common.h"

// ค่าเริ่มต้นสำหรับโหมด Offline (เปลี่ยนชื่อผู้เล่นตรงนี้ได้เลย)
static constexpr char kOfflineUsername[] = "OfflinePlayer";
static constexpr char kOfflineUuid[] = "00000000-0000-0000-0000-000000000000";
static constexpr char kOfflineToken[] = "0";

bool SaveRefreshToken(const std::string& refreshToken) {
    WriteLog(L"[Offline Mode] SaveRefreshToken bypassed");
    return true;
}

std::string LoadRefreshToken() {
    // คืนค่าจำลองกลับไปเพื่อให้ App.cpp คิดว่าเคยล็อกอินค้างไว้แล้ว จะได้ไม่เด้งหน้าใส่รหัส Device Code
    WriteLog(L"[Offline Mode] Returning dummy refresh token");
    return "offline_refresh_token";
}

void ClearRefreshToken() {
    WriteLog(L"[Offline Mode] ClearRefreshToken bypassed");
}

bool RequestDeviceCode(DeviceCodeResponse& out, std::string& error) {
    out.userCode = "OFFLINE";
    out.deviceCode = "offline_device_code";
    out.verificationUri = "http://localhost";
    out.expiresIn = 900;
    out.interval = 1;
    WriteLog(L"[Offline Mode] RequestDeviceCode bypassed");
    return true;
}

DevicePollResult PollDeviceToken(const std::string& deviceCode) {
    DevicePollResult result;
    result.status = DevicePollStatus::Success;
    result.token.accessToken = kOfflineToken;
    result.token.refreshToken = "offline_refresh_token";
    result.token.expiresIn = 86400;
    WriteLog(L"[Offline Mode] PollDeviceToken succeeded immediately");
    return result;
}

bool RefreshMicrosoftToken(const std::string& refreshToken, MicrosoftTokenResponse& out, std::string& error) {
    out.accessToken = kOfflineToken;
    out.refreshToken = "offline_refresh_token";
    out.expiresIn = 86400;
    WriteLog(L"[Offline Mode] RefreshMicrosoftToken bypassed");
    return true;
}

bool AuthenticateWithXboxLive(const std::string& microsoftAccessToken, XboxAuthResponse& out, std::string& error) {
    out.token = "offline_xbl_token";
    out.userHash = "offline_uhs";
    return true;
}

bool AuthorizeWithXsts(const std::string& xboxToken, const char* relyingParty, XboxAuthResponse& out, std::string& error) {
    out.token = "offline_xsts_token";
    out.userHash = "offline_uhs";
    return true;
}

bool LoginToMinecraft(const std::string& userHash, const std::string& xstsToken, MicrosoftTokenResponse& out, std::string& error) {
    out.accessToken = kOfflineToken;
    out.expiresIn = 86400;
    return true;
}

bool EnsureMinecraftEntitlement(const std::string& minecraftAccessToken, std::string& error) {
    // ข้ามการเช็กสิทธิ์ออนไลน์
    return true;
}

bool FetchMinecraftProfile(const std::string& minecraftAccessToken, LaunchAuthConfig& out, std::string& error) {
    out.uuid = NormalizeMinecraftUuid(kOfflineUuid);
    out.username = kOfflineUsername;
    out.accessToken = kOfflineToken;
    return true;
}

bool BuildMinecraftAuth(const std::string& microsoftAccessToken, LaunchAuthConfig& out, std::string& error) {
    out.uuid = NormalizeMinecraftUuid(kOfflineUuid);
    out.username = kOfflineUsername;
    out.accessToken = kOfflineToken;

    WriteLogF(L"[Offline Mode] Minecraft auth resolved username=%s uuid=%s",
        a2w(out.username.c_str()).c_str(),
        a2w(out.uuid.c_str()).c_str());
    return true;
}
