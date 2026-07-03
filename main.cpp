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

#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "imgui/ImGuiFileDialog.h"
#include <GLFW/glfw3.h>

#include <xercesc/dom/DOM.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMAttr.hpp>
#include <xercesc/dom/DOMText.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/framework/LocalFileFormatTarget.hpp>

// ==================== 程序配置 ====================
struct AppConfig {
    struct ConsoleEntry {
        std::string logFilePath;
        int x, y, w, h;
    };
    std::vector<ConsoleEntry> consoles;
    int searchX = -1, searchY = -1, searchW = -1, searchH = -1;
    // 新增：主窗口几何
    int mainX = -1, mainY = -1, mainW = -1, mainH = -1;
};

static AppConfig LoadConfig() {
    AppConfig cfg;
    std::ifstream file("console_config.ini");
    if (!file) return cfg;
    std::string line;
    int count = 0;
    if (std::getline(file, line)) {
        std::istringstream iss(line);
        if (!(iss >> count)) count = 0;
    }
    for (int i = 0; i < count; ++i) {
        AppConfig::ConsoleEntry entry;
        if (!std::getline(file, line)) break;
        size_t s = line.find_first_not_of(" \t\r\n");
        size_t e = line.find_last_not_of(" \t\r\n");
        if (s != std::string::npos)
            entry.logFilePath = line.substr(s, e - s + 1);
        else
            entry.logFilePath.clear();
        if (!std::getline(file, line)) break;
        std::istringstream iss(line);
        iss >> entry.x >> entry.y >> entry.w >> entry.h;
        cfg.consoles.push_back(entry);
    }
    if (std::getline(file, line)) {
        std::istringstream iss(line);
        iss >> cfg.searchX >> cfg.searchY >> cfg.searchW >> cfg.searchH;
    }
    if (std::getline(file, line)) {
        std::istringstream iss(line);
        iss >> cfg.mainX >> cfg.mainY >> cfg.mainW >> cfg.mainH;
    }
    return cfg;
}

static void SaveConfig(const AppConfig& cfg) {
    std::ofstream file("console_config.ini", std::ios::trunc);
    if (!file) return;
    file << cfg.consoles.size() << "\n";
    for (const auto& c : cfg.consoles) {
        file << c.logFilePath << "\n";
        file << c.x << " " << c.y << " " << c.w << " " << c.h << "\n";
    }
    file << cfg.searchX << " " << cfg.searchY << " " << cfg.searchW << " " << cfg.searchH << "\n";
    // 新增：保存主窗口几何
    file << cfg.mainX << " " << cfg.mainY << " " << cfg.mainW << " " << cfg.mainH << "\n";
}

// ==================== 搜索面板 ====================
struct SearchPanel {
    bool visible = false;
    char searchText[256] = {};
    bool useRegex = false;
    std::regex pattern;
    bool regexValid = false;
    std::function<void(int)> onGoToLine;

    std::vector<int> matchLines;
    int selectedIndex = -1;
    bool justOpened = false;
    std::string lastSearchText;
    const void* lastBufferPtr = nullptr;
    std::string currentConsoleLabel;

    void Open() {
        visible = true;
        memset(searchText, 0, sizeof(searchText));
        useRegex = false;
        matchLines.clear();
        selectedIndex = -1;
        regexValid = false;
        justOpened = true;
        lastSearchText.clear();
        lastBufferPtr = nullptr;
        currentConsoleLabel.clear();
    }

    void Close() { visible = false; }

    static bool FuzzyMatch(const std::string& line, const std::string& query) {
        if (query.empty()) return false;
        size_t j = 0;
        for (size_t i = 0; i < line.size() && j < query.size(); ++i)
            if (line[i] == query[j]) ++j;
        return j == query.size();
    }

    void Search(const std::string& buffer, const void* bufferId) {
        if (bufferId != lastBufferPtr) {
            lastBufferPtr = bufferId;
            selectedIndex = -1;
        }
        bool textChanged = (searchText != lastSearchText);
        lastSearchText = searchText;
        if (textChanged) {
            matchLines.clear();
            selectedIndex = -1;
        }
        regexValid = false;
        if (searchText[0] == '\0') {
            matchLines.clear();
            selectedIndex = -1;
            return;
        }
        matchLines.clear();
        try {
            std::string query(searchText);
            if (useRegex) {
                pattern.assign(query);
                regexValid = true;
            }
            std::istringstream stream(buffer);
            std::string line;
            int lineNum = 0;
            while (std::getline(stream, line)) {
                bool matched = false;
                if (useRegex && regexValid)
                    matched = std::regex_search(line, pattern);
                else
                    matched = FuzzyMatch(line, query);
                if (matched) matchLines.push_back(lineNum);
                ++lineNum;
            }
        } catch (const std::regex_error&) {
            regexValid = false;
            matchLines.clear();
        }
        if (!textChanged && selectedIndex >= 0) {
            if (selectedIndex >= (int)matchLines.size())
                selectedIndex = (int)matchLines.size() - 1;
        } else if (textChanged) {
            selectedIndex = -1;
        }
    }

