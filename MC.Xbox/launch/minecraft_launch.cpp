bool RunEmbeddedMinecraft(const std::wstring& exeDir,
    const std::wstring& packageDir,
    const std::wstring& jreDir,
    const std::wstring& packagedJreRelativeDir,
    const std::wstring& javaBasePatchName,
    const std::wstring& javaZipfsPatchName,
    const std::wstring& gameDir,
    const std::wstring& assetsDir,
    const std::wstring& nativesDir,
    const std::wstring& bundledModsDir,
    const std::wstring& userModsDir,
    const std::wstring& clientJar,
    const std::wstring& javaLog,
    const std::wstring& argsPath,
    const std::wstring& classPath,
    const std::wstring& launchVersion,
    const std::wstring& assetIndex,
    const std::wstring& minecraftVersion,
    const std::wstring& loader,
    const std::wstring& loaderVersion,
    const std::wstring& mainClassName,
    const std::vector<std::wstring>& extraJvmArgs,
    const std::vector<std::wstring>& extraGameArgs,
    const std::wstring& neoFormVersion,
    const std::wstring& neoForgeInstallToolsVersion,
    const std::wstring& neoForgeJarSplitterVersion,
    const std::wstring& neoForgeBinaryPatcherVersion,
    const std::wstring& neoForgeAutoRenamingToolVersion,
    const LaunchAuthConfig& authConfig,
    LaunchProgressCallback progress)
{
    const auto reportProgress = [&](const wchar_t* status, const wchar_t* detail, float value) {
        if (progress) {
            progress(status, detail, value);
        }
    };

    if (g_embeddedJvmUsed) {
        WriteLog(L"Refusing launch: this process has already run a JVM, it cannot start another");
        reportProgress(
            L"Restart the launcher",
            L"A previous launch already used this session's Java runtime. Close the launcher and open it again.",
            1.0f);
        return false;
    }

    const LoaderId loaderId = ParseLoaderId(loader);
    std::wstring loaderLabel = L"Minecraft";
    if (loaderId == LoaderId::Fabric) {
        loaderLabel = L"Fabric";
    } else if (loaderId == LoaderId::NeoForge) {
        loaderLabel = L"NeoForge";
    } else if (loaderId == LoaderId::Forge) {
        loaderLabel = L"Forge";
    }
    reportProgress(
        L"Starting Minecraft",
        (L"Preparing " + loaderLabel + L" and launch files.").c_str(),
        0.12f);
    const std::wstring effectiveMainClass = LoaderDefaultMainClass(loaderId, mainClassName);
    std::wstring mainClassPath = effectiveMainClass;
    std::replace(mainClassPath.begin(), mainClassPath.end(), L'.', L'/');
    const std::wstring libraryDir = exeDir + L"\\game\\libraries";
    const std::wstring neoForgeVersionForLaunch = NeoForgeVersionFromLaunchVersion(launchVersion);
    std::wstring neoFormVersionForLaunch = !neoFormVersion.empty()
        ? neoFormVersion
        : FirstArgValue(extraGameArgs, L"--fml.neoFormVersion");
    if (neoFormVersionForLaunch.empty() && loaderId == LoaderId::Forge) {
        neoFormVersionForLaunch = FirstArgValue(extraGameArgs, L"--fml.mcpVersion");
    }
    const std::wstring neoForgeUniversalJar =
        loaderId == LoaderId::NeoForge && !neoForgeVersionForLaunch.empty()
            ? libraryDir + L"\\" + MavenPath(L"net.neoforged", L"neoforge", neoForgeVersionForLaunch, L"universal")
            : L"";
    std::wstring neoForgeMinecraftSrgJar;
    if (!minecraftVersion.empty() && !neoFormVersionForLaunch.empty()) {
        const std::wstring mcAndNeoForm = minecraftVersion + L"-" + neoFormVersionForLaunch;
        neoForgeMinecraftSrgJar = libraryDir + L"\\" + MavenPath(L"net.minecraft", L"client", mcAndNeoForm, L"srg");
    }
    auto expandLaunchArg = [&](std::wstring arg) {
        auto replaceAll = [&](const std::wstring& from, const std::wstring& to) {
            size_t pos = 0;
            while ((pos = arg.find(from, pos)) != std::wstring::npos) {
                arg.replace(pos, from.size(), to);
                pos += to.size();
            }
        };
        replaceAll(L"${library_directory}", fwd(libraryDir));
        replaceAll(L"${classpath_separator}", L";");
        replaceAll(L"${version_name}", launchVersion);
        replaceAll(L"${natives_directory}", fwd(nativesDir));
        replaceAll(L"${launcher_name}", L"BanditLauncher");
        replaceAll(L"${launcher_version}", L"1");
        return arg;
    };
    const std::wstring jnaTmpDir = gameDir + L"\\tmp";
    const std::wstring lwjglTmpDir = exeDir + L"\\tmp";
    const std::wstring launcherOverrideDir = gameDir + L"\\launcher-overrides";
    const std::wstring packagedNativesDir = packageDir + L"\\natives";
    const bool suppliedNativesReady =
        GetFileAttributesW((nativesDir + L"\\lwjgl.dll").c_str()) != INVALID_FILE_ATTRIBUTES &&
        GetFileAttributesW((nativesDir + L"\\glfw.dll").c_str()) != INVALID_FILE_ATTRIBUTES;
    const bool packagedNativesReady =
        GetFileAttributesW((packagedNativesDir + L"\\lwjgl.dll").c_str()) != INVALID_FILE_ATTRIBUTES &&
        GetFileAttributesW((packagedNativesDir + L"\\glfw.dll").c_str()) != INVALID_FILE_ATTRIBUTES;
    const std::wstring lwjglNativeDir =
        suppliedNativesReady ? nativesDir :
        (packagedNativesReady ? packagedNativesDir : nativesDir);
    const std::wstring lwjglGlfwDll = lwjglNativeDir + L"\\glfw.dll";
    const std::wstring logConfigPath = exeDir + L"\\game\\log_configs\\client-uwp.xml";
    const std::wstring fabricLogPath = gameDir + L"\\logs\\fabric-loader.log";
    const std::wstring forgeLogPath = gameDir + L"\\logs\\forge-loader.log";
    const std::wstring latestLogPath = gameDir + L"\\logs\\latest.log";
    const std::wstring xboxCompatLogPath = gameDir + L"\\xbox_compat.log";
    const std::wstring launcherLogDir = LogsCurrentDir(exeDir);
    EnsureDirectoryTree(launcherLogDir);
    const std::wstring stderrLogPath = launcherLogDir + L"\\stderr_stream.log";

    EnsureDirectoryTree(gameDir + L"\\logs");
    EnsureDirectoryTree(gameDir + L"\\crash-reports");
    EnsureDirectoryTree(gameDir + L"\\saves");
    EnsureDirectoryTree(gameDir + L"\\resourcepacks");
    EnsureDirectoryTree(gameDir + L"\\screenshots");
    EnsureDirectoryTree(gameDir + L"\\config");
    EnsureDirectoryTree(jnaTmpDir);
    EnsureDirectoryTree(launcherOverrideDir);
    DeleteDirectoryTree(gameDir + L"\\showdown");
    DeleteDirectoryTree(exeDir + L"\\showdown");
    EnsureDirectoryTree(gameDir + L"\\showdown");
    EnsureDirectoryTree(exeDir + L"\\showdown");
    EnsureDirectoryTree(userModsDir);
    if ((loaderId == LoaderId::Forge || loaderId == LoaderId::NeoForge) &&
        !bundledModsDir.empty() &&
        DirectoryExists(bundledModsDir)) {
        CopyDirectoryContentsAlways(bundledModsDir, userModsDir);
        WriteLogF(L"Synced bundled loader mods from %s to %s", bundledModsDir.c_str(), userModsDir.c_str());
    }
    const int earlyBlockedRemoved = PurgeBlockedModsFromDir(exeDir, userModsDir);
    if (earlyBlockedRemoved > 0) {
        WriteLogF(L"Removed %d blocked mod(s) from active profile before configuring launch", earlyBlockedRemoved);
    }
    ConfigureKnownModDefaults(gameDir, userModsDir, minecraftVersion, bundledModsDir);
    if (SetCurrentDirectoryW(gameDir.c_str())) {
        WriteLogF(L"Process current directory set to gameDir: %s", gameDir.c_str());
    } else {
        WriteLogF(L"Failed to set process current directory to gameDir err=%u", GetLastError());
    }
    EnsureDirectoryTree(lwjglTmpDir);
    DeleteFileW(javaLog.c_str());
    DeleteFileW(stderrLogPath.c_str());
    DeleteFileW(fabricLogPath.c_str());
    DeleteFileW(forgeLogPath.c_str());
    DeleteFileW(latestLogPath.c_str());
    DeleteFileW(xboxCompatLogPath.c_str());

    if (!RedirectStdStreams(javaLog, stderrLogPath)) {
        WriteLogF(L"Failed to redirect stdout/stderr errno=%d winerr=%u", errno, GetLastError());
    } else {
        WriteLog(L"stdout redirected to logs/current/java_output.log; stderr redirected to logs/current/stderr_stream.log");
    }

    const std::vector<LogTailerConfig> tailerConfigs = {
        LogTailerConfig{ javaLog, L"java_output.log" },
        LogTailerConfig{ stderrLogPath, L"stderr_stream.log" },
        LogTailerConfig{ loaderId == LoaderId::Fabric ? fabricLogPath : forgeLogPath, LoaderTailLogLabel(loaderId) },
        LogTailerConfig{ latestLogPath, L"latest.log" },
        LogTailerConfig{ xboxCompatLogPath, L"xbox_compat.log" }
    };
    LogTailerGuard logTailers(StartLogTailers(tailerConfigs));

    FILE* af = nullptr;
    _wfopen_s(&af, argsPath.c_str(), L"w");
    if (!af) {
        WriteLogF(L"FAILED args file err=%u", GetLastError());
        return false;
    }

    LoaderJvmContext loaderCtx;
    loaderCtx.loader = loaderId;
    loaderCtx.exeDir = exeDir;
    loaderCtx.packageDir = packageDir;
    loaderCtx.gameDir = gameDir;
    loaderCtx.clientJar = clientJar;
    loaderCtx.bundledModsDir = bundledModsDir;
    loaderCtx.userModsDir = userModsDir;
    loaderCtx.launchVersion = launchVersion;
    loaderCtx.loaderVersion = loaderVersion;
    loaderCtx.minecraftVersion = minecraftVersion;
    loaderCtx.neoFormVersion = neoFormVersion;
    loaderCtx.neoForgeInstallToolsVersion = neoForgeInstallToolsVersion;
    loaderCtx.neoForgeJarSplitterVersion = neoForgeJarSplitterVersion;
    loaderCtx.neoForgeBinaryPatcherVersion = neoForgeBinaryPatcherVersion;
    loaderCtx.neoForgeAutoRenamingToolVersion = neoForgeAutoRenamingToolVersion;
    loaderCtx.extraGameArgs = extraGameArgs;
    loaderCtx.launcherOverrideDir = launcherOverrideDir;
    loaderCtx.launcherLogDir = launcherLogDir;
    loaderCtx.fabricLogPath = fabricLogPath;
    loaderCtx.libraryDir = libraryDir;
    loaderCtx.neoForgeMinecraftSrgJar = neoForgeMinecraftSrgJar;
    loaderCtx.expandLaunchArg = expandLaunchArg;

    std::wstring effectiveClassPath = classPath;
    LoaderJvmSetupResult loaderSetup;
    LoaderAdjustClasspath(loaderCtx, effectiveClassPath, loaderSetup);
    effectiveClassPath = loaderSetup.effectiveClassPath;
    const bool neoForgeStartedWithGameClassPath = loaderSetup.neoForgeStartedWithGameClassPath;

    std::vector<std::string> vmOptionStorage;
    vmOptionStorage.reserve(64);
    vmOptionStorage.push_back("-Xmx3G");
    vmOptionStorage.push_back("-Xms512M");
    vmOptionStorage.push_back("-XX:MaxDirectMemorySize=512M");

    vmOptionStorage.push_back("-XX:+IgnoreUnrecognizedVMOptions");
    vmOptionStorage.push_back("-XX:+UnlockExperimentalVMOptions");
    vmOptionStorage.push_back("-XX:+UseG1GC");
    vmOptionStorage.push_back("-XX:MaxGCPauseMillis=50");
    vmOptionStorage.push_back("-XX:G1NewSizePercent=20");
    vmOptionStorage.push_back("-XX:G1ReservePercent=20");
    vmOptionStorage.push_back("-XX:G1HeapRegionSize=32M");
    vmOptionStorage.push_back("-XX:+PerfDisableSharedMem");
    WriteLog(L"JVM heap: -Xmx3G -Xms512M -XX:MaxDirectMemorySize=512M, G1 at 50ms pause target");
    vmOptionStorage.push_back("--enable-native-access=ALL-UNNAMED");
    vmOptionStorage.push_back("--add-opens=jdk.zipfs/jdk.nio.zipfs=ALL-UNNAMED");
    const std::wstring selectedJavaBasePatchName =
        javaBasePatchName.empty() ? L"java-base-uwp-filesystem.jar" : javaBasePatchName;
    const std::wstring localJavaBasePatch = exeDir + L"\\" + selectedJavaBasePatchName;
    const std::wstring packagedJavaBasePatch = packageDir + L"\\" + selectedJavaBasePatchName;
    const std::wstring javaBasePatch =
        GetFileAttributesW(localJavaBasePatch.c_str()) != INVALID_FILE_ATTRIBUTES
            ? localJavaBasePatch
            : packagedJavaBasePatch;
    if (GetFileAttributesW(javaBasePatch.c_str()) != INVALID_FILE_ATTRIBUTES) {
        vmOptionStorage.push_back("--patch-module=java.base=" + w2a(fwd(javaBasePatch)));
        WriteLogF(L"Java base UWP filesystem patch enabled: %s", javaBasePatch.c_str());
    } else {
        WriteLogF(L"Java base UWP filesystem patch missing: %s", javaBasePatch.c_str());
    }
    const std::wstring selectedJavaZipfsPatchName =
        javaZipfsPatchName.empty() ? L"java-zipfs-realpath.jar" : javaZipfsPatchName;
    const std::wstring localJavaZipfsPatch = exeDir + L"\\" + selectedJavaZipfsPatchName;
    const std::wstring packagedJavaZipfsPatch = packageDir + L"\\" + selectedJavaZipfsPatchName;
    const std::wstring javaZipfsPatch =
        GetFileAttributesW(localJavaZipfsPatch.c_str()) != INVALID_FILE_ATTRIBUTES
            ? localJavaZipfsPatch
            : packagedJavaZipfsPatch;
    if (GetFileAttributesW(javaZipfsPatch.c_str()) != INVALID_FILE_ATTRIBUTES) {
        vmOptionStorage.push_back("--patch-module=jdk.zipfs=" + w2a(fwd(javaZipfsPatch)));
        WriteLogF(L"Java ZipFS realpath patch enabled: %s", javaZipfsPatch.c_str());
    } else {
        WriteLogF(L"Java ZipFS realpath patch missing: %s", javaZipfsPatch.c_str());
    }
    const bool useJava21DesktopPatch = selectedJavaBasePatchName.find(L"-21.jar") != std::wstring::npos;
    const std::wstring javaDesktopPatchName = useJava21DesktopPatch ? L"java-desktop-uwp-awt-21.jar" : L"java-desktop-uwp-awt.jar";
    const std::wstring localJavaDesktopPatch = exeDir + L"\\" + javaDesktopPatchName;
    const std::wstring packagedJavaDesktopPatch = packageDir + L"\\" + javaDesktopPatchName;
    const std::wstring javaDesktopPatch =
        GetFileAttributesW(localJavaDesktopPatch.c_str()) != INVALID_FILE_ATTRIBUTES
            ? localJavaDesktopPatch
            : packagedJavaDesktopPatch;
    if (GetFileAttributesW(javaDesktopPatch.c_str()) != INVALID_FILE_ATTRIBUTES) {
        vmOptionStorage.push_back("--patch-module=java.desktop=" + w2a(fwd(javaDesktopPatch)));
        WriteLogF(L"Java desktop UWP AWT patch enabled: %s", javaDesktopPatch.c_str());
    } else {
        WriteLogF(L"Java desktop UWP AWT patch missing: %s", javaDesktopPatch.c_str());
    }
    vmOptionStorage.push_back("-Djava.home=" + w2a(fwd(jreDir)));
    vmOptionStorage.push_back("-Djava.security.properties==" + w2a(fwd(jreDir + L"\\conf\\security\\xbox.properties")));
    vmOptionStorage.push_back("-Djava.security.egd=file:/dev/urandom");
    vmOptionStorage.push_back("-Djava.awt.headless=true");
    vmOptionStorage.push_back("-Dbanditvault.awt.skipDesktopProperties=true");
    vmOptionStorage.push_back("-Dswing.defaultlaf=javax.swing.plaf.metal.MetalLookAndFeel");
    vmOptionStorage.push_back("-Dfml.readTimeout=300");
    vmOptionStorage.push_back("-Dfml.loginTimeout=300");
    LoaderAddJvmOptions(loaderCtx, vmOptionStorage);
    for (const std::wstring& arg : extraJvmArgs) {
        const std::wstring expanded = expandLaunchArg(arg);
        if (IsJvmMemoryOption(expanded)) {
            WriteLogF(L"Ignoring modpack JVM memory override on Xbox: %s", expanded.c_str());
        } else if (!expanded.empty()) {
            vmOptionStorage.push_back(w2a(expanded));
        }
    }
    if (VerboseLoggingEnabled()) {
        vmOptionStorage.push_back("-Dmixin.debug.verbose=true");
    }
    vmOptionStorage.push_back("-Djava.io.tmpdir=" + w2a(fwd(jnaTmpDir)));
    vmOptionStorage.push_back("-Djna.tmpdir=" + w2a(fwd(jnaTmpDir)));
    vmOptionStorage.push_back("-Djna.nosys=true");
    vmOptionStorage.push_back("-Djna.nounpack=true");
    vmOptionStorage.push_back("-Djna.boot.library.name=jnidispatch");
    vmOptionStorage.push_back("-Djna.boot.library.path=" + w2a(fwd(nativesDir)));
    vmOptionStorage.push_back("-Djava.library.path=" + w2a(fwd(lwjglNativeDir)));
    vmOptionStorage.push_back("-Dorg.lwjgl.librarypath=" + w2a(fwd(lwjglNativeDir)));
    if (VerboseLoggingEnabled()) {
        vmOptionStorage.push_back("-Dorg.lwjgl.util.Debug=true");
        vmOptionStorage.push_back("-Dorg.lwjgl.util.DebugLoader=true");
        vmOptionStorage.push_back("-Djna.debug_load=true");
        vmOptionStorage.push_back("-Djna.debug_load.jna=true");
    }
    vmOptionStorage.push_back("-Dorg.lwjgl.system.SharedLibraryExtractDirectory=" + w2a(fwd(lwjglTmpDir)));
    vmOptionStorage.push_back("-Dorg.lwjgl.glfw.libname=" + w2a(fwd(lwjglGlfwDll)));
    WriteLogF(L"LWJGL native directory: %s", lwjglNativeDir.c_str());
    WriteLogF(L"LWJGL GLFW library forced: %s", lwjglGlfwDll.c_str());
    std::wstring graphicsRuntime = GetEnvVarString(L"MC_GRAPHICS_RUNTIME");
    if (graphicsRuntime.empty()) {
        graphicsRuntime = L"mesa";
    }
    const std::wstring packagedOpenGl = packageDir + L"\\graphics\\" + graphicsRuntime + L"\\opengl32.dll";
    const std::wstring localOpenGl = exeDir + L"\\graphics\\" + graphicsRuntime + L"\\opengl32.dll";
    const std::wstring selectedOpenGl =
        GetFileAttributesW(packagedOpenGl.c_str()) != INVALID_FILE_ATTRIBUTES
            ? packagedOpenGl
            : localOpenGl;
    if (GetFileAttributesW(selectedOpenGl.c_str()) != INVALID_FILE_ATTRIBUTES) {
        vmOptionStorage.push_back("-Dorg.lwjgl.opengl.libname=" + w2a(fwd(selectedOpenGl)));
        WriteLogF(L"LWJGL OpenGL library forced: %s", selectedOpenGl.c_str());
    } else {
        WriteLogF(L"LWJGL OpenGL library override missing: %s", selectedOpenGl.c_str());
    }
    WriteLogF(L"Log4j configuration: %s", FileUriFromPath(logConfigPath).c_str());
    vmOptionStorage.push_back("-Djava.class.path=" + w2a(effectiveClassPath));
    if (loaderId == LoaderId::Forge) {
        vmOptionStorage.push_back("-DlegacyClassPath=" + w2a(effectiveClassPath));
        WriteLog(L"Forge legacyClassPath aligned with game classpath");
    }
    vmOptionStorage.push_back("-Duser.dir=" + w2a(fwd(gameDir)));
    vmOptionStorage.push_back("-Dlog4j.configurationFile=" + w2a(FileUriFromPath(logConfigPath)));
    vmOptionStorage.push_back("-XX:ErrorFile=" + w2a(fwd(gameDir + L"\\hs_err_pid%p.log")));

    const std::wstring jvmArgsPath = exeDir + L"\\jvm_args.txt";
    std::wstring jvmArgsText;
    if (ReadTextFile(jvmArgsPath, jvmArgsText)) {
        int applied = 0;
        size_t pos = 0;
        while (pos < jvmArgsText.size()) {
            size_t end = jvmArgsText.find(L'\n', pos);
            if (end == std::wstring::npos) end = jvmArgsText.size();
            const std::wstring arg = TrimWhitespace(jvmArgsText.substr(pos, end - pos));
            pos = end + 1;

            if (arg.empty() || arg[0] == L'#') continue;
            if (arg[0] != L'-') continue;
            vmOptionStorage.push_back(w2a(arg));
            ++applied;
        }
    }

    std::vector<std::string> appArgs = {
        "--username", authConfig.username,
        "--version", w2a(launchVersion),
        "--gameDir", w2a(fwd(gameDir)),
        "--assetsDir", w2a(fwd(assetsDir)),
        "--assetIndex", w2a(assetIndex),
        "--uuid", authConfig.uuid,
        "--accessToken", authConfig.accessToken,
        "--versionType", "release"
    };
    for (const std::wstring& arg : extraGameArgs) {
        appArgs.push_back(w2a(expandLaunchArg(arg)));
    }

    fprintf(af, "# JVM options\n");
    for (const auto& opt : vmOptionStorage) {
        fprintf(af, "%s\n", opt.c_str());
    }
    fprintf(af, "# Main class\n%s\n", w2a(effectiveMainClass).c_str());
    fprintf(af, "# App args\n");
    for (size_t i = 0; i < appArgs.size(); ++i) {
        fprintf(af, "%s\n", launchdiag::DiagnosticMinecraftAppArg(appArgs, i).c_str());
    }
    fclose(af);
    WriteLog(L"Embedded JVM options written");
    reportProgress(
        L"Starting Java runtime",
        L"Loading the embedded JVM.",
        0.24f);

    HMODULE jvmModule = nullptr;
    if (!PreloadJvm(exeDir, jreDir, packagedJreRelativeDir, nativesDir, &jvmModule)) {
        return false;
    }

    auto createJavaVm = reinterpret_cast<JNI_CreateJavaVM_t>(GetProcAddress(jvmModule, "JNI_CreateJavaVM"));
    if (!createJavaVm) {
        WriteLogF(L"GetProcAddress(JNI_CreateJavaVM) failed err=%u", GetLastError());
        return false;
    }

    std::vector<JavaVMOption> vmOptions(vmOptionStorage.size());
    for (size_t i = 0; i < vmOptionStorage.size(); ++i) {
        vmOptions[i].optionString = const_cast<char*>(vmOptionStorage[i].c_str());
        vmOptions[i].extraInfo = nullptr;
    }

    JavaVMInitArgs vmArgs = {};
    vmArgs.version = JNI_VERSION_21;
    vmArgs.nOptions = static_cast<jint>(vmOptions.size());
    vmArgs.options = vmOptions.data();
    vmArgs.ignoreUnrecognized = JNI_FALSE;

    JavaVM* vm = nullptr;
    JNIEnv* env = nullptr;
    const jint createResult = createJavaVm(&vm, reinterpret_cast<void**>(&env), &vmArgs);
    WriteLogF(L"JNI_CreateJavaVM => %d", createResult);
    if (createResult != JNI_OK || !vm || !env) {
        return false;
    }
    g_embeddedJvmUsed = true;

    reportProgress(
        L"Starting Java runtime",
        L"Java is online. Preparing loader startup next.",
        0.38f);

    if (!LoaderPrepareArtifactsAfterJvm(env, loaderCtx, effectiveClassPath, neoForgeStartedWithGameClassPath)) {
        DestroyEmbeddedJvm(vm, env);
        return false;
    }
    reportProgress(
        (L"Loading " + loaderLabel).c_str(),
        L"Loader artifacts are ready. Minecraft main is starting next.",
        0.52f);

    jclass mainClass = env->FindClass(w2a(mainClassPath).c_str());
    if (!mainClass || CheckAndLogJavaException(env, (L"FindClass(" + effectiveMainClass + L")").c_str())) {
        DestroyEmbeddedJvm(vm, env);
        return false;
    }

    jmethodID mainMethod = env->GetStaticMethodID(mainClass, "main", "([Ljava/lang/String;)V");
    if (!mainMethod || CheckAndLogJavaException(env, L"GetStaticMethodID(main)")) {
        DestroyEmbeddedJvm(vm, env);
        return false;
    }

    jclass stringClass = env->FindClass("java/lang/String");
    if (!stringClass || CheckAndLogJavaException(env, L"FindClass(String)")) {
        DestroyEmbeddedJvm(vm, env);
        return false;
    }

    jobjectArray argv = env->NewObjectArray(static_cast<jsize>(appArgs.size()), stringClass, nullptr);
    if (!argv || CheckAndLogJavaException(env, L"NewObjectArray")) {
        DestroyEmbeddedJvm(vm, env);
        return false;
    }

    for (jsize i = 0; i < static_cast<jsize>(appArgs.size()); ++i) {
        jstring value = env->NewStringUTF(appArgs[i].c_str());
        if (!value || CheckAndLogJavaException(env, L"NewStringUTF")) {
            DestroyEmbeddedJvm(vm, env);
            return false;
        }
        env->SetObjectArrayElement(argv, i, value);
        env->DeleteLocalRef(value);
    }

    WriteLogF(L"Invoking %s.main via embedded JVM", effectiveMainClass.c_str());
    reportProgress(
        (L"Loading " + loaderLabel).c_str(),
        L"Mods and libraries are starting. The Mojang loading screen appears once graphics initialize.",
        0.64f);

    std::atomic<bool> javaMainRunning{ true };
    std::thread javaMainWatchdog([&javaMainRunning, vm, effectiveMainClass]() {
        const HRESULT roHr = RoInitialize(RO_INIT_MULTITHREADED);
        unsigned seconds = 0;
        while (javaMainRunning.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            ++seconds;
        }
        if (SUCCEEDED(roHr)) {
            RoUninitialize();
        }
    });

    env->CallStaticVoidMethod(mainClass, mainMethod, argv);
    javaMainRunning.store(false);
    if (javaMainWatchdog.joinable()) {
        javaMainWatchdog.join();
    }

    if (CheckAndClearReturnToLauncherSignal(env, L"CallStaticVoidMethod(main)")) {
        LogTextFileTail(javaLog, L"java_output.log");
        LogTextFileTail(stderrLogPath, L"stderr_stream.log");
        WriteLog(L"Minecraft requested return to launcher");
        StopLogTailers();
        return true;
    }

    if (CheckAndLogJavaMainException(env, L"CallStaticVoidMethod(main)")) {
        LogTextFileTail(javaLog, L"java_output.log");
        LogTextFileTail(stderrLogPath, L"stderr_stream.log");
        WriteLog(L"Embedded JVM failed after startup; terminating host process");
        StopLogTailers();
        ExitProcess(1);
        return false;
    }

    WriteLogF(L"%s.main returned", effectiveMainClass.c_str());
    g_minecraftRunning.store(false);
    WriteLog(L"Minecraft exited normally; returning to launcher menu");
    StopLogTailers();
    return true;
}
