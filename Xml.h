#pragma once

// Minimal non-validating XML DOM parser, enough for Tiled .tmx/.tsx files.
// All nodes and strings live in an arena owned by the document.

typedef struct XmlAttribute XmlAttribute;
typedef struct XmlElement XmlElement;
typedef struct XmlBlock XmlBlock;

struct XmlAttribute
{
    const char* name;
    const char* value;          // entities already decoded
    XmlAttribute* next;
};

struct XmlElement
{
    const char* name;
    const char* text;           // first non blank text or CDATA before any child element, nullptr if none
    XmlAttribute* attributes;
    XmlElement* firstChild;
    XmlElement* nextSibling;
};

typedef struct XmlDocument
{
    XmlElement* root;
    XmlBlock* blocks;
    const char* error;          // static message describing the parse failure
    int errorLine;
} XmlDocument;

// on failure the document keeps only error and errorLine, nothing has to be unloaded
[[nodiscard]] bool ParseXml(XmlDocument* doc, const char* text);
void UnloadXml(XmlDocument* doc);

[[nodiscard]] const char* GetXmlAttribute(const XmlElement* element, const char* name);

// value is left untouched when the attribute is missing or malformed
bool QueryXmlInt(const XmlElement* element, const char* name, int* value);
bool QueryXmlFloat(const XmlElement* element, const char* name, float* value);
bool QueryXmlBool(const XmlElement* element, const char* name, bool* value);

// name of nullptr matches any element
[[nodiscard]] const XmlElement* FirstXmlChild(const XmlElement* element, const char* name);
[[nodiscard]] const XmlElement* NextXmlSibling(const XmlElement* element, const char* name);
[[nodiscard]] int CountXmlChildren(const XmlElement* element, const char* name);

#define XML_FOR_EACH_CHILD(child, parent, name) \
    for (const XmlElement* child = FirstXmlChild((parent), (name)); child; child = NextXmlSibling(child, (name)))
