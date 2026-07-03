
#include "ShortcutManager.h"

#include "../imgui/imgui.h"



void ShortcutManager::bind(ImGuiKeyChord InChord, std::function<void()> InCallback, const std::string& InDescription) 
{
	Shortcuts.push_back({
		InChord, 
		InDescription, 
		InCallback, 
		true
	});
}
void ShortcutManager::process() 
{
	for (const auto& Sc : Shortcuts)
	{
		if (Sc.Enable && ImGui::IsKeyChordPressed(Sc.Chord)) 
		{
			Sc.Callback();
		}
	}
}


