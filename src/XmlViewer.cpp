#include "XmlViewer.h"

#include <cctype>

#include "../imgui/imgui.h"
#include "../imgui/misc/cpp/imgui_stdlib.h"
#include "../imgui/ImGuiFileDialog.h"

#include <xercesc/dom/DOM.hpp>
#include <xercesc/dom/DOMDocument.hpp>
#include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMAttr.hpp>
#include <xercesc/dom/DOMText.hpp>
#include <xercesc/parsers/XercesDOMParser.hpp>
#include <xercesc/util/XMLString.hpp>
#include <xercesc/framework/LocalFileFormatTarget.hpp>

using namespace xercesc;

namespace
{
	class ScopedXmlCh
	{
	public:
		explicit ScopedXmlCh(const char* InText)
			: Value(XMLString::transcode(InText))
		{
		}

		~ScopedXmlCh()
		{
			if (Value)
			{
				XMLString::release(&Value);
			}
		}

		ScopedXmlCh(const ScopedXmlCh&) = delete;
		ScopedXmlCh& operator=(const ScopedXmlCh&) = delete;

		const XMLCh* get() const { return Value; }

	private:
		XMLCh* Value = nullptr;
	};

	static std::string toUtf8(const XMLCh* InText)
	{
		if (!InText)
		{
			return {};
		}

		char* Raw = XMLString::transcode(InText);
		if (!Raw)
		{
			return {};
		}

		std::string Result(Raw);
		XMLString::release(&Raw);
		return Result;
	}
}

XmlViewer::~XmlViewer()
{
	closeDocument();
}

bool XmlViewer::loadFromFile(const std::string& InFilePath)
{
	closeDocument();

	try
	{
		XercesDOMParser Parser;
		Parser.parse(InFilePath.c_str());

		Document = Parser.adoptDocument();
		SelectedNode = Document ? Document->getDocumentElement() : nullptr;
		CurrentFilePath = InFilePath;
		Visible = true;

		if (SelectedNode)
		{
			NodeNameBuffer = toUtf8(SelectedNode->getNodeName());
			TextBuffer = getTextContent(SelectedNode);
			updateAttributeBuffer(SelectedNode);
		}

		return true;
	}
	catch (const XMLException& Ex)
	{
		std::string Msg = toUtf8(Ex.getMessage());
		fprintf(stderr, "XML Error: %s\n", Msg.c_str());
		return false;
	}
	catch (...)
	{
		fprintf(stderr, "Unknown error loading XML\n");
		return false;
	}
}

bool XmlViewer::saveToFile(const std::string& InFilePath)
{
	if (!Document || InFilePath.empty())
	{
		return false;
	}

	ScopedXmlCh LsFeature("LS");
	DOMImplementation* Impl = DOMImplementationRegistry::getDOMImplementation(LsFeature.get());
	if (!Impl)
	{
		return false;
	}

	auto* ImplLs = dynamic_cast<DOMImplementationLS*>(Impl);
	if (!ImplLs)
	{
		return false;
	}

	DOMLSSerializer* Serializer = ImplLs->createLSSerializer();
	DOMConfiguration* Config = Serializer->getDomConfig();
	if (Config && Config->canSetParameter(XMLUni::fgDOMWRTFormatPrettyPrint, true))
	{
		Config->setParameter(XMLUni::fgDOMWRTFormatPrettyPrint, true);
	}

	XMLFormatTarget* Target = new LocalFileFormatTarget(InFilePath.c_str());
	DOMLSOutput* Output = ImplLs->createLSOutput();
	Output->setByteStream(Target);

	const bool Ok = Serializer->write(Document, Output);

	Output->release();
	delete Target;
	Serializer->release();

	return Ok;
}

void XmlViewer::draw()
{
	if (!Visible)
	{
		return;
	}

	ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("XML Viewer", &Visible, ImGuiWindowFlags_MenuBar))
	{
		ImGui::End();
		return;
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)
		&& ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S))
	{
		if (!CurrentFilePath.empty())
		{
			saveToFile(CurrentFilePath);
		}
	}

	if (!Document)
	{
		ImGui::Text("No XML file loaded.");
		if (ImGui::Button("Load XML..."))
		{
			ShowLoadDialog = true;
		}

		ImGui::End();
	}
	else
	{
		if (ImGui::BeginMenuBar())
		{
			if (ImGui::MenuItem("Load..."))
			{
				ShowLoadDialog = true;
			}
			if (ImGui::MenuItem("Save"))
			{
				saveToFile(CurrentFilePath);
			}
			ImGui::EndMenuBar();
		}

		ImGui::BeginChild("Tree", ImVec2(250, -1), true);
		if (Document)
		{
			drawNode(Document->getDocumentElement());
		}
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("Edit", ImVec2(0, -1), true);
		if (SelectedNode)
		{
			drawEditPanel(SelectedNode);
		}
		else
		{
			ImGui::Text("Select a node to edit.");
		}
		ImGui::EndChild();

		ImGui::End();
	}

	if (ShowLoadDialog)
	{
		IGFD::FileDialogConfig Cfg;
		Cfg.path = ".";
		Cfg.countSelectionMax = 1;
		Cfg.flags = ImGuiFileDialogFlags_Modal;
		ImGuiFileDialog::Instance()->OpenDialog("XmlViewerInternalFile", "Choose XML File", ".xml", Cfg);
		ShowLoadDialog = false;
	}

	if (ImGuiFileDialog::Instance()->Display("XmlViewerInternalFile"))
	{
		if (ImGuiFileDialog::Instance()->IsOk())
		{
			std::string Path = ImGuiFileDialog::Instance()->GetFilePathName();
			if (loadFromFile(Path))
			{
				CurrentFilePath = Path;
			}
		}

		ImGuiFileDialog::Instance()->Close();
	}
}