    void JumpToNextMatch() {
        if (matchLines.empty()) return;
        if (selectedIndex < 0 || selectedIndex >= (int)matchLines.size())
            selectedIndex = 0;
        else
            selectedIndex = (selectedIndex + 1) % matchLines.size();
        if (onGoToLine) onGoToLine(matchLines[selectedIndex]);
    }

    void Draw(const std::string& buffer, const void* bufferId) {
        if (!visible) return;
        if (!ImGui::Begin("Search", &visible)) { ImGui::End(); return; }

        if (!currentConsoleLabel.empty()) {
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "Searching in: %s", currentConsoleLabel.c_str());
            ImGui::Separator();
        }

        if (justOpened) {
            ImGui::SetKeyboardFocusHere();
            justOpened = false;
        }

        ImGui::PushItemWidth(200);
        ImGui::InputText("##searchText", searchText, sizeof(searchText));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::Checkbox("Regex", &useRegex);
        ImGui::SameLine();
        if (ImGui::Button("Find")) {
            Search(buffer, bufferId);
            JumpToNextMatch();
        }

        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
            if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)) {
                Search(buffer, bufferId);
                JumpToNextMatch();
            }
        }

        ImGui::TextColored(ImVec4(1,1,0.2f,1), "Yellow"); ImGui::SameLine();
        ImGui::Text("= match, "); ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.2f,1,1,1), "Cyan"); ImGui::SameLine();
        ImGui::Text("= current");
        ImGui::Text("Matches: %s",
                    matchLines.empty() ? "-" :
                    (std::to_string(selectedIndex + 1) + "/" + std::to_string(matchLines.size())).c_str());
        ImGui::End();
    }
};

// ==================== 菜单界面 ====================
struct MenuBar {
    using Action = std::function<void()>;
    MenuBar& AddMenuItem(const std::string& menu, const std::string& item, Action action) {
        menuItems.push_back({menu, item, std::move(action)});
        return *this;
    }
    void Draw() {
        if (!ImGui::BeginMainMenuBar()) return;
        std::string currentMenu;
        bool isMenuOpen = false;
        for (const auto& mi : menuItems) {
            if (mi.menu != currentMenu) {
                if (isMenuOpen) { ImGui::EndMenu(); isMenuOpen = false; }
                currentMenu = mi.menu;
                if (ImGui::BeginMenu(currentMenu.c_str())) isMenuOpen = true;
                else currentMenu.clear();
            }
            if (isMenuOpen && ImGui::MenuItem(mi.item.c_str())) mi.action();
        }
        if (isMenuOpen) ImGui::EndMenu();
        ImGui::EndMainMenuBar();
    }
private:
    struct MenuItem { std::string menu, item; Action action; };
    std::vector<MenuItem> menuItems;
};

// ==================== 快捷键管理器 ====================
class ShortcutManager {
public:
    struct Shortcut { ImGuiKeyChord chord; std::string description; std::function<void()> callback; bool enabled = true; };
    void Register(ImGuiKeyChord chord, std::function<void()> callback, const std::string& description = "") {
        shortcuts.push_back({chord, description, callback, true});
    }
    void Process() {
        for (const auto& sc : shortcuts)
            if (sc.enabled && ImGui::IsKeyChordPressed(sc.chord)) sc.callback();
    }
    const std::vector<Shortcut>& GetAll() const { return shortcuts; }
private:
    std::vector<Shortcut> shortcuts;
};

// ==================== 控制台 ====================
struct Console {
    std::string   filePath;
    std::ifstream fileStream;
    std::string   buffer;
    size_t        lastSize = 0;
    bool          autoScroll = true;
    char          pathBuffer[512] = {};
    bool          showLineNumbers = true;
    bool          showFileDialog = false;
    std::set<int> selectedLines;
    int           consoleId = 0;

    Console() = default;
    Console(int id) : consoleId(id) {}

    const std::string& GetBuffer() const { return buffer; }
    const void* GetBufferId() const { return &buffer; }

    void Open(const char* path) {
        filePath = path;
        buffer.clear(); lastSize = 0; selectedLines.clear();
        if (fileStream.is_open()) fileStream.close();
        fileStream.open(path, std::ios::in | std::ios::binary);
        if (!fileStream.is_open()) {
            std::ofstream create(path, std::ios::app);
            create.close();
            fileStream.open(path, std::ios::in | std::ios::binary);
        }
        if (fileStream) {
            fileStream.seekg(0, std::ios::end);
            lastSize = fileStream.tellg();
        }
        strncpy(pathBuffer, filePath.c_str(), sizeof(pathBuffer) - 1);
        pathBuffer[sizeof(pathBuffer) - 1] = '\0';
    }

    void RequestScrollToLine(int lineNumber) { pendingScrollLine = lineNumber; }

