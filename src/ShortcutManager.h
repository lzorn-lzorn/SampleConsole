
#pragma once
#include <string>
#include <vector>
#include <functional>

#include "../imgui/imgui.h"

class ShortcutManager {
public:
    struct Shortcut 
	{ 
		ImGuiKeyChord Chord; 
		std::string Description; 
		std::function<void()> Callback; 
		bool Enable = true; 
	};

    void bind(ImGuiKeyChord InChord, std::function<void()> InCallback, const std::string& InDescription = "");
    void process();
    const std::vector<Shortcut>& getShortcuts() const { return Shortcuts; }
private:
    std::vector<Shortcut> Shortcuts;
};
