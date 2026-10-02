#include "Xml.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


static constexpr size_t XML_BLOCK_SIZE = 64 * 1024;
static constexpr int XML_MAX_DEPTH = 256;

struct XmlBlock
{
    XmlBlock* next;
    size_t size;
    size_t used;
    alignas(max_align_t) unsigned char data[];
};

typedef struct XmlParser
{
    XmlDocument* doc;
    const char* start;
    const char* cursor;
} XmlParser;


static void FreeBlocks(XmlDocument* doc)
{
    for (XmlBlock* block = doc->blocks; block;)
    {
        XmlBlock* next = block->next;
        free(block);
        block = next;
    }
    doc->blocks = nullptr;
}

// zero initialized arena allocation
static void* Allocate(XmlDocument* doc, size_t size)
{
    constexpr size_t align = alignof(max_align_t);
    size = (size + align - 1) & ~(align - 1);

    XmlBlock* block = doc->blocks;
    if (!block || block->size - block->used < size)
    {
        const size_t capacity = size > XML_BLOCK_SIZE ? size : XML_BLOCK_SIZE;
        block = malloc(sizeof(XmlBlock) + capacity);
        if (!block)
        {
            return nullptr;
        }
        block->next = doc->blocks;
        block->size = capacity;
        block->used = 0;
        doc->blocks = block;
    }

    void* memory = block->data + block->used;
    block->used += size;
    return memset(memory, 0, size);
}

static bool Fail(XmlParser* parser, const char* message)
{
    int line = 1;
    for (const char* c = parser->start; c < parser->cursor && *c; ++c)
    {
        line += *c == '\n';
    }
    parser->doc->error = message;
    parser->doc->errorLine = line;
    return false;
}

static bool StartsWith(const char* text, const char* prefix)
{
    return strncmp(text, prefix, strlen(prefix)) == 0;
}

static bool IsNameChar(char c)
{
    // bytes of multibyte UTF-8 sequences are accepted as is
    return isalnum((unsigned char)c) || c == '_' || c == ':' || c == '-' || c == '.' || (unsigned char)c >= 0x80;
}

static bool IsBlank(const char* begin, const char* end)
{
    for (; begin < end; ++begin)
    {
        if (!isspace((unsigned char)*begin))
        {
            return false;
        }
    }
    return true;
}

static void SkipSpace(XmlParser* parser)
{
    while (isspace((unsigned char)*parser->cursor))
    {
        ++parser->cursor;
    }
}

static bool SkipPast(XmlParser* parser, const char* terminator)
{
    const char* found = strstr(parser->cursor, terminator);
    if (!found)
    {
        parser->cursor += strlen(parser->cursor);
        return false;
    }
    parser->cursor = found + strlen(terminator);
    return true;
}

static size_t EncodeUtf8(uint32_t codepoint, char* out)
{
    if (codepoint < 0x80)
    {
        out[0] = (char)codepoint;
        return 1;
    }
    if (codepoint < 0x800)
    {
        out[0] = (char)(0b1100'0000 | (codepoint >> 6));
        out[1] = (char)(0b1000'0000 | (codepoint & 0b11'1111));
        return 2;
    }
    if (codepoint < 0x1'0000)
    {
        out[0] = (char)(0b1110'0000 | (codepoint >> 12));
        out[1] = (char)(0b1000'0000 | ((codepoint >> 6) & 0b11'1111));
        out[2] = (char)(0b1000'0000 | (codepoint & 0b11'1111));
        return 3;
    }
    out[0] = (char)(0b1111'0000 | (codepoint >> 18));
    out[1] = (char)(0b1000'0000 | ((codepoint >> 12) & 0b11'1111));
    out[2] = (char)(0b1000'0000 | ((codepoint >> 6) & 0b11'1111));
    out[3] = (char)(0b1000'0000 | (codepoint & 0b11'1111));
    return 4;
}