    void Update() {
        if (!fileStream.is_open() || fileStream.fail()) { TryReopen(); return; }
        fileStream.clear();
        fileStream.seekg(0, std::ios::end);
        if (!fileStream) { TryReopen(); return; }
        std::streampos endPos = fileStream.tellg();
        if (endPos == std::streampos(-1)) { TryReopen(); return; }
        size_t curSize = static_cast<size_t>(endPos);
        if (curSize < lastSize) {
            buffer.clear(); selectedLines.clear(); lastSize = 0;
            fileStream.seekg(0, std::ios::beg);
        }
        if (curSize == lastSize) return;
        fileStream.seekg(lastSize, std::ios::beg);
        if (!fileStream) { TryReopen(); return; }
        std::string chunk(curSize - lastSize, '\0');
        fileStream.read(&chunk[0], chunk.size());
        chunk.resize(fileStream.gcount());
        buffer += chunk;
        lastSize += chunk.size();
        fileStream.clear();
    }

    void Draw(const char* title, bool* p_open,
              const std::vector<int>* highlightLines = nullptr,
              int selectedHighlightLine = -1,
              bool* outActivated = nullptr) {
        if (!ImGui::Begin(title, p_open)) { ImGui::End(); return; }

        if (outActivated && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            *outActivated = true;
        }

        ImGui::Text("Log File Path:");
        ImGui::SameLine();
        float inputWidth = ImGui::GetContentRegionAvail().x - 85.0f;
        if (inputWidth < 100.0f) inputWidth = 100.0f;
        ImGui::PushItemWidth(inputWidth);
        if (ImGui::InputText("##path", pathBuffer, sizeof(pathBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
            Open(pathBuffer);
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Browse", ImVec2(75, 0))) showFileDialog = true;

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILES")) {
                std::string droppedPaths((const char*)payload->Data, payload->DataSize);
                size_t end = droppedPaths.find('\n');
                if (end != std::string::npos) droppedPaths = droppedPaths.substr(0, end);
                Open(droppedPaths.c_str());
            }
            ImGui::EndDragDropTarget();
        }

        if (!fileStream.is_open())
            ImGui::TextColored(ImVec4(1,0,0,1), "File not available, retrying...");
        else
            ImGui::TextColored(ImVec4(0,1,0,1), "Monitoring: %s", filePath.c_str());

        ImGui::Checkbox("Show line numbers", &showLineNumbers);
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &autoScroll);
        ImGui::SameLine();
        if (ImGui::Button("Clear")) { buffer.clear(); selectedLines.clear(); }
        ImGui::Separator();

        std::set<int> highlightSet;
        if (highlightLines) highlightSet.insert(highlightLines->begin(), highlightLines->end());

        std::vector<std::string> lines;
        std::istringstream stream(buffer);
        std::string line;
        while (std::getline(stream, line)) {
            if (showLineNumbers) lines.push_back(std::to_string(lines.size()) + ": " + line);
            else lines.push_back(line);
        }

        const float footerHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
        ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footerHeight), true, ImGuiWindowFlags_HorizontalScrollbar);

        if (pendingScrollLine >= 0) {
            float lineHeight = ImGui::GetTextLineHeightWithSpacing();
            ImGui::SetScrollY(pendingScrollLine * lineHeight);
            pendingScrollLine = -1;
        }

        if (ImGui::IsWindowFocused()) {
            auto& io = ImGui::GetIO();
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
                std::string clipboard;
                for (int idx : selectedLines) if (idx < (int)lines.size()) clipboard += lines[idx] + "\n";
                if (!clipboard.empty()) ImGui::SetClipboardText(clipboard.c_str());
            }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_X)) {
                std::string clipboard;
                for (int idx : selectedLines) if (idx < (int)lines.size()) clipboard += lines[idx] + "\n";
                if (!clipboard.empty()) ImGui::SetClipboardText(clipboard.c_str());
            }
        }

        ImGuiListClipper clipper;
        clipper.Begin((int)lines.size());
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                bool isCur = (i == selectedHighlightLine);
                bool isHigh = highlightSet.find(i) != highlightSet.end();
                bool isSel = selectedLines.find(i) != selectedLines.end();

                if (isCur) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f,1,1,1));
                else if (isHigh) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,0.2f,1));
                else if (isSel) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f,0.8f,1,1));
                else ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));

                if (ImGui::Selectable(lines[i].c_str(), isSel)) {
                    if (ImGui::GetIO().KeyCtrl) {
                        if (isSel) selectedLines.erase(i); else selectedLines.insert(i);
                    } else { selectedLines.clear(); selectedLines.insert(i); }
                }
                ImGui::PopStyleColor();
            }
        }
        if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();
        ImGui::End();
    }

private:
    int pendingScrollLine = -1;
    void TryReopen() {
        if (fileStream.is_open()) fileStream.close();
        lastSize = 0;
        fileStream.open(filePath, std::ios::in | std::ios::binary);
        if (!fileStream) return;
        buffer.clear(); selectedLines.clear();
        fileStream.seekg(0, std::ios::end);
        if (fileStream) lastSize = static_cast<size_t>(fileStream.tellg());
    }
};

// ==================== XML 界面 ====================
using namespace xercesc;
class XMLViewer {
public:
    XMLViewer() = default;
    ~XMLViewer() { CloseDocument(); }

