#pragma once
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include <functional>

#include "../imgui/imgui.h"

struct MenuBar 
{
    using Action = std::function<void()>;

    MenuBar& addMenuItem(const std::string& InMenu, const std::string& InItem, Action InAction);

    void draw();
private:
    struct MenuItem 
	{
		std::string Menu, Item; 
		Action Action; 
	};
    std::vector<MenuItem> MenuItems;
};