// decodes entity at text (pointing to '&') into out, returns decoded length or 0 if it is not a known entity
static size_t DecodeEntity(const char* text, const char* end, char* out, size_t* consumed)
{
    static const struct
    {
        const char* name;
        char value;
    } named[] = {
        { "&lt;", '<' },
        { "&gt;", '>' },
        { "&amp;", '&' },
        { "&quot;", '"' },
        { "&apos;", '\'' },
    };

    for (size_t i = 0; i < sizeof named / sizeof named[0]; ++i)
    {
        const size_t length = strlen(named[i].name);
        if ((size_t)(end - text) >= length && strncmp(text, named[i].name, length) == 0)
        {
            *out = named[i].value;
            *consumed = length;
            return 1;
        }
    }

    if (end - text < 4 || text[1] != '#')
    {
        return 0;
    }

    const bool hex = text[2] == 'x' || text[2] == 'X';
    const char* digits = text + (hex ? 3 : 2);
    char* digitsEnd = nullptr;
    const unsigned long codepoint = strtoul(digits, &digitsEnd, hex ? 16 : 10);
    if (digitsEnd == digits || digitsEnd >= end || *digitsEnd != ';' || codepoint == 0 || codepoint > 0x10'FFFF)
    {
        return 0;
    }

    *consumed = (size_t)(digitsEnd - text) + 1;
    return EncodeUtf8((uint32_t)codepoint, out);
}

// copies [begin, end) into the arena, optionally decoding entities
static const char* CopyText(XmlParser* parser, const char* begin, const char* end, bool decode)
{
    // decoded text is never longer than the source
    char* copy = Allocate(parser->doc, (size_t)(end - begin) + 1);
    if (!copy)
    {
        return nullptr;
    }

    char* out = copy;
    while (begin < end)
    {
        size_t consumed = 0;
        const size_t written = decode && *begin == '&' ? DecodeEntity(begin, end, out, &consumed) : 0;
        if (written > 0)
        {
            out += written;
            begin += consumed;
        }
        else
        {
            *out++ = *begin++;
        }
    }
    *out = '\0';
    return copy;
}

static const char* ParseName(XmlParser* parser)
{
    const char* begin = parser->cursor;
    while (IsNameChar(*parser->cursor))
    {
        ++parser->cursor;
    }
    return parser->cursor == begin ? nullptr : CopyText(parser, begin, parser->cursor, false);
}

// skips whitespace, prolog, comments, processing instructions and doctype outside of the root element
static bool SkipMisc(XmlParser* parser)
{
    for (;;)
    {
        SkipSpace(parser);
        if (StartsWith(parser->cursor, "<?"))
        {
            if (!SkipPast(parser, "?>"))
            {
                return Fail(parser, "unterminated processing instruction");
            }
        }
        else if (StartsWith(parser->cursor, "<!--"))
        {
            if (!SkipPast(parser, "-->"))
            {
                return Fail(parser, "unterminated comment");
            }
        }
        else if (StartsWith(parser->cursor, "<!DOCTYPE"))
        {
            if (!SkipPast(parser, ">"))
            {
                return Fail(parser, "unterminated doctype");
            }
        }
        else
        {
            return true;
        }
    }
}

static bool ParseAttributes(XmlParser* parser, XmlElement* element, bool* selfClosed)
{
    XmlAttribute** tail = &element->attributes;
    for (;;)
    {
        SkipSpace(parser);
        if (StartsWith(parser->cursor, "/>"))
        {
            parser->cursor += 2;
            *selfClosed = true;
            return true;
        }
        if (*parser->cursor == '>')
        {
            ++parser->cursor;
            *selfClosed = false;
            return true;
        }

        XmlAttribute* attribute = Allocate(parser->doc, sizeof *attribute);
        if (!attribute)
        {
            return Fail(parser, "out of memory");
        }

        attribute->name = ParseName(parser);
        if (!attribute->name)
        {
            return Fail(parser, "expected attribute name");
        }

        SkipSpace(parser);
        if (*parser->cursor != '=')
        {
            return Fail(parser, "expected '=' after attribute name");
        }
        ++parser->cursor;
        SkipSpace(parser);

        const char quote = *parser->cursor;
        if (quote != '"' && quote != '\'')
        {
            return Fail(parser, "expected quoted attribute value");
        }

        const char* begin = ++parser->cursor;
        const char* end = strchr(begin, quote);
        if (!end)
        {
            return Fail(parser, "unterminated attribute value");
        }

        attribute->value = CopyText(parser, begin, end, true);
        if (!attribute->value)
        {
            return Fail(parser, "out of memory");
        }
        parser->cursor = end + 1;

        *tail = attribute;
        tail = &attribute->next;
    }
}

static XmlElement* ParseElement(XmlParser* parser, int depth);

static bool ParseContent(XmlParser* parser, XmlElement* element, int depth)
{
    XmlElement** tail = &element->firstChild;
    for (;;)
    {
        if (*parser->cursor == '\0')
        {
            return Fail(parser, "unexpected end of document");
        }

        if (StartsWith(parser->cursor, "</"))
        {
            parser->cursor += 2;
            const char* begin = parser->cursor;
            while (IsNameChar(*parser->cursor))
            {
                ++parser->cursor;
            }

            const size_t length = (size_t)(parser->cursor - begin);
            if (length != strlen(element->name) || strncmp(begin, element->name, length) != 0)
            {
                return Fail(parser, "mismatched closing tag");
            }

            SkipSpace(parser);
            if (*parser->cursor != '>')
            {
                return Fail(parser, "expected '>' in closing tag");
            }
            ++parser->cursor;
            return true;
        }

        if (StartsWith(parser->cursor, "<!--"))
        {
            if (!SkipPast(parser, "-->"))
            {
                return Fail(parser, "unterminated comment");
            }
        }
        else if (StartsWith(parser->cursor, "<?"))
        {
            if (!SkipPast(parser, "?>"))
            {
                return Fail(parser, "unterminated processing instruction");
            }
        }
        else if (StartsWith(parser->cursor, "<![CDATA["))
        {
            const char* begin = parser->cursor + 9;
            const char* end = strstr(begin, "]]>");
            if (!end)
            {
                return Fail(parser, "unterminated CDATA section");
            }
            if (!element->text && !element->firstChild)
            {
                element->text = CopyText(parser, begin, end, false);
            }
            parser->cursor = end + 3;
        }
        else if (*parser->cursor == '<')
        {
            XmlElement* child = ParseElement(parser, depth + 1);
            if (!child)
            {
                return false;
            }
            *tail = child;
            tail = &child->nextSibling;
        }
        else
        {
            const char* begin = parser->cursor;
            const char* end = strchr(begin, '<');
            if (!end)
            {
                end = begin + strlen(begin);
            }
            if (!element->text && !element->firstChild && !IsBlank(begin, end))
            {
                element->text = CopyText(parser, begin, end, true);
            }
            parser->cursor = end;
        }
    }
}

// cursor points to '<' of the start tag
static XmlElement* ParseElement(XmlParser* parser, int depth)
{
    if (depth > XML_MAX_DEPTH)
    {
        Fail(parser, "elements are nested too deep");
        return nullptr;
    }

    ++parser->cursor;
    XmlElement* element = Allocate(parser->doc, sizeof *element);
    if (!element)
    {
        Fail(parser, "out of memory");
        return nullptr;
    }

    element->name = ParseName(parser);
    if (!element->name)
    {
        Fail(parser, "expected element name");
        return nullptr;
    }

    bool selfClosed = false;
    if (!ParseAttributes(parser, element, &selfClosed))
    {
        return nullptr;
    }
    if (!selfClosed && !ParseContent(parser, element, depth))
    {
        return nullptr;
    }
    return element;
}

bool ParseXml(XmlDocument* doc, const char* text)
{
    *doc = (XmlDocument){};
    if (!text)
    {
        doc->error = "no input";
        return false;
    }

    // skip UTF-8 byte order mark
    if (StartsWith(text, "\xEF\xBB\xBF"))
    {
        text += 3;
    }

    XmlParser parser = { .doc = doc, .start = text, .cursor = text };
    bool ok = SkipMisc(&parser);
    if (ok && *parser.cursor != '<')
    {
        ok = Fail(&parser, "expected root element");
    }
    if (ok)
    {
        doc->root = ParseElement(&parser, 0);
        ok = doc->root && SkipMisc(&parser);
    }
    if (ok && *parser.cursor != '\0')
    {
        ok = Fail(&parser, "unexpected content after root element");
    }

    if (!ok)
    {
        FreeBlocks(doc);
        doc->root = nullptr;
    }
    return ok;
}

void UnloadXml(XmlDocument* doc)
{
    FreeBlocks(doc);
    *doc = (XmlDocument){};
}

const char* GetXmlAttribute(const XmlElement* element, const char* name)
{
    for (const XmlAttribute* attribute = element->attributes; attribute; attribute = attribute->next)
    {
        if (strcmp(attribute->name, name) == 0)
        {
            return attribute->value;
        }
    }
    return nullptr;
}

bool QueryXmlInt(const XmlElement* element, const char* name, int* value)
{
    const char* text = GetXmlAttribute(element, name);
    if (!text)
    {
        return false;
    }

    char* end = nullptr;
    errno = 0;
    const long parsed = strtol(text, &end, 10);
    if (end == text || errno == ERANGE || parsed < INT_MIN || parsed > INT_MAX)
    {
        return false;
    }
    *value = (int)parsed;
    return true;
}

bool QueryXmlFloat(const XmlElement* element, const char* name, float* value)
{
    const char* text = GetXmlAttribute(element, name);
    if (!text)
    {
        return false;
    }

    char* end = nullptr;
    const float parsed = strtof(text, &end);
    if (end == text)
    {
        return false;
    }
    *value = parsed;
    return true;
}

bool QueryXmlBool(const XmlElement* element, const char* name, bool* value)
{
    int parsed = 0;
    if (!QueryXmlInt(element, name, &parsed))
    {
        return false;
    }
    *value = parsed != 0;
    return true;
}

static bool Matches(const XmlElement* element, const char* name)
{
    return !name || strcmp(element->name, name) == 0;
}

const XmlElement* FirstXmlChild(const XmlElement* element, const char* name)
{
    const XmlElement* child = element->firstChild;
    while (child && !Matches(child, name))
    {
        child = child->nextSibling;
    }
    return child;
}

const XmlElement* NextXmlSibling(const XmlElement* element, const char* name)
{
    const XmlElement* sibling = element->nextSibling;
    while (sibling && !Matches(sibling, name))
    {
        sibling = sibling->nextSibling;
    }
    return sibling;
}

int CountXmlChildren(const XmlElement* element, const char* name)
{
    int count = 0;
    XML_FOR_EACH_CHILD(child, element, name)
    {
        ++count;
    }
    return count;
}