    bool LoadFromFile(const std::string& filePath) {
        CloseDocument();
        try {
            XercesDOMParser parser;
            parser.parse(filePath.c_str());
            m_doc = parser.adoptDocument();
            m_selectedNode = m_doc->getDocumentElement();
            m_currentFilePath = filePath;
            m_visible = true;   // 加载成功后自动显示
            return true;
        } catch (const XMLException& e) {
            char* msg = XMLString::transcode(e.getMessage());
            fprintf(stderr, "XML Error: %s\n", msg);
            XMLString::release(&msg);
            return false;
        } catch (...) {
            fprintf(stderr, "Unknown error loading XML\n");
            return false;
        }
    }

    bool SaveToFile(const std::string& filePath) {
        if (!m_doc || filePath.empty()) return false;
        // 使用 DOMImplementation 来序列化
        DOMImplementation* impl = DOMImplementationRegistry::getDOMImplementation(XMLString::transcode("LS"));
        if (!impl) return false;
        DOMLSSerializer* serializer = ((DOMImplementationLS*)impl)->createLSSerializer();
        if (serializer->getDomConfig()->canSetParameter(XMLUni::fgDOMWRTFormatPrettyPrint, true))
            serializer->getDomConfig()->setParameter(XMLUni::fgDOMWRTFormatPrettyPrint, true);
        XMLFormatTarget* target = new LocalFileFormatTarget(filePath.c_str());
        DOMLSOutput* output = ((DOMImplementationLS*)impl)->createLSOutput();
        output->setByteStream(target);
        serializer->write(m_doc, output);
        delete output;
        delete target;
        serializer->release();
        return true;
    }


    void Draw() {
        if (!m_visible) return;
        
        ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("XML Viewer", &m_visible)) {
            ImGui::End();
            return;
        }
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)) {
            if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S)) {
                if (!m_currentFilePath.empty()) {
                    SaveToFile(m_currentFilePath);
                }
            }
        }

        if (!m_doc) {
            ImGui::Text("No XML file loaded.");
            if (ImGui::Button("Load XML...")) {
                m_showLoadDialog = true;
            }
            ImGui::End();
            return;
        }

        // 菜单栏
        if (ImGui::BeginMenuBar()) {
            if (ImGui::MenuItem("Load...")) {
                m_showLoadDialog = true;
            }
            if (ImGui::MenuItem("Save")) {
                SaveToFile(m_currentFilePath);
            }
            ImGui::EndMenuBar();
        }

        // 左右分栏
        ImGui::BeginChild("Tree", ImVec2(250, -1), true);
        DrawNode(m_doc->getDocumentElement(), 0);
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("Edit", ImVec2(0, -1), true);
        if (m_selectedNode) DrawEditPanel(m_selectedNode);
        else ImGui::Text("Select a node to edit.");
        ImGui::EndChild();

        ImGui::End();

        // 内部的文件加载对话框（用于窗口内的 Load... 按钮）
        if (m_showLoadDialog) {
            IGFD::FileDialogConfig cfg;
            cfg.path = ".";
            cfg.countSelectionMax = 1;
            cfg.flags = ImGuiFileDialogFlags_Modal;
            ImGuiFileDialog::Instance()->OpenDialog("XmlViewerInternalFile", "Choose XML File", ".xml", cfg);
            m_showLoadDialog = false;
        }
        if (ImGuiFileDialog::Instance()->Display("XmlViewerInternalFile")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string path = ImGuiFileDialog::Instance()->GetFilePathName();
                if (LoadFromFile(path)) {
                    m_currentFilePath = path;
                }
            }
            ImGuiFileDialog::Instance()->Close();
        }
    }
    bool IsVisible() const { return m_visible; }
    bool IsOpen() const { return m_visible; }
    void ToggleVisible() { m_visible = !m_visible; }

