#pragma once
#include <cstdio>
#include <fstream>
#include <string>
#include <set>
#include <vector>

// ==================== 控制台 ====================
struct Console {
    std::string   FilePath;
    std::ifstream FileStream;
    std::string   Buffer;
    size_t        LastSize = 0;
    bool          AutoScroll = true;
    char          PathBuffer[512] = {};
    bool          ShowLineNumbers = true;
    bool          ShowFileDialog = false;
    std::set<int> SelectedLines;
    int           ConsoleId = 0;

    Console() = default;
    Console(int id) : ConsoleId(id) {}

    const std::string& getBuffer() const { return Buffer; }
    const void* getBufferId() const { return &Buffer; }

    void open(const char* path);

    void requestScrollToLine(int lineNumber) { pending_scroll_line_ = lineNumber; }

    void update();

    void draw(const char* title, bool* p_open,
              const std::vector<int>* highlightLines = nullptr,
              int selectedHighlightLine = -1,
              bool* outActivated = nullptr) ;

private:
    int pending_scroll_line_ = -1;
    void tryReopen();
};