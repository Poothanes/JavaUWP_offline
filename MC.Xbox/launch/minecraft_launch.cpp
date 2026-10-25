#include "minecraft_launch.h"

#include "app_globals.h"
#include "crash_report.h"
#include "launch_internal.h"
#include "loader.h"
#include "loader_common.h"
#include "launch_diagnostics.h"
#include "neoforge.h"
#include "launcher_common.h"
#include "mod_defaults.h"
#include "mods_browser.h"
#include "profiles.h"
#include "runtime_manager.h"
#include "telemetry.h"

#include <jni.h>
#include <roapi.h>
#include <io.h>
#include <fcntl.h>
#include <share.h>
#include <errno.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <wrl.h>
#include <wrl/wrappers/corewrappers.h>
#include <windows.applicationmodel.core.h>
#include <windows.foundation.h>
#include <windows.foundation.collections.h>
#include <windows.ui.core.h>

#include "third_party/miniz/miniz.h"

using namespace Microsoft::WRL;
using Microsoft::WRL::Wrappers::HStringReference;
using namespace ABI::Windows::ApplicationModel::Core;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
using namespace ABI::Windows::UI::Core;

typedef jint(JNICALL* JNI_CreateJavaVM_t)(JavaVM**, void**, void*);

static bool g_embeddedJvmUsed = false;

bool EmbeddedJvmAlreadyUsed() {
    return g_embeddedJvmUsed;
}

static void DestroyEmbeddedJvm(JavaVM*& vm, JNIEnv*& env) {
    if (!vm) return;
    vm->DestroyJavaVM();
    vm = nullptr;
    env = nullptr;
    WriteLog(L"Embedded JVM destroyed after launch preparation failure");
}

static constexpr wchar_t kEGLNativeWindowTypeProperty[] = L"EGLNativeWindowTypeProperty";

static unsigned long long ShimPresentedFrames() {
    using ReaderFn = unsigned long long (*)();
    static ReaderFn reader = nullptr;
    if (!reader) {
        HMODULE shim = GetModuleHandleW(L"glfw.dll");
        if (shim) {
            reader = reinterpret_cast<ReaderFn>(GetProcAddress(shim, "BanditShimPresentedFrames"));
        }
    }
    return reader ? reader() : 0ull;
}

static volatile LONG g_logTailerRunning = 0;
static HANDLE g_logTailerThreads[8] = {};
static int g_logTailerThreadCount = 0;
static HANDLE g_redirectedStdoutHandle = INVALID_HANDLE_VALUE;
static HANDLE g_redirectedStderrHandle = INVALID_HANDLE_VALUE;

static const ULONGLONG kLogTailerFastWindowMs = 120000;
static const DWORD kLogTailerFastPollMs = 250;
static const DWORD kLogTailerIdlePollMs = 2000;

static constexpr char kPlayableMarker[] = "banditvault:playable";

struct LogTailerConfig {
    std::wstring path;
    std::wstring label;
};

static std::wstring TailTextForUi(const std::wstring& text, size_t maxLines = 14, size_t maxChars = 4200) {
    if (text.empty()) return L"Waiting for pre-launch log output...";

    size_t end = text.size();
    while (end > 0 && (text[end - 1] == L'\r' || text[end - 1] == L'\n')) {
        --end;
    }
    size_t start = end;
    size_t lines = 0;
    while (start > 0 && lines < maxLines) {
        --start;
        if (text[start] == L'\n') {
            ++lines;
        }
    }
    if (lines >= maxLines && start < end) {
        ++start;
    }
    if (end > start && end - start > maxChars) {
        start = end - maxChars;
        while (start < end && text[start] != L'\n') {
            ++start;
        }
        if (start < end) {
            ++start;
        }
    }

    std::wstring tail = text.substr(start, end - start);
    tail.erase(std::remove(tail.begin(), tail.end(), L'\r'), tail.end());
    for (wchar_t& ch : tail) {
        if (ch == L'\t') ch = L' ';
    }
    if (tail.empty()) return L"Waiting for pre-launch log output...";

    constexpr size_t maxLineChars = 150;
    std::wstring compact;
    size_t lineStart = 0;
    while (lineStart <= tail.size()) {
        size_t lineEnd = tail.find(L'\n', lineStart);
        if (lineEnd == std::wstring::npos) lineEnd = tail.size();
        std::wstring line = tail.substr(lineStart, lineEnd - lineStart);
        if (line.size() > maxLineChars) {
            constexpr size_t headChars = 96;
            constexpr size_t tailChars = maxLineChars - headChars - 5;
            line = line.substr(0, headChars) + L" ... " + line.substr(line.size() - tailChars);
        }
        if (!compact.empty()) compact += L'\n';
        compact += line;
        if (lineEnd >= tail.size()) break;
        lineStart = lineEnd + 1;
    }
    return compact.empty() ? L"Waiting for pre-launch log output..." : compact;
}

std::wstring ReadLaunchLogTailForUi(const std::vector<std::wstring>& paths, size_t maxLines) {
    for (const std::wstring& path : paths) {
        std::wstring text;
        if (ReadTextFile(path, text) && !text.empty()) {
            return TailTextForUi