private:
    DOMDocument* m_doc = nullptr;
    DOMNode* m_selectedNode = nullptr;
    bool m_visible = false;
    bool m_showLoadDialog = false;
    std::string m_currentFilePath;

    // 编辑缓冲区
    std::string m_nodeNameBuf;
    std::string m_textBuf;
    std::map<std::string, std::string> m_attrBuf;   // 属性名 -> 属性值

    // 用于暂存新节点的属性
    std::string m_newAttrName;
    std::string m_newAttrValue;
    std::map<std::string, std::string> m_newAttributes;   // 待添加的属性集

    void ClearNewAttributes() {
        m_newAttrName.clear();
        m_newAttrValue.clear();
        m_newAttributes.clear();
    }

    void CloseDocument() {
        if (m_doc) {
            m_doc->release();
            m_doc = nullptr;
            m_selectedNode = nullptr;
        }
    }

    // 递归绘制树节点
    void DrawNode(DOMNode* node, int id) {
        if (!node) return;
        if (node->getNodeType() == DOMNode::ELEMENT_NODE) {
            char* name = XMLString::transcode(node->getNodeName());
            bool isSelected = (node == m_selectedNode);
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow;
            if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

            bool opened = ImGui::TreeNodeEx((void*)(intptr_t)id, flags, "%s", name);
            if (ImGui::IsItemClicked()) {
                m_selectedNode = node;
                // 更新缓冲区
                m_nodeNameBuf = name;
                m_textBuf = GetTextContent(node);
                UpdateAttrBuf(node);
            }

            XMLString::release(&name);

            if (opened) {
                DOMNode* child = node->getFirstChild();
                int counter = 0;
                while (child) {
                    DrawNode(child, counter++);
                    child = child->getNextSibling();
                }
                ImGui::TreePop();
            }
        } else if (node->getNodeType() == DOMNode::TEXT_NODE) {
            char* text = XMLString::transcode(node->getNodeValue());
            // 忽略仅包含空白的文本节点
            std::string s(text);
            XMLString::release(&text);
            bool onlyWhitespace = true;
            for (char c : s) if (!std::isspace((unsigned char)c)) { onlyWhitespace = false; break; }
            if (!onlyWhitespace) {
                ImGui::BulletText("\"%s\"", s.c_str());
            }
        }
    }

    // 编辑面板
    void DrawEditPanel(DOMNode* node) {
        if (!node) return;
        ImGui::Text("Node Name:");
        ImGui::SameLine();
        if (ImGui::InputText("##nodename", &m_nodeNameBuf)) {
            // 修改节点名称
            if (!m_nodeNameBuf.empty()) {
                DOMElement* elem = dynamic_cast<DOMElement*>(node);
                if (elem) {
                    DOMDocument* doc = node->getOwnerDocument();
                    DOMElement* newElem = doc->createElement(XMLString::transcode(m_nodeNameBuf.c_str()));
                    // 复制属性
                    DOMNamedNodeMap* attrs = elem->getAttributes();
                    if (attrs) {
                        for (XMLSize_t i = 0; i < attrs->getLength(); ++i) {
                            DOMNode* attr = attrs->item(i);
                            newElem->setAttribute(attr->getNodeName(), attr->getNodeValue());
                        }
                    }
                    // 复制子节点
                    DOMNode* child = elem->getFirstChild();
                    while (child) {
                        DOMNode* next = child->getNextSibling();
                        newElem->appendChild(child->cloneNode(true));
                        child = next;
                    }
                    DOMNode* parent = elem->getParentNode();
                    if (parent) {
                        parent->replaceChild(newElem, elem);
                        m_selectedNode = newElem;
                    }
                }
            }
        }

        // 属性编辑
        DOMNamedNodeMap* attrs = node->getAttributes();
        if (attrs && attrs->getLength() > 0) {
            ImGui::Text("Attributes:");
            for (XMLSize_t i = 0; i < attrs->getLength(); ++i) {
                DOMNode* attr = attrs->item(i);
                char* attrName = XMLString::transcode(attr->getNodeName());
                std::string key(attrName);
                XMLString::release(&attrName);
                // 确保缓冲区存在
                if (m_attrBuf.find(key) == m_attrBuf.end()) {
                    char* val = XMLString::transcode(attr->getNodeValue());
                    m_attrBuf[key] = val;
                    XMLString::release(&val);
                }
                ImGui::Text("%s", key.c_str());
                ImGui::SameLine();
                if (ImGui::InputText(("##attr_" + key).c_str(), &m_attrBuf[key])) {
                    // 更新 DOM 属性值
                    xercesc::DOMElement* elem = dynamic_cast<xercesc::DOMElement*>(node);
                    if (elem) {
                        elem->setAttribute(
                            XMLString::transcode(key.c_str()), 
                            XMLString::transcode(m_attrBuf[key].c_str()));
                    }
                }
            }
        }

        // 文本内容
        ImGui::Text("Text Content:");
        
        if (ImGui::InputTextMultiline("##text", &m_textBuf, ImVec2(-1, 100))) {
            // 更新文本节点
            DOMNode* textChild = nullptr;
            DOMNode* child = node->getFirstChild();
            while (child) {
                if (child->getNodeType() == DOMNode::TEXT_NODE) {
                    textChild = child;
                    break;
                }
                child = child->getNextSibling();
            }
            if (textChild) {
                textChild->setNodeValue(XMLString::transcode(m_textBuf.c_str()));
            } else if (!m_textBuf.empty()) {
                DOMText* newText = node->getOwnerDocument()->createTextNode(XMLString::transcode(m_textBuf.c_str()));
                node->appendChild(newText);
            }
        }

        ImGui::Separator();

        // ---------- 新节点属性预定义 ----------
        ImGui::Text("New Child Attributes:");
        ImGui::PushItemWidth(100);
        ImGui::InputText("##newAttrName", &m_newAttrName);
        ImGui::SameLine();
        ImGui::InputText("##newAttrValue", &m_newAttrValue);
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button("Add Attribute")) {
            if (!m_newAttrName.empty()) {
                m_newAttributes[m_newAttrName] = m_newAttrValue;
                m_newAttrName.clear();
                m_newAttrValue.clear();
            }
        }

        // 显示已添加的属性
        if (!m_newAttributes.empty()) {
            ImGui::Text("Attributes to apply:");
            for (const auto& pair : m_newAttributes) {
                ImGui::BulletText("%s = %s", pair.first.c_str(), pair.second.c_str());
            }
        }

        // ---------- 按钮行 ----------
        if (ImGui::Button("Save")) {
            SaveToFile(m_currentFilePath);
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Child Element")) {
            DOMElement* elem = node->getOwnerDocument()->createElement(XMLString::transcode("newElement"));
            // 应用暂存属性
            for (const auto& pair : m_newAttributes) {
                elem->setAttribute(XMLString::transcode(pair.first.c_str()),
                                XMLString::transcode(pair.second.c_str()));
            }
            node->appendChild(elem);
            m_selectedNode = elem;
            m_nodeNameBuf = "newElement";
            m_textBuf = "";
            UpdateAttrBuf(elem);
            ClearNewAttributes();  // 清空临时属性列表
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete Node")) {
            DOMNode* parent = node->getParentNode();
            if (parent) {
                parent->removeChild(node);
                m_selectedNode = parent;
                if (m_selectedNode == m_doc->getDocumentElement() && !m_doc->getDocumentElement()) {
                    m_selectedNode = nullptr;
                }
                UpdateAttrBuf(m_selectedNode);
                if (m_selectedNode) {
                    char* name = XMLString::transcode(m_selectedNode->getNodeName());
                    m_nodeNameBuf = name;
                    XMLString::release(&name);
                    m_textBuf = GetTextContent(m_selectedNode);
                }
                ClearNewAttributes();  // 切换节点后清空
            }
        }
    }

    // 工具函数
    std::string GetTextContent(DOMNode* node) {
        DOMNode* child = node->getFirstChild();
        while (child) {
            if (child->getNodeType() == DOMNode::TEXT_NODE) {
                char* val = XMLString::transcode(child->getNodeValue());
                std::string s(val);
                XMLString::release(&val);
                return s;
            }
            child = child->getNextSibling();
        }
        return "";
    }

    void UpdateAttrBuf(DOMNode* node) {
        m_attrBuf.clear();
        DOMNamedNodeMap* attrs = node->getAttributes();
        if (attrs) {
            for (XMLSize_t i = 0; i < attrs->getLength(); ++i) {
                DOMNode* attr = attrs->item(i);
                char* name = XMLString::transcode(attr->getNodeName());
                char* value = XMLString::transcode(attr->getNodeValue());
                m_attrBuf[name] = value;
                XMLString::release(&name);
                XMLString::release(&value);
            }
        }
    }
};


