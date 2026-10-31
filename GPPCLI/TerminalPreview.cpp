module;

#include "../GalTranslPP/GPPMacros.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_options.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/dom/node.hpp>
#ifdef _WIN32
#include <Windows.h>
#elif defined(__APPLE__)
#include <unistd.h>
#endif

module TerminalPreview;

NAMESPACE_BEGIN(gpp::cli)

struct PreviewPage {
    std::string title;
    std::vector<std::string> lines;
};

struct ProjectPreview {
    std::vector<PreviewPage> pages;
    std::vector<std::string> problems;
    bool ready = true;
    std::string overviewFormat;
};

std::string enabledText(bool enabled)
{
    return enabled ? gppTr("TerminalPreview", "开启").toStdString() : gppTr("TerminalPreview", "关闭").toStdString();
}

void addRow(PreviewPage& page, const QString& label, const std::string& value)
{
    page.lines.push_back(label.toStdString() + ": " + value);
}

std::string protocolName(ApiProtocol protocol)
{
    switch (protocol) {
    case ApiProtocol::OpenAI:
        return "OpenAI Chat Completions";
    case ApiProtocol::OpenAIRes:
        return "OpenAI Responses";
    case ApiProtocol::Claude:
        return "Claude Messages";
    case ApiProtocol::Gemini:
        return "Gemini GenerateContent";
    case ApiProtocol::GeminiInterAction:
        return "Gemini Interactions";
    }
    return {};
}

