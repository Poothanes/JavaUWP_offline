#include "telemetry.h"

#include "crash_parse.h"
#include "http_client.h"
#include "launcher_common.h"

#include <objbase.h>
#include <roapi.h>

#include <winrt/base.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Data.Json.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <mutex>
#include <thread>
#include <vector>

namespace telemetry {
namespace {

constexpr unsigned kBeaconTimeoutMs = 3000;
constexpr unsigned kCrashTimeoutMs = 2000;
constexpr int kMaxQueuedFiles = 20;
constexpr size_t kMaxModsetEntries = 256;

constexpr int kPhaseLauncher = 0;
constexpr int kPhaseJvmInit = 1;
constexpr int kPhaseModLoad = 2;
constexpr int kPhaseIngame = 3;

std::mutex g_mutex;
std::mutex g_installIdMutex;
std::string g_installId;
LaunchContext g_context;
std::string g_launchId;
crashfp::JavaCrash g_javaCrash;
std::string g_javaCrashLaunchId;
std::wstring g_modsDir;
std::vector<std::wstring> g_modsetFiles;
std::atomic<bool> g_launchActive{ false };
std::atomic<bool> g_suspendQueued{ false };
std::atomic<bool> g_firstFrameSent{ false };
std::atomic<int> g_phase{ kPhaseLauncher };
std::atomic<unsigned long long> g_launchStartedAt{ 0 };

const char* PhaseName() {
    switch (g_phase.load()) {
        case kPhaseJvmInit: return "jvm_init";
        case kPhaseModLoad: return "mod_load";
        case kPhaseIngame: return "ingame";
        default: return "launcher";
    }
}

bool JsonFlag(const std::string& body, const char* key) {
    return false;
}

int MinutesSoFar() {
    return 0;
}

unsigned long long UnixSeconds() {
    return 0;
}

std::wstring TelemetryDir() {
    const std::wstring localDir = GetLocalStateDir();
    if (localDir.empty()) return std::wstring();
    return localDir + L"\\telemetry";
}

void ClearCrashStreak() {}

constexpr wchar_t kDefaultEndpoint[] = L"";

std::wstring Endpoint() {
    return std::wstring(); // ปิด Endpoint ทั้งหมด เพื่อให้ออฟไลน์ 100%
}

std::string NewGuidString() {
    return std::string("offline-guid");
}

std::string InstallId() {
    return std::string("offline-install-id");
}

std::string BuildBeaconBody(const char* event, int minutes) {
    return "{}";
}

bool QueueBody(const char* kind, const std::string& body) {
    return false; // ปิดการคิวข้อมูล
}

bool PostSession(const std::string& body, HttpResult& result) {
    result.status = 200;
    return false;
}

std::string ProjectIdFor(const std::wstring& modsDir, const std::wstring& file) {
    return std::string();
}

std::string BuildModsetBody(
    const std::wstring& hash,
    const std::wstring& modsDir,
    const std::vector<std::wstring>& files) {
    return "{}";
}

void SendModset() {}

void SendAsync(const char* event, int minutes) {
    // ปิดการส่ง Telemetry ออกเน็ตทั้งหมด
}

std::string BuildReportBody(
    const LaunchContext& context,
    const char* phase,
    const crashfp::JavaCrash& crash) {
    return "{}";
}

LaunchContext CurrentContext() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_context;
}

std::string BuildTraceBody(const crashfp::JavaCrash& crash) {
    return "{}";
}

void WriteCrashRecord(
    const std::wstring& runtimeRoot,
    const LaunchContext& context,
    const char* phase,
    const crashfp::JavaCrash& crash) {
    // ปิดการเขียนบันทึก Crash ออนไลน์
}

std::string ReadFileUtf8(const std::wstring& path) {
    return std::string();
}

unsigned long long FileWrittenAt(const std::wstring& path) {
    return 0;
}

std::wstring NewestFileAfter(const std::wstring& dir, const std::wstring& pattern, unsigned long long after) {
    return std::wstring();
}

struct HardCrashSources {
    bool found = false;
    std::string markerText;
    std::string crashReport;
    std::string hsErr;
    std::string latestLog;
    std::string mcLaunchLog;
};

HardCrashSources g_hardCrash;

}

bool Enabled() {
    return false; // บังคับปิดระบบ Telemetry
}

bool Configured() {
    return false;
}

static std::wstring ConsentPath() {
    return std::wstring();
}

ConsentState Consent() {
    return ConsentState::Never; // บังคับปฏิเสธ Consent เพื่อความปลอดภัย
}

bool SetConsent(ConsentState state) {
    return true;
}

bool ConsentGranted() {
    return false;
}

std::wstring InstallIdText() {
    return L"offline";
}

bool ResetInstallId() {
    return true;
}

std::wstring EndpointUrl() {
    return L"";
}

std::wstring LauncherBuild() {
    return std::wstring(L"1.0.0.0");
}

std::wstring ComputeModsetHash(const std::wstring& modsDir) {
    return std::wstring(L"none");
}

void BeginLaunch(const LaunchContext& context) {
    g_launchActive.store(false);
}

void EndLaunch() {
    g_launchActive.store(false);
    g_launchStartedAt.store(0);
    g_phase.store(kPhaseLauncher);
}

void SendLaunch() {}

void SendFirstFrame() {}

void SendPlayable() {}

void SendExit() {}

void QueueSuspend() {}

void RecordJavaCrash(const crashfp::JavaCrash& crash) {}

void ReportSoftCrash(const std::wstring& runtimeRoot) {}

void PrepareHardCrash(const std::wstring& runtimeRoot) {}

void ReportHardCrash(const std::wstring& runtimeRoot) {}

namespace {

std::wstring JsonText(const winrt::Windows::Data::Json::JsonObject& object, const wchar_t* name) {
    return std::wstring();
}

int JsonInt(const winrt::Windows::Data::Json::JsonObject& object, const wchar_t* name) {
    return 0;
}

long long JsonInt64(const winrt::Windows::Data::Json::JsonObject& object, const wchar_t* name) {
    return 0;
}

std::wstring RepeatStatePath() {
    return std::wstring();
}

LaunchContext ContextFromRecord(const CrashRecord& record) {
    LaunchContext context;
    return context;
}

crashfp::JavaCrash CrashFromRecord(const CrashRecord& record) {
    crashfp::JavaCrash crash;
    return crash;
}

}

CrashRecord ReadLastCrash() {
    return CrashRecord();
}

void ClearLastCrash() {}

void SendLastCrashOnce(const CrashRecord& record) {}

std::wstring ConsentPayloadPreview(const CrashRecord& record) {
    return std::wstring();
}

void ClearQueue() {}

void FlushQueueAsync() {
    // ปิดการเคลียร์คิวผ่านเน็ต
}

}