void XmlViewer::clearNewAttributes()
{
	NewAttributeName.clear();
	NewAttributeValue.clear();
	NewAttributes.clear();
}

void XmlViewer::closeDocument()
{
	if (Document)
	{
		Document->release();
		Document = nullptr;
	}

	SelectedNode = nullptr;
}

void XmlViewer::drawNode(DOMNode* InNode)
{
	if (!InNode)
	{
		return;
	}

	if (InNode->getNodeType() == DOMNode::ELEMENT_NODE)
	{
		const std::string Name = toUtf8(InNode->getNodeName());

		ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow;
		if (InNode == SelectedNode)
		{
			Flags |= ImGuiTreeNodeFlags_Selected;
		}

		const bool Opened = ImGui::TreeNodeEx((void*)InNode, Flags, "%s", Name.c_str());
		if (ImGui::IsItemClicked())
		{
			SelectedNode = InNode;
			NodeNameBuffer = Name;
			TextBuffer = getTextContent(InNode);
			updateAttributeBuffer(InNode);
		}

		if (Opened)
		{
			DOMNode* Child = InNode->getFirstChild();
			while (Child)
			{
				drawNode(Child);
				Child = Child->getNextSibling();
			}
			ImGui::TreePop();
		}
	}
	else if (InNode->getNodeType() == DOMNode::TEXT_NODE)
	{
		const std::string Text = toUtf8(InNode->getNodeValue());
		bool OnlyWhitespace = true;
		for (unsigned char Ch : Text)
		{
			if (!std::isspace(Ch))
			{
				OnlyWhitespace = false;
				break;
			}
		}

		if (!OnlyWhitespace)
		{
			ImGui::BulletText("\"%s\"", Text.c_str());
		}
	}
}