ProjectPreview readPreview(const fs::path& projectDir)
{
    ProjectPreview preview;
    const auto config = uparseToml(projectDir / L"Config.toml");
    const auto settings = readTranslationSettings(config);
    const auto engineName = toml::find<std::string>(config, "plugins", "transEngine");
    const auto engineIt = names2TransEngine.find(engineName);
    if (engineIt == names2TransEngine.end()) {
        throw std::invalid_argument(
            gppTr("NormalJsonTranslator.normalJsonInit", "无效的 TransEngine: %1").arg(engineName).toStdString());
    }
    const auto engine = engineIt->second;
    const bool agentEnabled = settings.agentEnabled && supportsTransAgent(engine);
    validateTranslationSettings(settings, agentEnabled);
    preview.overviewFormat = settings.problemOverviewFormat;
    if (ProblemOverviewTracker::hasUnimportedChanges(projectDir, transCacheDirName, config)) {
        preview.problems.push_back(gppTr("TerminalPreview", "检测到 %1 被修改但未导入。")
                .arg(wide2Ascii(ProblemOverviewTracker::overviewPath(projectDir, config).filename()))
                .toStdString());
    }

    PreviewPage general{ gppTr("TerminalPreview", "概览").toStdString() };
    addRow(general, gppTr("TerminalPreview", "项目"), wide2Ascii(projectDir));
    addRow(general, gppTr("TerminalPreview", "翻译引擎"), engineName);
    addRow(general, gppTr("TerminalPreview", "目标语言"), settings.targetLang);
    addRow(general, gppTr("TerminalPreview", "Agent"), enabledText(agentEnabled));
    if (settings.agentEnabled && !agentEnabled) {
        preview.problems.push_back(
            gppTr("TerminalPreview", "当前翻译引擎不支持 Agent，运行时将自动关闭").toStdString());
    }
    addRow(general, gppTr("TerminalPreview", "并发数"), std::to_string(settings.threadsNum));
    addRow(general,
        gppTr("TerminalPreview", "每批句数"),
        std::to_string(engine == TransEngine::NameTrans ? settings.nameTransBatchSize : settings.batchSize));
    addRow(general, gppTr("TerminalPreview", "文件顺序"), settings.sortMethod);
    addRow(general,
        gppTr("TerminalPreview", "单文件分割"),
        settings.splitFileMethod == "No" ? "No"
                                         : std::format("{} / {}", settings.splitFileMethod, settings.splitFileNum));
    addRow(general, gppTr("TerminalPreview", "重复块复用"), enabledText(settings.reuseRepeatedBlocks));
    addRow(general, gppTr("TerminalPreview", "换行符"), settings.linebreakSymbol);
    addRow(general, gppTr("TerminalPreview", "缓存保存间隔"), std::to_string(settings.saveCacheInterval));
    addRow(general, gppTr("TerminalPreview", "最大请求次数"), std::to_string(settings.maxRequestCount));
    addRow(general, gppTr("TerminalPreview", "智能重试"), enabledText(settings.smartRetry));
    addRow(general, gppTr("TerminalPreview", "改善拒答档位"), std::to_string(settings.enhanceJailbreak));
    addRow(general, gppTr("TerminalPreview", "额度检测"), enabledText(settings.checkQuota));
    addRow(general,
        gppTr("TerminalPreview", "整批重翻"),
        agentEnabled ? gppTr("TerminalPreview", "当前模式不使用").toStdString()
                     : enabledText(settings.retransAllWhenFail));
    addRow(general, gppTr("TerminalPreview", "问题清单格式"), settings.problemOverviewFormat);
    preview.pages.push_back(std::move(general));

    PreviewPage apis{ gppTr("TerminalPreview", "Api").toStdString() };
    if (usesTranslationApi(engine)) {
        const auto strategy = toml::find_or(config, "backend", "apiStrategy", "random");
        if (strategy != "random" && strategy != "fallback") {
            throw std::invalid_argument(
                gppTr("NormalJsonTranslator.normalJsonInit", "apiStrategy 必须为 random 或 fallback").toStdString());
        }
        addRow(apis, gppTr("TerminalPreview", "令牌策略"), strategy);
        addRow(apis,
            gppTr("TerminalPreview", "请求超时（秒）"),
            std::to_string(toml::find_or(config, "backend", "apiTimeout", 300)));
        const auto entries = readProjectApis(config,
            engine,
            agentEnabled,
            [&](const std::string& warning)
            {
                preview.problems.push_back(warning);
            });
        for (const auto& entry : entries) {
            const auto& api = entry.tokens.front();
            apis.lines.push_back("");
            apis.lines.push_back(std::format("[{}] {}", entry.index + 1, entry.name));
            addRow(apis, gppTr("TerminalPreview", "协议"), protocolName(api.protocol));
            addRow(apis, gppTr("TerminalPreview", "模型"), api.modelName);
            addRow(apis, gppTr("TerminalPreview", "地址"), api.apiurl);
            addRow(apis, gppTr("TerminalPreview", "可用 Key 数量"), std::to_string(entry.tokens.size()));
            addRow(apis, gppTr("TerminalPreview", "思考等级"), api.thinkingLevel);
            addRow(apis, gppTr("TerminalPreview", "系统代理"), enabledText(api.useSystemProxy));
            addRow(apis,
                gppTr("TerminalPreview", "流式输出"),
                agentEnabled ? gppTr("TerminalPreview", "当前模式不使用").toStdString() : enabledText(api.stream));
            if (agentEnabled) {
                const bool gemini =
                    api.protocol == ApiProtocol::Gemini || api.protocol == ApiProtocol::GeminiInterAction;
                addRow(apis,
                    gppTr("TerminalPreview", "严格工具参数"),
                    gemini ? gppTr("TerminalPreview", "当前模式不使用").toStdString() : api.agentStrictTools);
                const bool stateful = api.agentStateful && (api.protocol == ApiProtocol::OpenAIRes ||
                                                               api.protocol == ApiProtocol::GeminiInterAction);
                addRow(apis, gppTr("TerminalPreview", "服务端会话接续"), enabledText(stateful));
                const bool nativeCompaction =
                    api.agentNativeAutoCompaction &&
                    (api.protocol == ApiProtocol::OpenAIRes || api.protocol == ApiProtocol::Claude);
                addRow(apis, gppTr("TerminalPreview", "原生自动压缩"), enabledText(nativeCompaction));
                std::string threshold = gppTr("TerminalPreview", "当前模式不使用").toStdString();
                if (nativeCompaction) {
                    if (api.agentCompactThresholdTokens > 0) {
                        threshold = std::to_string(api.agentCompactThresholdTokens);
                    }
                    else if (api.protocol == ApiProtocol::Claude) {
                        threshold = gppTr("TerminalPreview", "服务端默认").toStdString();
                    }
                    else {
                        threshold = "100000";
                    }
                }
                addRow(apis, gppTr("TerminalPreview", "原生压缩阈值（tokens）"), threshold);
            }
        }
        if (entries.empty()) {
            preview.ready = false;
            preview.problems.push_back(
                gppTr("NormalJsonTranslator.normalJsonInit", "找不到可用的 Api key").toStdString());
        }
    }
    else {
        apis.lines.push_back(gppTr("TerminalPreview", "当前模式不使用 Api 和提示词").toStdString());
    }
    preview.pages.push_back(std::move(apis));

    PreviewPage dictionaries{ gppTr("TerminalPreview", "字典与规则").toStdString() };
    for (const std::string type : { "pre", "gpt", "post" }) {
        const bool used = type == "pre" ||
                          (type == "gpt" && engine != TransEngine::DumpName && engine != TransEngine::ShowNormal) ||
                          (type == "post" && engine != TransEngine::DumpName && engine != TransEngine::ShowNormal &&
                              engine != TransEngine::NameTrans && engine != TransEngine::GenDict);
        dictionaries.lines.push_back(
            type + (used ? "" : " / " + gppTr("TerminalPreview", "当前模式不使用").toStdString()));
        for (const auto& file : resolveProjectDictionaries(projectDir, config, type)) {
            dictionaries.lines.push_back(
                "  " + wide2Ascii(fs::absolute(file.path)) +
                (file.exists ? "" : " / " + gppTr("TerminalPreview", "文件不存在，将忽略").toStdString()));
        }
    }
    for (const auto& [key, fallback] : std::vector<std::pair<std::string, bool>>{ { "usePreDictInName", false },
             { "usePostDictInName", false },
             { "usePreDictInMsg", true },
             { "usePostDictInMsg", true },
             { "useGPTDictToReplaceName", false } }) {
        dictionaries.lines.push_back(key + ": " + enabledText(toml::find_or(config, "dictionary", key, fallback)));
    }
    if (config.contains("problemAnalyze")) {
        dictionaries.lines.push_back("");
        for (const auto& [key, value] : config.at("problemAnalyze").as_table()) {
            // 只列出检查开关及规则，保留规则的原始类型和结构。
            if (value.is_table() && value.contains("enable")) {
                dictionaries.lines.push_back(key + ": " + enabledText(toml::find_or(value, "enable", false)));
            }
            else {
                dictionaries.lines.push_back(key + ": " + toml::format(value));
            }
        }
    }
    preview.pages.push_back(std::move(dictionaries));

    PreviewPage plugins{ gppTr("TerminalPreview", "插件").toStdString() };
    addRow(plugins, gppTr("TerminalPreview", "文件插件"), toml::find_or(config, "plugins", "filePlugin", "NormalJson"));
    addRow(
        plugins, gppTr("TerminalPreview", "脚本基类"), toml::find_or(config, "plugins", "baseClassName", "NormalJson"));
    const auto textPlugins = toml::find<std::optional<std::vector<std::string>>>(config, "plugins", "textPlugins");
    if (textPlugins) {
        for (const auto& plugin : *textPlugins) {
            plugins.lines.push_back((plugin.starts_with('>') ? "[-] " : "[+] ") +
                                    replaceStr(plugin, "%PROJECT_DIR%", wide2Ascii(projectDir)));
        }
    }
    preview.pages.push_back(std::move(plugins));

    PreviewPage agent{ gppTr("TerminalPreview", "Agent").toStdString() };
    addRow(agent, gppTr("TerminalPreview", "Agent"), enabledText(agentEnabled));
    if (agentEnabled) {
        addRow(agent, gppTr("TerminalPreview", "允许运行 Python"), enabledText(settings.agentAllowPython));
        addRow(agent, gppTr("TerminalPreview", "每批最大轮次"), std::to_string(settings.agentMaxTurnsPerChunk));
        addRow(agent,
            gppTr("TerminalPreview", "本地压缩阈值（字节）"),
            std::to_string(settings.agentCompactContextThresholdBytes));
        agent.lines.push_back(gppTr("TerminalPreview", "启用原生自动压缩的 Api 不使用本地压缩阈值").toStdString());
        addRow(agent, gppTr("TerminalPreview", "搜索结果上限"), std::to_string(settings.agentSearchResultLimit));
        addRow(agent, gppTr("TerminalPreview", "读取行数上限"), std::to_string(settings.agentContextLinesLimit));
        auto note = fs::path(ascii2Wide(settings.projectNotePath));
        if (note.is_relative()) {
            note = projectDir / note;
        }
        addRow(agent,
            gppTr("TerminalPreview", "项目笔记"),
            wide2Ascii(note) +
                (fs::exists(note) ? "" : " / " + gppTr("TerminalPreview", "文件不存在，将忽略").toStdString()));
    }
    preview.pages.push_back(std::move(agent));

    PreviewPage prompts{ gppTr("TerminalPreview", "提示词").toStdString() };
    if (usesTranslationApi(engine)) {
        try {
            const auto projectPrompts = readProjectPrompts(projectDir, engine, agentEnabled);
            for (const ProjectPrompt* prompt : { &projectPrompts.system, &projectPrompts.user }) {
                prompts.lines.push_back(prompt->key + " / " + wide2Ascii(fs::absolute(prompt->path)));
                prompts.lines.push_back(prompt->text);
                prompts.lines.push_back("");
            }
        }
        catch (const std::exception& e) {
            preview.ready = false;
            preview.problems.push_back(e.what());
        }
    }
    else {
        prompts.lines.push_back(gppTr("TerminalPreview", "当前模式不使用 Api 和提示词").toStdString());
    }
    preview.pages.push_back(std::move(prompts));
    return preview;
}

