#pragma once

#include <memory>
#include <string>
#include <vector>

struct GLFWwindow;

struct Console;
class XmlViewer;
class ShortcutManager;
struct SearchPanel;
struct MenuBar;

class App
{
public:
    static App& instance();

    int run();

private:
    struct AppConfig
    {
        struct ConsoleEntry
        {
            std::string LogFilePath;
            int X = -1;
            int Y = -1;
            int W = -1;
            int H = -1;
        };

        std::vector<ConsoleEntry> Consoles;
        int SearchX = -1;
        int SearchY = -1;
        int SearchW = -1;
        int SearchH = -1;
        int MainX = -1;
        int MainY = -1;
        int MainW = -1;
        int MainH = -1;
    };

    App() = default;
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    bool initialize();
    void shutdown();
    void setupGlfwHints();
    void initializeModules();
    void processDialogs();
    void drawXmlViewers();
    void drawConsoles();
    void drawSearchPanel();
    void updateMainWindowGeometry();
    void saveCurrentConfig();

    static AppConfig loadConfig();
    static void saveConfig(const AppConfig& InConfig);

    static void glfwErrorCallback(int InError, const char* InDescription);

private:
    GLFWwindow* Window = nullptr;
    const char* GlslVersion = "#version 130";

    bool XercesInitialized = false;
    bool ImGuiInitialized = false;

    AppConfig Config;
    std::vector<std::unique_ptr<Console>> Consoles;
    std::vector<std::unique_ptr<XmlViewer>> XmlViewers;
    int ConsoleIdCounter = 0;

    std::unique_ptr<ShortcutManager> Shortcuts;
    std::unique_ptr<SearchPanel> Search;
    std::unique_ptr<MenuBar> Menu;

    bool BrowseDialogOpen = false;
    Console* PendingBrowseConsole = nullptr;
    Console* LastActiveConsole = nullptr;
};
