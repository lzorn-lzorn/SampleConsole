#pragma once

#include <map>
#include <string>

#include <xercesc/dom/DOM.hpp>

class XmlViewer
{
public:
	XmlViewer() = default;
	~XmlViewer();

	bool loadFromFile(const std::string& InFilePath);
	bool saveToFile(const std::string& InFilePath);
	void draw();

	bool isOpen() const { return Visible; }

private:
	xercesc::DOMDocument* Document = nullptr;
	xercesc::DOMNode* SelectedNode = nullptr;
	bool Visible = false;
	bool ShowLoadDialog = false;
	std::string CurrentFilePath;

	std::string NodeNameBuffer;
	std::string TextBuffer;
	std::map<std::string, std::string> AttributeBuffer;

	std::string NewAttributeName;
	std::string NewAttributeValue;
	std::map<std::string, std::string> NewAttributes;

	void clearNewAttributes();
	void closeDocument();

	void drawNode(xercesc::DOMNode* InNode);
	void drawEditPanel(xercesc::DOMNode* InNode);

	std::string getTextContent(xercesc::DOMNode* InNode);
	void updateAttributeBuffer(xercesc::DOMNode* InNode);
};