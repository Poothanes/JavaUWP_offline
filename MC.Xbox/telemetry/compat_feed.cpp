#include "compat_feed.h"

#include "http_client.h"
#include "launcher_common.h"
#include "telemetry.h"

#include <roapi.h>

#include <winrt/base.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <atomic>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace compat {
namespace {

constexpr unsigned kFetchTimeoutMs = 4000;
constexpr unsigned long long kRefreshSeconds = 24ull * 60ull * 60ull;
constexpr uint32_t kMaxFlags = 1000;
constexpr uint32_t kMaxProjectIds = 32;
constexpr size_t kMaxKeyChars = 128;
constexpr size_t kMaxNoteChars = 240;
constexpr wchar_t kAny[] = L"*";

struct Entry {
    std::wstring slug;
    std::wstring modVersion;
    std::wstring mcVersion;
    std::wstring loader;
    std::wstring severity;
    std::wstring note;
    std::vector<std::wstring> projectIds;
};

std::mutex g_mutex;
std::vector<Entry> g_entries;
bool g_loaded = false;
std::atomic<bool> g_fetching{ false };

std::wstring FeedDir() {
    const std::wstring localDir = GetLocalStateDir();
    if (localDir.empty()) return std::wstring();
    return localDir + L"\\telemetry";
}

std::wstring FeedPath() {
    const std::wstring dir = FeedDir();
    return dir.empty() ? dir : dir + L"\\compat.json";
}

std::wstring MetaPath() {
    const std::wstring dir = FeedDir();
    return dir.empty() ? dir : dir + L"\\compat.meta";
}

unsigned long long UnixNow() {
    FILETIME now = {};
    GetSystemTimeAsFileTime(&now);
    ULARGE_INTEGER value;
    value.LowPart = now.dwLowDateTime;
    value.HighPart = now.dwHighDateTime;
    return value.QuadPart / 10000000ull - 11644473600ull;
}

std::wstring MetaValue(const std::wstring& body, const wchar_t* key) {
    const std::wstring needle = std::wstring(key) + L"\t";
    size_t at = body.find(needle);
    if (at == std::wstring::npos) return std::wstring();
    const size_t start = at + needle.size();
    const size_t end = body.find_first_of(L"\r\n", start);
    return TrimWhitespace(body.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start));
}

bool WriteMeta(const std::string& etag, unsigned long long fetched) {
    return false;
}

bool ReadMeta(std::string& etag, unsigned long long& fetched) {
    etag.clear();
    fetched = 0;
    return false;
}

std::wstring JsonString(
    const winrt::Windows::Data::Json::JsonObject& object,
    const wchar_t* key,
    const wchar_t* fallback) {
    return std::wstring(fallback);
}

bool Parse(const std::wstring& text, std::vector<Entry>& out) {
    out.clear();
    return false;
}

void LoadLocked() {
    g_loaded = true;
    g_entries.clear();
}

bool Matches(const std::wstring& field, const std::wstring& actual) {
    if (field == kAny) return true;
    return field == ToLowerW(actual);
}

}

void RefreshAsync() {
    // ปิดการดึงข้อมูล Compat Feed ผ่านเน็ตโดยสมบูรณ์
    return;
}

Flag Lookup(
    const std::wstring& slug,
    const std::wstring& projectId,
    const std::wstring& mcVersion,
    const std::wstring& loader) {
    Flag flag;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_loaded) LoadLocked();
    return flag; // คืนค่าว่างเปล่าเพื่อให้รันแบบออฟไลน์ได้ทันที
}

}
