
#include "MenuBar.h"
#include "../imgui/imgui.h"


MenuBar& MenuBar::addMenuItem(const std::string& InMenu, const std::string& InItem, Action InAction) 
{
	MenuItems.push_back({InMenu, InItem, std::move(InAction)});
	return *this;
}

void MenuBar::draw() 
{
	if (!ImGui::BeginMainMenuBar())
	{
		return;
	}
	std::string CurrentMenu;
	bool IsMenuOpen = false;
	for (const auto& mi : MenuItems) 
	{
		if (mi.Menu != CurrentMenu)
		{
			if (IsMenuOpen) 
			{ 
				ImGui::EndMenu(); 
				IsMenuOpen = false; 
			}
			CurrentMenu = mi.Menu;
			if (ImGui::BeginMenu(CurrentMenu.c_str())) IsMenuOpen = true;
			else CurrentMenu.clear();
		}
		if (IsMenuOpen 
			&& ImGui::MenuItem(mi.Item.c_str())) 
		{
			mi.Action();
		}
	}
	if (IsMenuOpen)	
	{
		ImGui::EndMenu();
	}
	ImGui::EndMainMenuBar();
}

