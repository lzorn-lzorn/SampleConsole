#include "SearchPanel.h"

namespace 
{
	static bool doFuzzyMatch(const std::string& Line, const std::string& query) 
	{
		if (query.empty())
		{
			return false;
		}
		size_t j = 0;
		for (size_t i = 0; i < Line.size() && j < query.size(); ++i)
		{
			if (Line[i] == query[j])
			{
				++j;
			}
		}
		return j == query.size();
	}
}

void SearchPanel::open() {
	Visible       = true;
	UseRegex      = false;
	SelectedIndex = -1;
	RegexValid    = false;
	JustOpened    = true;
	LastBufferPtr = nullptr;

	LastSearchText.clear();
	CurrentConsoleLabel.clear();
	MatchLines.clear();
	memset(SearchText, 0, sizeof(SearchText));
}


void SearchPanel::search(const std::string& InBuffer, const void* InBufferId) 
{
	if (InBufferId != LastBufferPtr) 
	{
		LastBufferPtr = InBufferId;
		SelectedIndex = -1;
	}
	bool TextChanged = (SearchText != LastSearchText);
	LastSearchText = SearchText;
	if (TextChanged) {
		MatchLines.clear();
		SelectedIndex = -1;
	}
	RegexValid = false;
	if (SearchText[0] == '\0') {
		MatchLines.clear();
		SelectedIndex = -1;
		return;
	}
	MatchLines.clear();
	try {
		std::string query(SearchText);
		if (UseRegex) 
		{
			Pattern.assign(query);
			RegexValid = true;
		}
		std::istringstream Stream(InBuffer);
		std::string Line;
		int LineNum = 0;
		while (std::getline(Stream, Line)) 
		{
			bool IsMatched = false;
			if (UseRegex && RegexValid)
			{
				IsMatched = std::regex_search(Line, Pattern);
			} 
			else
			{
				IsMatched = doFuzzyMatch(Line, query);
			}
			if (IsMatched) 
			{
				MatchLines.push_back(LineNum);
			}
			++LineNum;
		}
	} catch (const std::regex_error&) {
		RegexValid = false;
		MatchLines.clear();
	}
	if (!TextChanged && SelectedIndex >= 0) 
	{
		if (SelectedIndex >= (int)MatchLines.size())
		{
			SelectedIndex = (int)MatchLines.size() - 1;
		}
	} else if (TextChanged) 
	{
		SelectedIndex = -1;
	}
}

void SearchPanel::jumpToNextMatch() {
	if (MatchLines.empty()) 
	{
		return;
	}
	if (SelectedIndex < 0 || SelectedIndex >= (int)MatchLines.size())
	{
		SelectedIndex = 0;
	}
	else
	{
		SelectedIndex = (SelectedIndex + 1) % MatchLines.size();
	}
	if (OnGoToLine)
	{
		OnGoToLine(MatchLines[SelectedIndex]);
	}
}

void SearchPanel::draw(const std::string& InBuffer, const void* InBufferId) {
	if (!Visible) 
	{
		return;
	}
	if (!ImGui::Begin("Search", &Visible)) 
	{ 
		ImGui::End(); 
		return; 
	}

	if (!CurrentConsoleLabel.empty()) 
	{
		ImGui::TextColored(
			ImVec4(0.7f, 0.9f, 1.0f, 1.0f), 
			"Searching in: %s", 
			CurrentConsoleLabel.c_str()
		);
		ImGui::Separator();
	}

	if (JustOpened) 
	{
		ImGui::SetKeyboardFocusHere();
		JustOpened = false;
	}

	ImGui::PushItemWidth(200);
	ImGui::InputText("##SearchText", SearchText, sizeof(SearchText));
	ImGui::PopItemWidth();
	ImGui::SameLine();
	ImGui::Checkbox("Regex", &UseRegex);
	ImGui::SameLine();

	if (ImGui::Button("Find")) 
	{
		search(InBuffer, InBufferId);
		jumpToNextMatch();
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) 
	{
		if (ImGui::IsKeyPressed(ImGuiKey_Enter) 
			|| ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) 
		{
			search(InBuffer, InBufferId);
			jumpToNextMatch();
		}
	}

	ImGui::TextColored(ImVec4(1,1,0.2f,1), "Yellow"); 
	ImGui::SameLine();
	ImGui::Text("= match, "); ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.2f,1,1,1), "Cyan"); 
	ImGui::SameLine();
	ImGui::Text("= current");
	ImGui::Text("Matches: %s",
		MatchLines.empty() 
		? "-" 
		:(std::to_string(SelectedIndex + 1) 
			+ "/" 
			+ std::to_string(MatchLines.size())).c_str()
	);
	ImGui::End();
}

