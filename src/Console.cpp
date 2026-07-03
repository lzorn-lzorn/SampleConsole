#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <functional>
#include <vector>
#include <regex>
#include <sstream>
#include <set>
#include <memory>
#include "Console.h"

#include "../imgui/imgui.h"

void Console::open(const char* path) 
{
	FilePath = path;
	Buffer.clear(); LastSize = 0; SelectedLines.clear();
	if (FileStream.is_open())
	{
		FileStream.close();
	}
	FileStream.open(path, std::ios::in | std::ios::binary);
	if (!FileStream.is_open()) 
	{
		std::ofstream create(path, std::ios::app);
		create.close();
		FileStream.open(path, std::ios::in | std::ios::binary);
	}
	if (FileStream) 
	{
		FileStream.seekg(0, std::ios::end);
		LastSize = FileStream.tellg();
	}
	strncpy(PathBuffer, FilePath.c_str(), sizeof(PathBuffer) - 1);
	PathBuffer[sizeof(PathBuffer) - 1] = '\0';
}


void Console::update() 
{
	if (!FileStream.is_open() || FileStream.fail()) 
	{ 
		tryReopen(); 
		return; 
	}
	FileStream.clear();
	FileStream.seekg(0, std::ios::end);
	if (!FileStream) 
	{ 
		tryReopen(); 
		return; 
	}
	std::streampos endPos = FileStream.tellg();
	if (endPos == std::streampos(-1)) 
	{
		tryReopen(); 
		return; 
	}
	size_t curSize = static_cast<size_t>(endPos);
	if (curSize < LastSize) 
	{
		Buffer.clear(); 
		SelectedLines.clear(); 
		LastSize = 0;
		FileStream.seekg(0, std::ios::beg);
	}
	if (curSize == LastSize)
	{
		return;
	}
	FileStream.seekg(LastSize, std::ios::beg);
	if (!FileStream) 
	{
		tryReopen(); 
		return; 
	}
	std::string chunk(curSize - LastSize, '\0');
	FileStream.read(&chunk[0], chunk.size());
	chunk.resize(FileStream.gcount());
	Buffer += chunk;
	LastSize += chunk.size();
	FileStream.clear();
}