static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int, char**) {
    try {
        XMLPlatformUtils::Initialize();
    } catch (const XMLException& e) {
        char* msg = XMLString::transcode(e.getMessage());
        fprintf(stderr, "Xerces init error: %s\n", msg);
        XMLString::release(&msg);
        return 1;
    }

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

#if defined(IMGUI_IMPL_OPENGL_ES2)
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#else
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

    AppConfig config = LoadConfig();
    // 主窗口默认大小
    int winW = 800, winH = 600;
    if (config.mainW > 0 && config.mainH > 0) {
        winW = config.mainW;
        winH = config.mainH;
    }

    GLFWwindow* window = glfwCreateWindow(winW, winH, "Sample Console", nullptr, nullptr);
    if (!window) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // 恢复主窗口位置
    if (config.mainX >= 0 && config.mainY >= 0) {
        glfwSetWindowPos(window, config.mainX, config.mainY);
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    const char* fontPaths[] = {
        "NotoSansSC-Regular.ttf",
        "/usr/share/fonts/truetype/noto/NotoSansSC-Regular.ttf",
        "C:/Windows/Fonts/msyh.ttc",
        "/System/Library/Fonts/PingFang.ttc"
    };
    bool fontLoaded = false;
    for (const char* path : fontPaths) {
        std::ifstream test(path);
        if (test.good()) {
            fontLoaded = (io.Fonts->AddFontFromFileTTF(path, 18.0f, NULL,
                io.Fonts->GetGlyphRangesChineseSimplifiedCommon()) != NULL);
            if (fontLoaded) break;
        }
    }
    if (!fontLoaded) { fprintf(stderr, "Warning: No Chinese font found\n"); io.Fonts->AddFontDefault(); }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    if (config.consoles.empty()) {
        config.consoles.push_back({"output.log", -1, -1, -1, -1});
    }

    std::vector<std::unique_ptr<Console>> consoles;
    std::vector<std::unique_ptr<XMLViewer>> xmlViewers;
    int consoleIdCounter = 0;
    for (auto& entry : config.consoles) {
        auto c = std::make_unique<Console>(consoleIdCounter++);
        c->Open(entry.logFilePath.c_str());
        if (entry.w <= 0 || entry.h <= 0) {
            entry.w = 400; entry.h = 300; entry.x = 100; entry.y = 100;
        }
        consoles.push_back(std::move(c));
    }

    ShortcutManager shortcuts;
    SearchPanel searchPanel;
    MenuBar menuBar;

    menuBar.AddMenuItem("File", "New Console", [&]() {
        IGFD::FileDialogConfig cfg;
        cfg.path = "."; cfg.fileName = "output.log"; cfg.countSelectionMax = 1; cfg.flags = ImGuiFileDialogFlags_Modal;
        ImGuiFileDialog::Instance()->OpenDialog("NewConsoleFile", "Choose Log File", ".log,.txt", cfg);
    }).AddMenuItem("File", "Save Config", [&]() {
        for (size_t i = 0; i < consoles.size(); ++i)
            config.consoles[i].logFilePath = consoles[i]->filePath;
        SaveConfig(config);
    }).AddMenuItem("File", "Exit", [&]() { glfwSetWindowShouldClose(window, true); })
    .AddMenuItem("View", "Toggle Search Panel", [&]() {
        searchPanel.visible = !searchPanel.visible;
        if (searchPanel.visible) searchPanel.Open();
    }).AddMenuItem("View", "XML Viewer", [&]() {
        IGFD::FileDialogConfig cfg;
        cfg.path = ".";
        cfg.countSelectionMax = 1;
        cfg.flags = ImGuiFileDialogFlags_Modal;
        ImGuiFileDialog::Instance()->OpenDialog("XmlViewerFileDialog", "Choose XML File", ".xml", cfg);
    });

    shortcuts.Register(ImGuiMod_Ctrl | ImGuiKey_F, [&]() {
        searchPanel.visible = !searchPanel.visible;
        if (searchPanel.visible) searchPanel.Open();
    }, "Toggle search");

    bool browseDialogOpen = false;
    Console* pendingBrowseConsole = nullptr;
    Console* lastActiveConsole = consoles.empty() ? nullptr : consoles[0].get();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        for (auto& c : consoles) c->Update();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        menuBar.Draw();
        shortcuts.Process();

        if (ImGuiFileDialog::Instance()->Display("NewConsoleFile")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string path = ImGuiFileDialog::Instance()->GetFilePathName();
                auto nc = std::make_unique<Console>(consoleIdCounter++);
                nc->Open(path.c_str());
                consoles.push_back(std::move(nc));
                lastActiveConsole = consoles.back().get();
            }
            ImGuiFileDialog::Instance()->Close();
        }
        if (ImGuiFileDialog::Instance()->Display("XmlViewerFileDialog")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string path = ImGuiFileDialog::Instance()->GetFilePathName();
                auto viewer = std::make_unique<XMLViewer>();
                if (viewer->LoadFromFile(path)) {
                    xmlViewers.push_back(std::move(viewer));
                }
            }
            ImGuiFileDialog::Instance()->Close();
        }

        // 绘制所有 XML Viewer
        for (size_t i = 0; i < xmlViewers.size(); ++i) {
            xmlViewers[i]->Draw();
            // 如果窗口关闭，从列表中移除
            if (!xmlViewers[i]->IsOpen()) {
                xmlViewers.erase(xmlViewers.begin() + i);
                --i;
            }
        }
        if (!browseDialogOpen) {
            pendingBrowseConsole = nullptr;
            for (auto& c : consoles) {
                if (c->showFileDialog) { pendingBrowseConsole = c.get(); break; }
            }
            if (pendingBrowseConsole) {
                IGFD::FileDialogConfig cfg;
                cfg.path = "."; cfg.fileName = "output.log"; cfg.countSelectionMax = 1; cfg.flags = ImGuiFileDialogFlags_Modal;
                ImGuiFileDialog::Instance()->OpenDialog("BrowseLogFile", "Choose Log File", ".log,.txt", cfg);
                pendingBrowseConsole->showFileDialog = false;
                browseDialogOpen = true;
            }
        }

        if (ImGuiFileDialog::Instance()->Display("BrowseLogFile")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                std::string path = ImGuiFileDialog::Instance()->GetFilePathName();
                if (pendingBrowseConsole) {
                    pendingBrowseConsole->Open(path.c_str());
                    lastActiveConsole = pendingBrowseConsole;
                }
            }
            ImGuiFileDialog::Instance()->Close();
            browseDialogOpen = false;
            pendingBrowseConsole = nullptr;
        }

        std::vector<AppConfig::ConsoleEntry> newConfigEntries;
        for (size_t i = 0; i < consoles.size(); ++i) {
            auto& c = consoles[i];
            std::string title = "Console " + std::to_string(i + 1);
            if (!c->filePath.empty()) title += " - " + c->filePath;

            AppConfig::ConsoleEntry entry;
            entry.logFilePath = c->filePath;

            if (i < config.consoles.size()) {
                ImGui::SetNextWindowPos(ImVec2((float)config.consoles[i].x, (float)config.consoles[i].y), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2((float)config.consoles[i].w, (float)config.consoles[i].h), ImGuiCond_FirstUseEver);
            }

            bool open = true;
            bool activated = false;

            bool isActive = (c.get() == lastActiveConsole) && searchPanel.visible;
            const std::vector<int>* hlLines = isActive ? &searchPanel.matchLines : nullptr;
            int hlLine = -1;
            if (isActive && searchPanel.selectedIndex >= 0 && searchPanel.selectedIndex < (int)searchPanel.matchLines.size())
                hlLine = searchPanel.matchLines[searchPanel.selectedIndex];

            c->Draw(title.c_str(), &open, hlLines, hlLine, &activated);

            ImVec2 pos = ImGui::GetWindowPos();
            ImVec2 size = ImGui::GetWindowSize();
            entry.x = (int)pos.x; entry.y = (int)pos.y;
            entry.w = (int)size.x; entry.h = (int)size.y;
            newConfigEntries.push_back(entry);

            if (!open) {
                if (lastActiveConsole == c.get()) {
                    lastActiveConsole = consoles.empty() ? nullptr : (consoles.size() > 1 ? consoles[0].get() : nullptr);
                }
                consoles.erase(consoles.begin() + i);
                --i;
                continue;
            }

            if (activated) lastActiveConsole = c.get();
        }

        config.consoles = std::move(newConfigEntries);

        // 更新主窗口几何
        glfwGetWindowPos(window, &config.mainX, &config.mainY);
        glfwGetWindowSize(window, &config.mainW, &config.mainH);

        if (lastActiveConsole) {
            std::string consoleName = "Console " + std::to_string(lastActiveConsole->consoleId + 1);
            if (!lastActiveConsole->filePath.empty())
                consoleName += " - " + lastActiveConsole->filePath;
            searchPanel.currentConsoleLabel = consoleName;
        } else {
            searchPanel.currentConsoleLabel.clear();
        }

        std::string activeBuffer;
        const void* activeBufferId = nullptr;
        if (lastActiveConsole) {
            activeBuffer = lastActiveConsole->GetBuffer();
            activeBufferId = lastActiveConsole->GetBufferId();
        }

        if (searchPanel.visible && !activeBuffer.empty()) {
            searchPanel.Search(activeBuffer, activeBufferId);
        }

        searchPanel.onGoToLine = [&lastActiveConsole](int line) {
            if (lastActiveConsole) {
                lastActiveConsole->RequestScrollToLine(line);
                lastActiveConsole->autoScroll = false;
            }
        };

        ImGui::SetNextWindowPos(ImVec2((float)config.searchX, (float)config.searchY), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2((float)config.searchW, (float)config.searchH), ImGuiCond_FirstUseEver);
        searchPanel.Draw(activeBuffer, activeBufferId);
        if (searchPanel.visible) {
            ImVec2 pos = ImGui::GetWindowPos();
            ImVec2 size = ImGui::GetWindowSize();
            config.searchX = (int)pos.x; config.searchY = (int)pos.y;
            config.searchW = (int)size.x; config.searchH = (int)size.y;
        }

        
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // 最终保存配置
    for (size_t i = 0; i < consoles.size(); ++i)
        if (i < config.consoles.size())
            config.consoles[i].logFilePath = consoles[i]->filePath;
    SaveConfig(config);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