void XmlViewer::drawEditPanel(DOMNode* InNode)
{
	if (!InNode)
	{
		return;
	}

	ImGui::Text("Node Name:");
	ImGui::SameLine();
	if (ImGui::InputText("##nodename", &NodeNameBuffer) && !NodeNameBuffer.empty())
	{
		auto* Elem = dynamic_cast<DOMElement*>(InNode);
		if (Elem)
		{
			DOMDocument* Doc = InNode->getOwnerDocument();
			if (Doc)
			{
				ScopedXmlCh NewName(NodeNameBuffer.c_str());
				DOMElement* NewElem = Doc->createElement(NewName.get());

				DOMNamedNodeMap* Attrs = Elem->getAttributes();
				if (Attrs)
				{
					for (XMLSize_t i = 0; i < Attrs->getLength(); ++i)
					{
						DOMNode* Attr = Attrs->item(i);
						NewElem->setAttribute(Attr->getNodeName(), Attr->getNodeValue());
					}
				}

				DOMNode* Child = Elem->getFirstChild();
				while (Child)
				{
					DOMNode* Next = Child->getNextSibling();
					NewElem->appendChild(Child->cloneNode(true));
					Child = Next;
				}

				DOMNode* Parent = Elem->getParentNode();
				if (Parent)
				{
					Parent->replaceChild(NewElem, Elem);
					SelectedNode = NewElem;
					updateAttributeBuffer(SelectedNode);
					TextBuffer = getTextContent(SelectedNode);
				}
			}
		}
	}

	DOMNamedNodeMap* Attrs = InNode->getAttributes();
	if (Attrs && Attrs->getLength() > 0)
	{
		ImGui::Text("Attributes:");
		for (XMLSize_t i = 0; i < Attrs->getLength(); ++i)
		{
			DOMNode* Attr = Attrs->item(i);
			const std::string Key = toUtf8(Attr->getNodeName());
			if (AttributeBuffer.find(Key) == AttributeBuffer.end())
			{
				AttributeBuffer[Key] = toUtf8(Attr->getNodeValue());
			}

			ImGui::Text("%s", Key.c_str());
			ImGui::SameLine();
			if (ImGui::InputText(("##attr_" + Key).c_str(), &AttributeBuffer[Key]))
			{
				auto* Elem = dynamic_cast<DOMElement*>(InNode);
				if (Elem)
				{
					ScopedXmlCh Name(Key.c_str());
					ScopedXmlCh Value(AttributeBuffer[Key].c_str());
					Elem->setAttribute(Name.get(), Value.get());
				}
			}
		}
	}

	ImGui::Text("Text Content:");
	if (ImGui::InputTextMultiline("##text", &TextBuffer, ImVec2(-1, 100)))
	{
		DOMNode* TextChild = nullptr;
		DOMNode* Child = InNode->getFirstChild();
		while (Child)
		{
			if (Child->getNodeType() == DOMNode::TEXT_NODE)
			{
				TextChild = Child;
				break;
			}
			Child = Child->getNextSibling();
		}

		if (TextChild)
		{
			ScopedXmlCh Text(TextBuffer.c_str());
			TextChild->setNodeValue(Text.get());
		}
		else if (!TextBuffer.empty())
		{
			ScopedXmlCh Text(TextBuffer.c_str());
			DOMText* NewText = InNode->getOwnerDocument()->createTextNode(Text.get());
			InNode->appendChild(NewText);
		}
	}

	ImGui::Separator();

	ImGui::Text("New Child Attributes:");
	ImGui::PushItemWidth(100);
	ImGui::InputText("##newAttrName", &NewAttributeName);
	ImGui::SameLine();
	ImGui::InputText("##newAttrValue", &NewAttributeValue);
	ImGui::PopItemWidth();
	ImGui::SameLine();

	if (ImGui::Button("Add Attribute"))
	{
		if (!NewAttributeName.empty())
		{
			NewAttributes[NewAttributeName] = NewAttributeValue;
			NewAttributeName.clear();
			NewAttributeValue.clear();
		}
	}

	if (!NewAttributes.empty())
	{
		ImGui::Text("Attributes to apply:");
		for (const auto& Pair : NewAttributes)
		{
			ImGui::BulletText("%s = %s", Pair.first.c_str(), Pair.second.c_str());
		}
	}

	if (ImGui::Button("Save"))
	{
		saveToFile(CurrentFilePath);
	}
	ImGui::SameLine();

	if (ImGui::Button("Add Child Element"))
	{
		DOMDocument* Doc = InNode->getOwnerDocument();
		if (Doc)
		{
			ScopedXmlCh NewElementName("newElement");
			DOMElement* Elem = Doc->createElement(NewElementName.get());

			for (const auto& Pair : NewAttributes)
			{
				ScopedXmlCh AttrName(Pair.first.c_str());
				ScopedXmlCh AttrValue(Pair.second.c_str());
				Elem->setAttribute(AttrName.get(), AttrValue.get());
			}

			InNode->appendChild(Elem);
			SelectedNode = Elem;
			NodeNameBuffer = "newElement";
			TextBuffer.clear();
			updateAttributeBuffer(Elem);
			clearNewAttributes();
		}
	}
	ImGui::SameLine();

	if (ImGui::Button("Delete Node"))
	{
		DOMNode* Parent = InNode->getParentNode();
		if (Parent)
		{
			Parent->removeChild(InNode);
			SelectedNode = Parent;

			if (Document && !Document->getDocumentElement())
			{
				SelectedNode = nullptr;
			}

			updateAttributeBuffer(SelectedNode);
			if (SelectedNode)
			{
				NodeNameBuffer = toUtf8(SelectedNode->getNodeName());
				TextBuffer = getTextContent(SelectedNode);
			}
			else
			{
				NodeNameBuffer.clear();
				TextBuffer.clear();
			}

			clearNewAttributes();
		}
	}
}

std::string XmlViewer::getTextContent(DOMNode* InNode)
{
	if (!InNode)
	{
		return {};
	}

	DOMNode* Child = InNode->getFirstChild();
	while (Child)
	{
		if (Child->getNodeType() == DOMNode::TEXT_NODE)
		{
			return toUtf8(Child->getNodeValue());
		}
		Child = Child->getNextSibling();
	}
	return {};
}

void XmlViewer::updateAttributeBuffer(DOMNode* InNode)
{
	AttributeBuffer.clear();
	if (!InNode)
	{
		return;
	}

	DOMNamedNodeMap* Attrs = InNode->getAttributes();
	if (!Attrs)
	{
		return;
	}

	for (XMLSize_t i = 0; i < Attrs->getLength(); ++i)
	{
		DOMNode* Attr = Attrs->item(i);
		AttributeBuffer[toUtf8(Attr->getNodeName())] = toUtf8(Attr->getNodeValue());
	}
}