void Console::draw(const char* title, bool* p_open, const std::vector<int>* highlightLines, int selectedHighlightLine, bool* outActivated, ImVec2* outWindowPos, ImVec2* outWindowSize)
{
	if (!ImGui::Begin(title, p_open)) 
	{  
		ImGui::End(); 
		return; 
	}

	if (outActivated 
		&& ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) 
		&& ImGui::IsMouseClicked(ImGuiMouseButton_Left)) 
	{
		*outActivated = true;
	}

	ImGui::Text("Log File Path:");
	ImGui::SameLine();
	float input_width = ImGui::GetContentRegionAvail().x - 85.0f;
	if (input_width < 100.0f)
	{
		input_width = 100.0f;
	}
	ImGui::PushItemWidth(input_width);
	if (ImGui::InputText(
			"##path", 
			PathBuffer, 
			sizeof(PathBuffer), 
			ImGuiInputTextFlags_EnterReturnsTrue
		))
	{
		open(PathBuffer);
	}
	ImGui::PopItemWidth();
	ImGui::SameLine();
	if (ImGui::Button("Browse", ImVec2(75, 0))) 
	{
		ShowFileDialog = true;
	}

	if (ImGui::BeginDragDropTarget()) 
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILES")) 
		{
			std::string dropped_paths((const char*)payload->Data, payload->DataSize);
			size_t end = dropped_paths.find('\n');
			if (end != std::string::npos) 
			{
				dropped_paths = dropped_paths.substr(0, end);
			}
			open(dropped_paths.c_str());
		}
		ImGui::EndDragDropTarget();
	}

	if (!FileStream.is_open())
	{
		ImGui::TextColored(ImVec4(1,0,0,1), "File not available, retrying...");
	}
	else
	{
		ImGui::TextColored(ImVec4(0,1,0,1), "Monitoring: %s", FilePath.c_str());
	}

	ImGui::Checkbox("Show line numbers", &ShowLineNumbers);
	ImGui::SameLine();
	ImGui::Checkbox("Auto-scroll", &AutoScroll);
	ImGui::SameLine();
	if (ImGui::Button("Clear")) 
	{ 
		Buffer.clear(); 
		SelectedLines.clear(); 
	}
	ImGui::Separator();

	std::set<int> highlight_set;
	if (highlightLines) 
	{
		highlight_set.insert(highlightLines->begin(), highlightLines->end());
	}

	std::vector<std::string> lines;
	std::istringstream stream(Buffer);
	std::string line;
	while (std::getline(stream, line)) 
	{
		if (ShowLineNumbers) 
		{
			lines.push_back(std::to_string(lines.size()) + ": " + line);
		}
		else 
		{
			lines.push_back(line);
		}
	}

	const float footer_height = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height), true, ImGuiWindowFlags_HorizontalScrollbar);

	if (pending_scroll_line_ >= 0) 
	{
		float lineHeight = ImGui::GetTextLineHeightWithSpacing();
		ImGui::SetScrollY(pending_scroll_line_ * lineHeight);
		pending_scroll_line_ = -1;
	}

	if (ImGui::IsWindowFocused()) 
	{
		auto& io = ImGui::GetIO();
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) 
		{
			std::string clipboard;
			for (int idx : SelectedLines)
			{
				if (idx < (int)lines.size()) 
				{
					clipboard += lines[idx] + "\n";
				}
			}
			if (!clipboard.empty())
			{
				ImGui::SetClipboardText(clipboard.c_str());
			}
		}
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_X)) 
		{
			std::string clipboard;
			for (int idx : SelectedLines)
			{
				if (idx < (int)lines.size())
				{
					clipboard += lines[idx] + "\n";
				}
			}
			if (!clipboard.empty())
			{
				ImGui::SetClipboardText(clipboard.c_str());
			}
		}
	}

	ImGuiListClipper clipper;
	clipper.Begin((int)lines.size());
	while (clipper.Step()) 
	{
		for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) 
		{
			bool isCur = (i == selectedHighlightLine);
			bool isHigh = highlight_set.find(i) != highlight_set.end();
			bool isSel = SelectedLines.find(i) != SelectedLines.end();

			if (isCur)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f,1,1,1));
			}
			else if (isHigh)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,0.2f,1));
			}
			else if (isSel)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f,0.8f,1,1));
			}
			else 
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
			}

			if (ImGui::Selectable(lines[i].c_str(), isSel)) 
			{
				if (ImGui::GetIO().KeyCtrl) 
				{
					if (isSel) 
					{
						SelectedLines.erase(i); 
					}
					else 
					{
						SelectedLines.insert(i);
					}
				} 
				else 
				{ 
					SelectedLines.clear(); 
					SelectedLines.insert(i); 
				}
			}
			ImGui::PopStyleColor();
		}
	}
	if (AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) 
	{
		ImGui::SetScrollHereY(1.0f);
	}
	ImGui::EndChild();

	if (outWindowPos)
	{
		*outWindowPos = ImGui::GetWindowPos();
	}
	if (outWindowSize)
	{
		*outWindowSize = ImGui::GetWindowSize();
	}

	ImGui::End();
}

void Console::tryReopen() 
{
	if (FileStream.is_open()) 
	{
		FileStream.close();
	}
	LastSize = 0;
	FileStream.open(FilePath, std::ios::in | std::ios::binary);
	if (!FileStream)
	{
		return;
	}
	Buffer.clear(); 
	SelectedLines.clear();
	FileStream.seekg(0, std::ios::end);
	if (FileStream) 
	{
		LastSize = static_cast<size_t>(FileStream.tellg());
	}
}