NAMESPACE_END(gpp::cli)

NAMESPACE_BEGIN(gpp)

bool hasInteractiveTerminal()
{
#ifdef _WIN32
    DWORD inputMode, outputMode;
    return GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &inputMode) &&
           GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &outputMode);
#elif defined(__APPLE__)
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
#else
    return false;
#endif
}

void printProjectPreview(const fs::path& projectDir)
{
    const auto preview = cli::readPreview(projectDir);
    for (const auto& page : preview.pages) {
        std::cout << "\n=== " << page.title << " ===\n";
        for (const auto& line : page.lines) {
            std::cout << line << '\n';
        }
    }
    for (const auto& problem : preview.problems) {
        std::cerr << problem << '\n';
    }
    if (!preview.ready) {
        throw std::invalid_argument(gppTr("TerminalPreview", "配置尚不可运行，请修正后重试").toStdString());
    }
}

bool showProjectPreview(const fs::path& projectDir)
{
    using namespace ftxui;
    auto preview = cli::readPreview(projectDir);
    auto screen = ScreenInteractive::Fullscreen();
    bool start = false;
    bool importOpen = false;
    bool confirmStartOpen = false;
    std::string confirmStartMessage;
    int selected = 0;
    int scroll = 0;
    Element content;
    std::string status;
    std::string overviewPath = wide2Ascii(projectDir / ascii2Wide("ProblemOverview." + preview.overviewFormat));
    std::vector<std::string> titles;
    for (const auto& page : preview.pages) {
        titles.push_back(page.title);
    }
    auto menuOption = MenuOption::Horizontal();
    menuOption.on_change = [&]()
        {
            scroll = 0;
        };
    auto menu = Menu(&titles, &selected, menuOption);

    const auto startTranslation = [&]()
        {
            if (!preview.ready) {
                status = gppTr("TerminalPreview", "配置尚不可运行，请修正后重试").toStdString();
                return;
            }
            try {
                const auto config = uparseToml(projectDir / L"Config.toml");
                if (ProblemOverviewTracker::hasUnimportedChanges(projectDir, transCacheDirName, config)) {
                    confirmStartMessage =
                        gppTr("TerminalPreview", "检测到 %1 被修改但未导入。是否继续翻译？")
                            .arg(wide2Ascii(ProblemOverviewTracker::overviewPath(projectDir, config).filename()))
                            .toStdString();
                    confirmStartOpen = true;
                    return;
                }
                start = true;
                screen.Exit();
            }
            catch (const std::exception& e) {
                status = e.what();
            }
        };
    auto startButton = Button(gppTr("TerminalPreview", "开始翻译 [Enter]").toStdString(), startTranslation);
    auto importButton = Button(gppTr("TerminalPreview", "导入问题清单 [I]").toStdString(),
            [&]()
        {
            importOpen = true;
        });
    const auto reload = [&]()
        {
            try {
                preview = cli::readPreview(projectDir);
                scroll = 0;
                status = gppTr("TerminalPreview", "已重新读取配置").toStdString();
            }
            catch (const std::exception& e) {
                preview.ready = false;
                status = e.what();
            }
        };
    auto reloadButton = Button(gppTr("TerminalPreview", "重新读取 [R]").toStdString(), reload);
    auto backButton = Button(gppTr("TerminalPreview", "返回 [Esc]").toStdString(),
            [&]()
        {
            screen.Exit();
        });
    auto buttons = Container::Horizontal({ startButton, importButton, reloadButton, backButton });
    auto controls = Container::Vertical({ menu, buttons });
    auto main = Renderer(controls,
            [&]()
        {
            Elements lines;
            for (const auto& line : preview.pages[selected].lines) {
                // 按显式换行拆开，再让 paragraph 根据终端宽度折行。
                for (const auto part : splitStringView(line, '\n')) {
                    lines.push_back(paragraph(std::string(part)));
                }
            }
            Elements messages;
            for (const auto& problem : preview.problems) {
                messages.push_back(paragraph(problem) | color(Color::Yellow));
            }
            content = vbox(std::move(lines));
            return vbox({
                text("GalTransl++ CLI / " + wide2Ascii(projectDir.filename())) | bold,
                menu->Render(),
                separator(),
                content | focusPosition(0, scroll) | vscroll_indicator | yframe | flex,
                separator(),
                vbox(std::move(messages)) | size(HEIGHT, LESS_THAN, 4),
                paragraph(status),
                buttons->Render(),
                text(gppTr("TerminalPreview", "左右切换分页 / 上下、滚轮滚动 / 仅预览，不修改配置").toStdString()) | dim
            }) | border;
        });

    const auto importOverview = [&]()
        {
            try {
                fs::path path = ascii2Wide(overviewPath);
                const auto result = importProblemOverview(projectDir, path);
                ProblemOverviewTracker::recordLastWriteTime(projectDir, path);
                preview = cli::readPreview(projectDir);
                status = gppTr("ProblemOverview", "成功导入 %1 个句子至 transl_cache").arg(result.imported).toStdString();
                if (!result.problems.empty()) {
                    status +=
                        " / " + gppTr("TerminalPreview", "导入中出现的问题记录在 import_problems.log 中").toStdString();
                }
                importOpen = false;
            }
            catch (const std::exception& e) {
                status = e.what();
                importOpen = false;
            }
        };
    auto inputOption = InputOption::Default();
    inputOption.multiline = false;
    inputOption.on_enter = importOverview;
    auto pathInput = Input(&overviewPath, "ProblemOverview.json / .toml", inputOption);
    auto confirmImport = Button(gppTr("TerminalPreview", "导入").toStdString(), importOverview);
    auto cancelImport = Button(gppTr("TerminalPreview", "取消").toStdString(),
            [&]()
        {
            importOpen = false;
        });
    auto importControls = Container::Vertical({ pathInput, Container::Horizontal({ confirmImport, cancelImport }) });
    auto dialog = Renderer(importControls,
            [&]()
        {
            return vbox({
                text(gppTr("TerminalPreview", "导入问题清单").toStdString()) | bold,
                paragraph(gppTr("TerminalPreview", "按文件名和 index 替换整条缓存记录，确认后写入 transl_cache").toStdString()),
                separator(),
                pathInput->Render() | border,
                hbox({ confirmImport->Render(), cancelImport->Render() })
            }) | border | center;
        });
    auto cancelStart = Button(gppTr("TerminalPreview", "返回").toStdString(),
            [&]()
        {
            confirmStartOpen = false;
        });
    auto continueStart = Button(gppTr("TerminalPreview", "继续翻译").toStdString(),
            [&]()
        {
            start = true;
            screen.Exit();
        });
    auto confirmStartControls = Container::Horizontal({ cancelStart, continueStart });
    auto confirmStartDialog = Renderer(confirmStartControls,
            [&]()
        {
            return vbox({
                text(gppTr("TerminalPreview", "问题概览尚未导入").toStdString()) | bold,
                separator(),
                paragraph(confirmStartMessage),
                confirmStartControls->Render()
            }) | border | center;
        });
    auto mainWithImport = Modal(main, dialog, &importOpen);
    auto root = CatchEvent(Modal(mainWithImport, confirmStartDialog, &confirmStartOpen),
            [&](Event event)
        {
            if (confirmStartOpen) {
                if (event == Event::Escape) {
                    confirmStartOpen = false;
                    return true;
                }
                return false;
            }
            if (importOpen) {
                if (event == Event::Escape) {
                    importOpen = false;
                    return true;
                }
                return false;
            }
            if (event == Event::Return && menu->Focused()) {
                startTranslation();
                return true;
            }
            if (event == Event::Escape || event == Event::Character("q") || event == Event::Character("Q")) {
                screen.Exit();
                return true;
            }
            if (event == Event::Character("i") || event == Event::Character("I")) {
                importOpen = true;
                return true;
            }
            if (event == Event::Character("r") || event == Event::Character("R")) {
                reload();
                return true;
            }
            int scrollDelta = 0;
            if (event == Event::ArrowUp) {
                scrollDelta = -1;
            }
            else if (event == Event::ArrowDown) {
                scrollDelta = 1;
            }
            else if (event == Event::PageUp) {
                scrollDelta = -10;
            }
            else if (event == Event::PageDown) {
                scrollDelta = 10;
            }
            else if (event.is_mouse() && event.mouse().button == Mouse::WheelUp) {
                scrollDelta = -3;
            }
            else if (event.is_mouse() && event.mouse().button == Mouse::WheelDown) {
                scrollDelta = 3;
            }
            else {
                return false;
            }
            // reflect 得到的是裁剪后的可见区域；滚动上限应使用按终端宽度折行后的完整内容高度。
            const int lastLine = std::max(0, content->requirement().min_y - 1);
            scroll = std::clamp(scroll + scrollDelta, 0, lastLine);
            return true;
        });
    screen.Loop(root);
    return start;
}

NAMESPACE_END(gpp)