/*
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <thread>
#include <random>
#include <vector>

// 随机日志消息模板（UTF-8 编码的中文）
static const std::vector<std::string> messages = {
    "系统初始化完成",
    "用户 admin 登录成功",
    "开始处理任务 #%d",
    "警告：内存使用率达到 %d%%",
    "数据库连接池已刷新",
    "收到外部请求，来源 IP: 192.168.1.%d",
    "错误：磁盘空间不足",
    "服务重启中...",
    "任务 #%d 执行超时",
    "新消息：您好，世界！"
};

int main() {
    // 目标文件路径
    std::string filePath1 = "C:\\Users\\lizhuoran.lzr\\Desktop\\new_log.log";
    std::string filePath2 = "C:\\Users\\lizhuoran.lzr\\Desktop\\new_log1.log";

    // 打开两个文件（追加模式，不存在则创建）
    std::ofstream outFile1(filePath1, std::ios::app | std::ios::binary);
    std::ofstream outFile2(filePath2, std::ios::app | std::ios::binary);

    if (!outFile1) {
        std::cerr << "无法打开文件: " << filePath1 << std::endl;
        return 1;
    }
    if (!outFile2) {
        std::cerr << "无法打开文件: " << filePath2 << std::endl;
        return 1;
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> msgDist(0, messages.size() - 1);
    std::uniform_int_distribution<int> paramDist(1, 100);

    int counter = 0;
    while (true) {
        // 生成时间戳（共用）
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        char timeBuffer[30];
        std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S",
                      std::localtime(&now_c));

        // 为第一个文件生成随机日志
        std::string msg1 = messages[msgDist(gen)];
        int param1 = paramDist(gen);
        size_t pos1 = msg1.find("%d");
        if (pos1 != std::string::npos) {
            msg1.replace(pos1, 2, std::to_string(param1));
        }
        std::string logLine1 = "[" + std::string(timeBuffer) + "] " + msg1 + "\n";
        outFile1.write(logLine1.c_str(), logLine1.size());
        outFile1.flush();
        std::cout << "写入文件1: " << logLine1;

        // 为第二个文件生成随机日志
        std::string msg2 = messages[msgDist(gen)];
        int param2 = paramDist(gen);
        size_t pos2 = msg2.find("%d");
        if (pos2 != std::string::npos) {
            msg2.replace(pos2, 2, std::to_string(param2));
        }
        std::string logLine2 = "[" + std::string(timeBuffer) + "] " + msg2 + "\n";
        outFile2.write(logLine2.c_str(), logLine2.size());
        outFile2.flush();
        std::cout << "写入文件2: " << logLine2;

        // 等待 500 毫秒
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        ++counter;
    }

    outFile1.close();
    outFile2.close();
    return 0;
}
*/