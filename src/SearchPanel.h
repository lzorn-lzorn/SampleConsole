
#pragma once
#include <string>
#include <vector>
#include <regex>
#include <functional>
#include <sstream>

#include "../imgui/imgui.h"

struct SearchPanel {
    bool Visible = false;
    char SearchText[256] = {};
    bool UseRegex = false;
    std::regex Pattern;
    bool RegexValid = false;
    std::function<void(int)> OnGoToLine;

    std::vector<int> MatchLines;
    int SelectedIndex = -1;
    bool JustOpened = false;
    std::string LastSearchText;
    const void* LastBufferPtr = nullptr;
    std::string CurrentConsoleLabel;

    void open();

    void close() { Visible = false; }

    void search(const std::string& InBuffer, const void* InBufferId);
    void jumpToNextMatch();
    void draw(const std::string& InBuffer, const void* InBufferId);
};
