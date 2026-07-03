#include "App.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include <glad/glad.h>

#include "../imgui/imgui.h"
#include "../imgui/ImGuiFileDialog.h"
#include "../imgui/backends/imgui_impl_glfw.h"
#include "../imgui/backends/imgui_impl_opengl3.h"

#include "Console.h"
#include "MenuBar.h"
#include "SearchPanel.h"
#include "ShortcutManager.h"
#include "XmlViewer.h"

#include <GLFW/glfw3.h>

#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/util/XMLString.hpp>

using namespace xercesc;

namespace
{
    static std::string trim(const std::string& InValue)
    {
        const size_t Start = InValue.find_first_not_of(" \t\r\n");
        if (Start == std::string::npos)
        {
            return {};
        }

        const size_t End = InValue.find_last_not_of(" \t\r\n");
        return InValue.substr(Start, End - Start + 1);
    }
}

App& App::instance()
{
    static App Instance;
    return Instance;
}

App::~App() = default;

int App::run()
{
    if (!initialize())
    {
        shutdown();
        return 1;
    }

    while (!glfwWindowShouldClose(Window))
    {
        glfwPollEvents();
        for (auto& C : Consoles)
        {
            C->update();
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        Menu->draw();
        Shortcuts->process();

        processDialogs();
        drawXmlViewers();
        drawConsoles();
        drawSearchPanel();
        updateMainWindowGeometry();

        ImGui::Render();

        int DisplayW = 0;
        int DisplayH = 0;
        glfwGetFramebufferSize(Window, &DisplayW, &DisplayH);

        glViewport(0, 0, DisplayW, DisplayH);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(Window);
    }

    saveCurrentConfig();
    shutdown();
    return 0;
}

bool App::initialize()
{
    try
    {
        XMLPlatformUtils::Initialize();
        XercesInitialized = true;
    }
    catch (const XMLException& Ex)
    {
        char* Msg = XMLString::transcode(Ex.getMessage());
        fprintf(stderr, "Xerces init error: %s\n", Msg ? Msg : "unknown");
        XMLString::release(&Msg);
        return false;
    }

    glfwSetErrorCallback(glfwErrorCallback);
    if (!glfwInit())
    {
        return false;
    }

    setupGlfwHints();

    Config = loadConfig();

    int WinW = 800;
    int WinH = 600;
    if (Config.MainW > 0 && Config.MainH > 0)
    {
        WinW = Config.MainW;
        WinH = Config.MainH;
    }

    Window = glfwCreateWindow(WinW, WinH, "Sample Console", nullptr, nullptr);
    if (!Window)
    {
        return false;
    }

    glfwMakeContextCurrent(Window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        fprintf(stderr, "Failed to initialize GLAD\n");
        return false;
    }

    if (Config.MainX >= 0 && Config.MainY >= 0)
    {
        glfwSetWindowPos(Window, Config.MainX, Config.MainY);
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& Io = ImGui::GetIO();
    Io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    const char* FontPaths[] = {
        "NotoSansSC-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSansSC-Regular.ttf",
        "C:/Windows/Fonts/msyh.ttc",
        "/System/Library/Fonts/PingFang.ttc"
    };

    bool FontLoaded = false;
    for (const char* Path : FontPaths)
    {
        std::ifstream Test(Path);
        if (Test.good())
        {
            FontLoaded = (Io.Fonts->AddFontFromFileTTF(
                Path,
                18.0f,
                nullptr,
                Io.Fonts->GetGlyphRangesChineseSimplifiedCommon()) != nullptr);
            if (FontLoaded)
            {
                break;
            }
        }
    }

    if (!FontLoaded)
    {
        fprintf(stderr, "Warning: No Chinese font found\n");
        Io.Fonts->AddFontDefault();
    }

    ImGui_ImplGlfw_InitForOpenGL(Window, true);
    ImGui_ImplOpenGL3_Init(GlslVersion);
    ImGuiInitialized = true;

    initializeModules();
    return true;
}

void App::shutdown()
{
    if (ImGuiInitialized)
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        ImGuiInitialized = false;
    }

    if (Window)
    {
        glfwDestroyWindow(Window);
        Window = nullptr;
    }

    glfwTerminate();

    if (XercesInitialized)
    {
        XMLPlatformUtils::Terminate();
        XercesInitialized = false;
    }
}

void App::setupGlfwHints()
{
#if defined(IMGUI_IMPL_OPENGL_ES2)
    GlslVersion = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    GlslVersion = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    GlslVersion = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif
}

void App::initializeModules()
{
    if (Config.Consoles.empty())
    {
        Config.Consoles.push_back({"output.log", -1, -1, -1, -1});
    }

    for (auto& Entry : Config.Consoles)
    {
        auto C = std::make_unique<Console>(ConsoleIdCounter++);
        C->open(Entry.LogFilePath.c_str());

        if (Entry.W <= 0 || Entry.H <= 0)
        {
            Entry.W = 400;
            Entry.H = 300;
            Entry.X = 100;
            Entry.Y = 100;
        }

        Consoles.push_back(std::move(C));
    }

    LastActiveConsole = Consoles.empty() ? nullptr : Consoles[0].get();

    Shortcuts = std::make_unique<ShortcutManager>();
    Search = std::make_unique<SearchPanel>();
    Menu = std::make_unique<MenuBar>();

    Menu->addMenuItem("File", "New Console", [this]() {
        IGFD::FileDialogConfig Cfg;
        Cfg.path = ".";
        Cfg.fileName = "output.log";
        Cfg.countSelectionMax = 1;
        Cfg.flags = ImGuiFileDialogFlags_Modal;
        ImGuiFileDialog::Instance()->OpenDialog("NewConsoleFile", "Choose Log File", ".log,.txt", Cfg);
    })
    .addMenuItem("File", "Save Config", [this]() {
        saveCurrentConfig();
    })
    .addMenuItem("File", "Exit", [this]() {
        glfwSetWindowShouldClose(Window, true);
    })
    .addMenuItem("View", "Toggle Search Panel", [this]() {
        Search->Visible = !Search->Visible;
        if (Search->Visible)
        {
            Search->open();
        }
    })
    .addMenuItem("View", "XML Viewer", []() {
        IGFD::FileDialogConfig Cfg;
        Cfg.path = ".";
        Cfg.countSelectionMax = 1;
        Cfg.flags = ImGuiFileDialogFlags_Modal;
        ImGuiFileDialog::Instance()->OpenDialog("XmlViewerFileDialog", "Choose XML File", ".xml", Cfg);
    });

    Shortcuts->bind(ImGuiMod_Ctrl | ImGuiKey_F, [this]() {
        Search->Visible = !Search->Visible;
        if (Search->Visible)
        {
            Search->open();
        }
    }, "Toggle search");
}

void App::processDialogs()
{
    if (ImGuiFileDialog::Instance()->Display("NewConsoleFile"))
    {
        if (ImGuiFileDialog::Instance()->IsOk())
        {
            const std::string Path = ImGuiFileDialog::Instance()->GetFilePathName();
            auto NewConsole = std::make_unique<Console>(ConsoleIdCounter++);
            NewConsole->open(Path.c_str());
            Consoles.push_back(std::move(NewConsole));
            LastActiveConsole = Consoles.back().get();
        }

        ImGuiFileDialog::Instance()->Close();
    }

    if (ImGuiFileDialog::Instance()->Display("XmlViewerFileDialog"))
    {
        if (ImGuiFileDialog::Instance()->IsOk())
        {
            const std::string Path = ImGuiFileDialog::Instance()->GetFilePathName();
            auto Viewer = std::make_unique<XmlViewer>();
            if (Viewer->loadFromFile(Path))
            {
                XmlViewers.push_back(std::move(Viewer));
            }
        }

        ImGuiFileDialog::Instance()->Close();
    }

    if (!BrowseDialogOpen)
    {
        PendingBrowseConsole = nullptr;
        for (auto& C : Consoles)
        {
            if (C->ShowFileDialog)
            {
                PendingBrowseConsole = C.get();
                break;
            }
        }

        if (PendingBrowseConsole)
        {
            IGFD::FileDialogConfig Cfg;
            Cfg.path = ".";
            Cfg.fileName = "output.log";
            Cfg.countSelectionMax = 1;
            Cfg.flags = ImGuiFileDialogFlags_Modal;

            ImGuiFileDialog::Instance()->OpenDialog("BrowseLogFile", "Choose Log File", ".log,.txt", Cfg);
            PendingBrowseConsole->ShowFileDialog = false;
            BrowseDialogOpen = true;
        }
    }

    if (ImGuiFileDialog::Instance()->Display("BrowseLogFile"))
    {
        if (ImGuiFileDialog::Instance()->IsOk())
        {
            const std::string Path = ImGuiFileDialog::Instance()->GetFilePathName();
            if (PendingBrowseConsole)
            {
                PendingBrowseConsole->open(Path.c_str());
                LastActiveConsole = PendingBrowseConsole;
            }
        }

        ImGuiFileDialog::Instance()->Close();
        BrowseDialogOpen = false;
        PendingBrowseConsole = nullptr;
    }
}

void App::drawXmlViewers()
{
    for (size_t i = 0; i < XmlViewers.size(); ++i)
    {
        XmlViewers[i]->draw();
        if (!XmlViewers[i]->isOpen())
        {
            XmlViewers.erase(XmlViewers.begin() + i);
            --i;
        }
    }
}

void App::drawConsoles()
{
    std::vector<AppConfig::ConsoleEntry> NewEntries;
    NewEntries.reserve(Consoles.size());

    for (size_t i = 0; i < Consoles.size(); ++i)
    {
        auto& C = Consoles[i];

        std::string Title = "Console " + std::to_string(i + 1);
        if (!C->FilePath.empty())
        {
            Title += " - " + C->FilePath;
        }

        AppConfig::ConsoleEntry Entry;
        Entry.LogFilePath = C->FilePath;

        if (i < Config.Consoles.size())
        {
            ImGui::SetNextWindowPos(ImVec2((float)Config.Consoles[i].X, (float)Config.Consoles[i].Y), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2((float)Config.Consoles[i].W, (float)Config.Consoles[i].H), ImGuiCond_FirstUseEver);
        }

        bool Open = true;
        bool Activated = false;

        const bool IsActive = (C.get() == LastActiveConsole) && Search->Visible;
        const std::vector<int>* HighlightLines = IsActive ? &Search->MatchLines : nullptr;

        int HighlightLine = -1;
        if (IsActive
            && Search->SelectedIndex >= 0
            && Search->SelectedIndex < (int)Search->MatchLines.size())
        {
            HighlightLine = Search->MatchLines[Search->SelectedIndex];
        }

        ImVec2 WindowPos(0.0f, 0.0f);
        ImVec2 WindowSize(0.0f, 0.0f);

        C->draw(
            Title.c_str(),
            &Open,
            HighlightLines,
            HighlightLine,
            &Activated,
            &WindowPos,
            &WindowSize);

        Entry.X = (int)WindowPos.x;
        Entry.Y = (int)WindowPos.y;
        Entry.W = (int)WindowSize.x;
        Entry.H = (int)WindowSize.y;
        NewEntries.push_back(Entry);

        if (!Open)
        {
            if (LastActiveConsole == C.get())
            {
                LastActiveConsole = (Consoles.size() > 1) ? Consoles[0].get() : nullptr;
            }

            Consoles.erase(Consoles.begin() + i);
            --i;
            continue;
        }

        if (Activated)
        {
            LastActiveConsole = C.get();
        }
    }

    Config.Consoles = std::move(NewEntries);
}

void App::drawSearchPanel()
{
    if (LastActiveConsole)
    {
        std::string ConsoleName = "Console " + std::to_string(LastActiveConsole->ConsoleId + 1);
        if (!LastActiveConsole->FilePath.empty())
        {
            ConsoleName += " - " + LastActiveConsole->FilePath;
        }
        Search->CurrentConsoleLabel = ConsoleName;
    }
    else
    {
        Search->CurrentConsoleLabel.clear();
    }

    static const std::string EmptyBuffer;
    const std::string* ActiveBuffer = &EmptyBuffer;
    const void* ActiveBufferId = nullptr;

    if (LastActiveConsole)
    {
        ActiveBuffer = &LastActiveConsole->getBuffer();
        ActiveBufferId = LastActiveConsole->getBufferId();
    }

    if (Search->Visible && !ActiveBuffer->empty())
    {
        Search->search(*ActiveBuffer, ActiveBufferId);
    }

    Search->OnGoToLine = [this](int InLine) {
        if (LastActiveConsole)
        {
            LastActiveConsole->requestScrollToLine(InLine);
            LastActiveConsole->AutoScroll = false;
        }
    };

    ImGui::SetNextWindowPos(ImVec2((float)Config.SearchX, (float)Config.SearchY), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2((float)Config.SearchW, (float)Config.SearchH), ImGuiCond_FirstUseEver);

    Search->draw(*ActiveBuffer, ActiveBufferId);

    if (Search->Visible)
    {
        const ImVec2 Pos = ImGui::GetWindowPos();
        const ImVec2 Size = ImGui::GetWindowSize();
        Config.SearchX = (int)Pos.x;
        Config.SearchY = (int)Pos.y;
        Config.SearchW = (int)Size.x;
        Config.SearchH = (int)Size.y;
    }
}

void App::updateMainWindowGeometry()
{
    glfwGetWindowPos(Window, &Config.MainX, &Config.MainY);
    glfwGetWindowSize(Window, &Config.MainW, &Config.MainH);
}

void App::saveCurrentConfig()
{
    for (size_t i = 0; i < Consoles.size() && i < Config.Consoles.size(); ++i)
    {
        Config.Consoles[i].LogFilePath = Consoles[i]->FilePath;
    }

    saveConfig(Config);
}

App::AppConfig App::loadConfig()
{
    AppConfig OutConfig;

    std::ifstream File("console_config.ini");
    if (!File)
    {
        return OutConfig;
    }

    std::string Line;
    int Count = 0;

    if (std::getline(File, Line))
    {
        std::istringstream Iss(Line);
        if (!(Iss >> Count))
        {
            Count = 0;
        }
    }

    for (int i = 0; i < Count; ++i)
    {
        AppConfig::ConsoleEntry Entry;

        if (!std::getline(File, Line))
        {
            break;
        }
        Entry.LogFilePath = trim(Line);

        if (!std::getline(File, Line))
        {
            break;
        }

        std::istringstream Iss(Line);
        Iss >> Entry.X >> Entry.Y >> Entry.W >> Entry.H;
        OutConfig.Consoles.push_back(Entry);
    }

    if (std::getline(File, Line))
    {
        std::istringstream Iss(Line);
        Iss >> OutConfig.SearchX >> OutConfig.SearchY >> OutConfig.SearchW >> OutConfig.SearchH;
    }

    if (std::getline(File, Line))
    {
        std::istringstream Iss(Line);
        Iss >> OutConfig.MainX >> OutConfig.MainY >> OutConfig.MainW >> OutConfig.MainH;
    }

    return OutConfig;
}

void App::saveConfig(const AppConfig& InConfig)
{
    std::ofstream File("console_config.ini", std::ios::trunc);
    if (!File)
    {
        return;
    }

    File << InConfig.Consoles.size() << "\n";
    for (const auto& C : InConfig.Consoles)
    {
        File << C.LogFilePath << "\n";
        File << C.X << " " << C.Y << " " << C.W << " " << C.H << "\n";
    }

    File << InConfig.SearchX << " " << InConfig.SearchY << " " << InConfig.SearchW << " " << InConfig.SearchH << "\n";
    File << InConfig.MainX << " " << InConfig.MainY << " " << InConfig.MainW << " " << InConfig.MainH << "\n";
}

void App::glfwErrorCallback(int InError, const char* InDescription)
{
    fprintf(stderr, "GLFW Error %d: %s\n", InError, InDescription ? InDescription : "unknown");
}
