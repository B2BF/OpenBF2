// Client list-box cell data. [INFERRED] Source placement.
#include "BfListBoxData.h"
#include "BfNewListBoxNode.h"
#if !DEDICATED
#include "../External/BFMeme/TypeDescriptor.h"
#include "../External/BFMeme/Stream.h"
#include "../External/BFMeme/Action.h"
#include "../External/BFMeme/Node.h"
#include "../External/BFMeme/Style.h"
#include "../External/BFMeme/System.h"
#include <cmath>
#include <new>
#if !defined(_M_IX86)
#include <emmintrin.h>
#endif
namespace dice { namespace meme {
// [FRONTIER] Releases a cell resource through an unidentified interface. [INFERRED] Name and void return.
// Keep declaration-only until the full typed interface prefix is established.
void releaseListBoxCellResource(dice::hfe::IBase*, int resourceIndex);
namespace {
// PE's x87 fistp integer conversion, including its saved rounding mode. This
// expression helper is not a separately attested retail function.
int truncateListWidth(double value)
{
#if defined(_M_IX86)
    unsigned short original, truncating;
    int result;
    __asm {
        fnstcw original
        mov ax, original
        or ax, 0C00h
        mov truncating, ax
        fldcw truncating
        fld value
        fistp result
        fldcw original
    }
    return result;
#else
    // cvttsd2si truncates like the chopped fistp, including the 0x80000000 indefinite result.
    return _mm_cvttsd_si32(_mm_set_sd(value));
#endif
}
}
ListBoxData::ListBoxData(const char* name) : Data(name), m_row(0) {}
TypeDescriptor* ListBoxData::getClassType()
{
    static ClassTypeDescriptor<ListBoxData, Data> type("dice::meme::ListBoxData");
    return &type;
}
TypeDescriptor* ListBoxData::getType() { return getClassType(); }
unsigned int ListBoxData::getObjectSize() { return sizeof(ListBoxData); }
bool ListBoxData::onCreate(System*) { return true; }
int ListBoxData::getCharacterElement(System*, int, int) { return 0; }
int ListBoxData::getElementCount(System*) { return 0; }
void ListBoxData::setRow(int row) { m_row = row; }
void ListBoxData::setColumn(int column) { m_column = column; }
bool ListBoxData::activateSelection(System*) { return false; }
bool ListBoxData::updateSelection(System*, const Rect*, const NodeState*) { return false; }

ListBoxCell::ListBoxCell(std::wstring text, float r, float g, float b, float a, int value)
    : narrow(), wide()
{
    wide = text; // The potentially allocating copy precedes all scalar stores.
    color = {r,g,b,a};
    isWide = true;
    field068 = 1;
    texture = 0;
    isImage = false;
    imageWidth = imageHeight = imageX = imageY = 0;
    field06c = false;
    resourceIndex = -1;
    field064 = value;
    field090 = 0;
}
// Both strings are default-constructed first, then `narrow` is assigned
// (87F318 operator=, not a copy-construct) - the shipped order, and the reason
// the wide member exists but stays empty on a narrow cell.
ListBoxCell::ListBoxCell(std::string text, float r, float g, float b, float a, int value)
    : narrow(), wide()
{
    narrow = text;
    color = {r,g,b,a};
    texture = 0;
    isImage = false;
    imageWidth = imageHeight = imageX = imageY = 0;
    field06c = false;
    isWide = false;
    resourceIndex = -1;
    field064 = value;
    field068 = 1;
    field090 = 0;
}
// The positioned narrow cell. Same construction order as 7C2B70 above - both strings
// default-constructed, then `narrow` assigned - and the same defaults, except that imageX/imageY
// take the caller's offset. isImage stays false and texture stays 0, so this is still a text
// cell; the two image fields are being reused as a text offset.
ListBoxCell::ListBoxCell(std::string text, float x, float y, float r, float g, float b, float a,
    int value) : narrow(), wide()
{
    narrow = text;
    color = {r,g,b,a};
    imageX = x; imageY = y;
    texture = 0;
    isImage = false;
    imageWidth = imageHeight = 0;
    field06c = false;
    isWide = false;
    resourceIndex = -1;
    field064 = value;
    field068 = 1;
    field090 = 0;
}
// 7C2C80, the wide twin of 7C2BF0: `wide` is assigned and isWide is true.
ListBoxCell::ListBoxCell(std::wstring text, float x, float y, float r, float g, float b, float a,
    int value) : narrow(), wide()
{
    wide = text; // The potentially allocating copy precedes all scalar stores.
    color = {r,g,b,a};
    imageX = x; imageY = y;
    isWide = true;
    field068 = 1;
    texture = 0;
    isImage = false;
    imageWidth = imageHeight = 0;
    field06c = false;
    resourceIndex = -1;
    field064 = value;
    field090 = 0;
}
ListBoxColumn::ListBoxColumn(int number, int size, bool flag, bool flag2)
    : index(number), cells(), width(size), enabled(true), field019(flag),
      field01a(flag2), field01c(0), field020(-1) {}
ListBoxCell::ListBoxCell(TextureHandle image, float width, float height, float x, float y,
    float r, float g, float b, float a) : narrow(), wide()
{
    texture = image;
    color = {r,g,b,a};
    imageWidth = width; imageHeight = height; imageX = x; imageY = y;
    field06c = false; isWide = false; field064 = 0; field090 = 0;
    isImage = true; resourceIndex = -1; field068 = 1;
}
BfListBoxData::BfListBoxData(const char* name)
    : ListBoxData(name), m_node(nullptr), m_createAction(nullptr), m_createString(nullptr),
      m_profileName(nullptr), m_outline(false), m_field021(true), m_field022(true),
      m_field023(true), m_field024(0), m_visibleRows(20), m_field030(1),
      m_field034(7), m_field038(8), m_field03c(-1), m_field040(false),
      m_field044(800), m_resource(nullptr), m_field04c(0), m_columns(),
      m_columnCount(0), m_activeColumn(nullptr)
{
    m_row = -1;
    addColumn(0, true, false);
}
BfListBoxData::~BfListBoxData()
{
    // 7C57E0 releases pointer-vector storage before resource release. It never
    // calls7C89B0 and never deletes the columns or their cells.
    std::vector<ListBoxColumn*>().swap(m_columns);
    m_columns.resize(0, nullptr);
    if (m_resource) m_resource->release();
}
unsigned int BfListBoxData::getObjectSize() { return sizeof(BfListBoxData); }
TypeDescriptor* BfListBoxData::getClassType()
{
    static ClassTypeDescriptor<BfListBoxData, ListBoxData> type("dice::meme::BfListBoxData");
    return &type;
}
TypeDescriptor* BfListBoxData::getType() { return getClassType(); }
void BfListBoxData::selectRow(int row, bool notify)
{
    const bool emit = (row != m_row || !m_field023) && notify;
    m_row = row;
    if (m_node && emit) m_node->enqueueEvent(7);
}
int BfListBoxData::getFirstVisibleRow() { return m_node ? m_node->m_firstRow : 0; }
void BfListBoxData::selectActiveColumnForNode(unsigned int index)
{
    m_activeColumn = index < m_columns.size() ? m_columns[index] : nullptr;
}
// The node guard wraps both the store and the event, so there is no pending row at all
// until a node is attached. Event 8 is this pair's counterpart to selectRow's event 7.
void BfListBoxData::setFirstVisibleRow(int row)
{
    if (!m_node) return;
    m_field028 = row;
    m_node->enqueueEvent(8);
}
bool BfListBoxData::streamProperties(System* system, Stream* stream)
{
    if (!Data::streamProperties(system, stream)) return false;
    if (!stream->streamAction(system, m_createAction, "Create new action", nullptr)) return false;
    if (!stream->streamData(system, m_createString, "Create new string", nullptr)) return false;
    return stream->streamData(system, m_profileName, "Profile name", nullptr) != false;
}
int BfListBoxData::getCharacterElement(System*, int index, int character)
{
    if (!m_activeColumn || static_cast<unsigned int>(index) >= m_activeColumn->cells.size()) return 0;
    ListBoxCell* cell = m_activeColumn->cells[index];
    if (!cell || cell->isImage) return 0;
    const int length = static_cast<int>(cell->isWide ? cell->wide.size() : cell->narrow.size());
    if (character >= length) return 0; // Retail does not reject negative character indexes.
    return cell->isWide ? static_cast<unsigned short>(cell->wide.c_str()[character])
                        : static_cast<signed char>(cell->narrow.c_str()[character]);
}
int BfListBoxData::getElementCount(System*)
{
    ListBoxColumn* column = m_columns[0]; // Shipped missing outer-vector guard.
    return column ? static_cast<int>(column->cells.size()) : 0;
}
int BfListBoxData::getRowCount()
{ return static_cast<int>(m_columns[0]->cells.size()); }
bool BfListBoxData::activateSelection(System* system)
{
    if (m_row != 0) return false;
    if (m_createAction) {
        const NodeInputEvent event = {1, 1, 0, 0, 0};
        const NodeState state = {1, 1};
        const Rect rect = {0, 0, 0, 0, 0};
        m_createAction->execute(system, &rect, &state, &event);
    }
    return true;
}
bool BfListBoxData::updateSelection(System* system, const Rect*, const NodeState*)
{
    if (m_row >= getElementCount(system)) m_row = 0;
    return false;
}
bool BfListBoxData::addColumn(int width, bool flag, bool flag2)
{
    void* allocation = ::operator new(sizeof(ListBoxColumn));
    ListBoxColumn* column = allocation ? new(allocation) ListBoxColumn(m_columnCount, width, flag, flag2) : nullptr;
    m_columns.push_back(column);
    ++m_columnCount;
    return true;
}
bool BfListBoxData::appendWideCell(int index, std::wstring text, float r, float g, float b, float a, int value)
{
    if (static_cast<unsigned int>(index) < m_columns.size()) {
        ListBoxColumn* column = m_columns[index];
        if (column) {
            void* allocation = ::operator new(sizeof(ListBoxCell));
            ListBoxCell* cell = allocation ? new(allocation) ListBoxCell(text, r, g, b, a, value) : nullptr;
            column->cells.push_back(cell);
        }
    }
    return true;
}
// 7C8090, the narrow twin of appendWideCell, byte for byte at the same shape.
// It returns true unconditionally: the `mov al,1` sits past the join point, so
// a column index at or past the end, a null column and a failed allocation are
// all reported as success. A failed allocation still appends a null cell.
bool BfListBoxData::appendCell(int index, std::string text, float r, float g, float b, float a, int value)
{
    if (static_cast<unsigned int>(index) < m_columns.size()) {
        ListBoxColumn* column = m_columns[index];
        if (column) {
            void* allocation = ::operator new(sizeof(ListBoxCell));
            ListBoxCell* cell = allocation ? new(allocation) ListBoxCell(text, r, g, b, a, value) : nullptr;
            column->cells.push_back(cell);
        }
    }
    return true;
}
// pure forwarders, every argument re-pushed unchanged (the
// string re-copied). Same treatment as setWideCell0x7c87f0 above.
bool BfListBoxData::appendCell0x7c81c0(int index, std::string text,
    float r, float g, float b, float a, int value)
{ return appendCell(index, text, r, g, b, a, value); }
bool BfListBoxData::appendImageCell0x7c8300(int index, TextureHandle texture, float width, float height,
    float x, float y, float r, float g, float b, float a)
{ return appendImageCell(index, texture, width, height, x, y, r, g, b, a); }
// 7C8120 / 7C83D0 and their forwarders. See BfListBoxData.h for the shape and the two inherited
// quirks; the only difference from appendCell / appendWideCell is the constructor selected.
bool BfListBoxData::appendPositionedCell(int index, std::string text, float x, float y,
    float r, float g, float b, float a, int value)
{
    if (static_cast<unsigned int>(index) < m_columns.size()) {
        ListBoxColumn* column = m_columns[index];
        if (column) {
            void* allocation = ::operator new(sizeof(ListBoxCell));
            ListBoxCell* cell = allocation
                ? new(allocation) ListBoxCell(text, x, y, r, g, b, a, value) : nullptr;
            column->cells.push_back(cell);
        }
    }
    return true;
}
bool BfListBoxData::appendPositionedWideCell(int index, std::wstring text, float x, float y,
    float r, float g, float b, float a, int value)
{
    if (static_cast<unsigned int>(index) < m_columns.size()) {
        ListBoxColumn* column = m_columns[index];
        if (column) {
            void* allocation = ::operator new(sizeof(ListBoxCell));
            ListBoxCell* cell = allocation
                ? new(allocation) ListBoxCell(text, x, y, r, g, b, a, value) : nullptr;
            column->cells.push_back(cell);
        }
    }
    return true;
}
bool BfListBoxData::appendPositionedCell0x7c8210(int index, std::string text, float x, float y,
    float r, float g, float b, float a, int value)
{ return appendPositionedCell(index, text, x, y, r, g, b, a, value); }
bool BfListBoxData::appendPositionedWideCell0x7c8470(int index, std::wstring text, float x, float y,
    float r, float g, float b, float a, int value)
{ return appendPositionedWideCell(index, text, x, y, r, g, b, a, value); }
bool BfListBoxData::addHiddenColumn()
{
    // unlike addColumn, the new column's enabled byte is false.
    void* allocation = ::operator new(sizeof(ListBoxColumn));
    ListBoxColumn* column = allocation ? new(allocation) ListBoxColumn(m_columnCount, 0, true, false) : nullptr;
    if (column) column->enabled = false;
    m_columns.push_back(column);
    ++m_columnCount;
    return true;
}
bool BfListBoxData::addIconColumn(int width, int height, bool flag)
{
    void* allocation = ::operator new(sizeof(ListBoxColumn));
    ListBoxColumn* column = allocation ? new(allocation) ListBoxColumn(m_columnCount, width, false, flag) : nullptr;
    if (column) column->field020 = height;
    m_columns.push_back(column);
    ++m_columnCount;
    return true;
}
std::string BfListBoxData::getSelectedString(int index)
{
    if (static_cast<unsigned int>(index) < m_columns.size()) {
        ListBoxColumn* column = m_columns[index];
        if (column && static_cast<unsigned int>(m_row) < column->cells.size()) {
            ListBoxCell* cell = column->cells[m_row];
            if (cell) return cell->narrow;
        }
    }
    return "";
}
std::wstring BfListBoxData::getWideCell(int index, int row)
{
    if (static_cast<unsigned int>(index) < m_columns.size()) {
        ListBoxColumn* column = m_columns[index];
        if (column && static_cast<unsigned int>(row) < column->cells.size()) {
            ListBoxCell* cell = column->cells[row];
            if (cell) return cell->wide;
        }
    }
    return L"";
}
bool BfListBoxData::appendImageCell(int index, TextureHandle texture, float width, float height,
    float x, float y, float r, float g, float b, float a)
{
    if (static_cast<unsigned int>(index) < m_columns.size()) {
        ListBoxColumn* column = m_columns[index];
        if (column) {
            void* allocation = ::operator new(sizeof(ListBoxCell));
            ListBoxCell* cell = allocation ? new(allocation) ListBoxCell(texture, width, height, x, y, r, g, b, a) : nullptr;
            column->cells.push_back(cell);
        }
    }
    return true;
}
bool BfListBoxData::setImageCell(int index, int row, TextureHandle texture, float width, float height,
    float x, float y, float r, float g, float b, float a, int extra)
{
    if (static_cast<unsigned int>(index) < m_columns.size()) {
        ListBoxColumn* column = m_columns[index];
        if (column && static_cast<unsigned int>(row) < column->cells.size()) {
            ListBoxCell* cell = column->cells[row];
            if (cell) {
                if (cell->texture != texture) cell->texture = texture;
                if (cell->imageWidth != width) cell->imageWidth = width;
                if (cell->imageHeight != height) cell->imageHeight = height;
                if (cell->imageX != x) cell->imageX = x;
                if (cell->imageY != y) cell->imageY = y;
                if (cell->color.r != r) cell->color.r = r;
                if (cell->color.g != g) cell->color.g = g;
                if (cell->color.b != b) cell->color.b = b;
                if (cell->color.a != a) cell->color.a = a;
                cell->isImage = true; cell->isWide = false;
                cell->resourceIndex = -1; cell->field090 = extra;
                return true;
            }
        }
    }
    // Retail fallback discards the requested row, color and extra field.
    appendImageCell(index, texture, width, height, x, y, 1,1,1,1);
    return true;
}
bool BfListBoxData::setWideCell(int index, int row, std::wstring text,
    float r, float g, float b, float a, int value, int extra)
{
    if (static_cast<unsigned int>(index) < m_columns.size()) {
        ListBoxColumn* column = m_columns[index];
        if (column && static_cast<unsigned int>(row) < column->cells.size()) {
            ListBoxCell* cell = column->cells[row];
            if (cell) {
                if (cell->wide.compare(text)) cell->wide = text;
                if (cell->color.r != r) cell->color.r = r;
                if (cell->color.g != g) cell->color.g = g;
                if (cell->color.b != b) cell->color.b = b;
                if (cell->color.a != a) cell->color.a = a;
                if (cell->field064 != value) cell->field064 = value;
                cell->isImage = false;
                cell->isWide = true;
                cell->resourceIndex = -1;
                cell->field090 = extra;
                return true;
            }
        }
    }
    appendWideCell(index, text, r, g, b, a, value); // requested row/extra discarded in retail
    return true;
}
bool BfListBoxData::setWideCell0x7c87f0(int index, int row, std::wstring text,
    float r, float g, float b, float a, int value, int extra)
{ return setWideCell(index, row, text, r, g, b, a, value, extra); }
// Updates a positioned cell. Unequal or unordered floating-point comparisons trigger writes.
bool BfListBoxData::setPositionedCell(int index, int row, std::string text, float x, float y,
    float r, float g, float b, float a, int value, int extra)
{
    if (static_cast<unsigned int>(index) < m_columns.size()) {
        ListBoxColumn* column = m_columns[index];
        if (column && static_cast<unsigned int>(row) < column->cells.size()) {
            ListBoxCell* cell = column->cells[row];
            if (cell) {
                if (cell->narrow.compare(text)) cell->narrow = text;
                if (cell->color.r != r) cell->color.r = r;
                if (cell->color.g != g) cell->color.g = g;
                if (cell->color.b != b) cell->color.b = b;
                if (cell->color.a != a) cell->color.a = a;
                if (cell->imageX != x) cell->imageX = x;                 // +58
                if (cell->imageY != y) cell->imageY = y;                 // +5C
                if (cell->field064 != value) cell->field064 = value;
                cell->field090 = extra;                                  // +90 first here,
                cell->isImage = false;
                cell->isWide = false;                                    // the wide twin sets true
                cell->resourceIndex = -1;
                return true;
            }
        }
    }
    appendPositionedCell(index, text, x, y, r, g, b, a, value); // requested row/extra discarded in retail
    return true;
}
// 7C8640 (88 B): the pure forwarder, every argument re-pushed and the string re-copied.
bool BfListBoxData::setPositionedCell0x7c8640(int index, int row, std::string text, float x,
    float y, float r, float g, float b, float a, int value, int extra)
{ return setPositionedCell(index, row, text, x, y, r, g, b, a, value, extra); }
bool BfListBoxData::clearRows()
{
    for (int index = 0; index < m_columnCount; ++index) {
        if (static_cast<unsigned int>(index) < m_columns.size()) {
            ListBoxColumn* column = m_columns[index];
            if (column) {
                for (ListBoxCell* cell : column->cells) delete cell;
                std::vector<ListBoxCell*>().swap(column->cells);
                column->cells.resize(0, nullptr);
            }
        }
    }
    return true;
}
// 7C4AF0 and its forwarder 7C4BA0. The column walk is the same shape clearRow uses below, but the
// cell lookup is the vector's own iteration rather than an index, and a column whose cell vector is
// shorter than `row` is simply skipped.
bool BfListBoxData::setRowHighlight(int row, float r, float g, float b, float a,
    float field084, float field088, float field080)
{
    for (int index = 0; index < m_columnCount; ++index) {
        ListBoxColumn* const column =
            static_cast<unsigned int>(index) < m_columns.size() ? m_columns[index] : nullptr;
        if (!column) continue;
        int position = 0;
        for (std::vector<ListBoxCell*>::iterator it = column->cells.begin();
             it != column->cells.end(); ++it, ++position) {
            if (position != row) continue;
            ListBoxCell* const cell = *it;
            cell->highlight = {r,g,b,a};
            cell->highlightField084 = field084;
            cell->field06c = true;
            cell->highlightField088 = field088;
            cell->highlightField080 = field080;
            break;
        }
    }
    return true;
}
bool BfListBoxData::setRowHighlight0x7c4ba0(int row, float r, float g, float b, float a,
    float field084, float field088, float field080)
{ return setRowHighlight(row, r, g, b, a, field084, field088, field080); }
bool BfListBoxData::clearRow(int row)
{
    for (int index = 0; index < m_columnCount; ++index) clearCell(index, row);
    return true;
}
bool BfListBoxData::clearCell(int index, int row)
{
    if (static_cast<unsigned int>(index) < m_columns.size()) {
        ListBoxColumn* column = m_columns[index];
        if (column && static_cast<unsigned int>(row) < column->cells.size()) {
            ListBoxCell* cell = column->cells[row];
            if (cell) {
                cell->texture = 0; cell->isImage = false;
                cell->narrow = ""; cell->wide = L"";
                cell->color = {1,1,1,1};
                cell->imageWidth = cell->imageHeight = cell->imageX = cell->imageY = 0;
                cell->field06c = false; cell->isWide = false;
                if (m_resource) releaseListBoxCellResource(m_resource, cell->resourceIndex);
                cell->resourceIndex = -1; cell->field090 = 0;
            }
        }
    }
    return true;
}
bool BfListBoxData::copyCell(int index, int row, const ListBoxCell* source)
{
    if (static_cast<unsigned int>(index) < m_columns.size() && m_columns[index]) {
        if (static_cast<unsigned int>(index) < m_columns.size()) {
            ListBoxColumn* column = m_columns[index];
            if (column && static_cast<unsigned int>(row) < column->cells.size()) {
                ListBoxCell* cell = column->cells[row];
                if (cell) {
                    if (m_resource) releaseListBoxCellResource(m_resource, cell->resourceIndex);
                    cell->resourceIndex = -1;
                    // No source-null guard; source may also equal destination.
                    cell->texture = source->texture; cell->isImage = source->isImage;
                    cell->narrow = source->narrow; cell->wide = source->wide;
                    cell->color = source->color;
                    cell->imageWidth = source->imageWidth; cell->imageHeight = source->imageHeight;
                    cell->imageX = source->imageX; cell->imageY = source->imageY;
                    cell->field06c = source->field06c; cell->isWide = source->isWide;
                    cell->field090 = source->field090;
                }
            }
        }
    }
    return true;
}
bool BfListBoxData::copyRow(int from, int to)
{
    for (int index = 0; index < m_columnCount; ++index) {
        if (static_cast<unsigned int>(index) < m_columns.size()) {
            ListBoxColumn* column = m_columns[index];
            if (column) {
                int row = 0;
                for (auto it = column->cells.begin(); it != column->cells.end(); ++it, ++row) {
                    if (row == from) { copyCell(index, to, *it); break; }
                }
            }
        }
    }
    return true;
}
bool BfListBoxData::moveRow(int from, int to)
{
    copyRow(from, to);
    for (int index = 0; index < m_columnCount; ++index) clearCell(index, from);
    return true;
}
int BfListBoxData::getColumnRightEdge(int index)
{
    const int right = m_node->m_border ? m_node->m_right : 0; // no node guard
    const int total = truncateListWidth(std::floor(m_node->m_width)) - right;
    int result = total;
    if (m_activeColumn) {
        const int start = m_activeColumn->width;
        if (!m_node || (m_field022 && m_activeColumn->field019)) {
            ListBoxColumn* column = static_cast<unsigned int>(index) < m_columns.size() ? m_columns[index] : nullptr;
            if (column) {
                unsigned int next = static_cast<unsigned int>(index) + 1;
                do {
                    ++next; // shipped scan skips index+1 when current column is disabled
                    if (column->enabled) { result = column->width; break; }
                    column = next < m_columns.size() ? m_columns[next] : nullptr;
                } while (column);
            }
        }
        const int limit = m_activeColumn->field020;
        if (limit > 0 && result > limit) result = start + limit;
        if (m_node && m_field021) {
            if (result > total - truncateListWidth(std::floor(static_cast<double>(m_node->m_scrollOffset) + m_node->m_scrollWidth)))
                result -= truncateListWidth(std::floor(static_cast<double>(m_node->m_scrollOffset) + m_node->m_scrollWidth));
        } else if (result > total - 6) result -= 6;
    }
    return result;
}
int BfListBoxData::getAvailableWidth(int alignment)
{
    const int left = m_node->m_border ? m_node->m_left : 0;
    const int right = m_node->m_border ? m_node->m_right : 0;
    const int total = truncateListWidth(std::floor(m_node->m_width)) - right;
    if (!m_activeColumn) return total - left;

    int start = m_activeColumn->width;
    if (alignment > 0 && alignment <= 2 && (!m_field022 || !m_activeColumn->field019)) {
        if (m_node->m_border)
            return getColumnRightEdge(m_activeColumn->index + 1) - m_node->m_left;
        start = 0;
    }
    return getColumnRightEdge(m_activeColumn->index + 1) - start;
}
int BfListBoxData::getAvailableWidth(int index, int alignment)
{
    const int left = m_node->m_border ? m_node->m_left : 0;
    const int right = m_node->m_border ? m_node->m_right : 0;
    const int total = truncateListWidth(std::floor(m_node->m_width) - right);
    if (static_cast<unsigned int>(index) < m_columns.size()) {
        ListBoxColumn* column = m_columns[index];
        if (column) {
            int start = column->width;
            if (alignment > 0 && alignment <= 2 && (!m_field022 || !m_activeColumn->field019)) {
                if (m_node->m_border) {
                    const int border = m_node->m_left;
                    return getColumnRightEdge(column->index + 1) - border;
                }
                start = 0;
            }
            return getColumnRightEdge(column->index + 1) - start;
        }
    }
    return total - left;
}

// These are insertion helpers, not append operations: an
// insertion at end() is refused, allocation failure still inserts null, and
// the return is true on every path. Wrapped text also loses its alignment;
// both constructors receive the shipped hard-coded zero.
bool BfListBoxData::insertCell(int index, int row, std::string text,
    float r, float g, float b, float a)
{
    ListBoxColumn* column =
        static_cast<unsigned int>(index) < m_columns.size() ? m_columns[index] : nullptr;
    if (column) {
        std::vector<ListBoxCell*>::iterator it = column->cells.begin();
        int position = 0;
        while (it != column->cells.end()) {
            if (position == row) {
                void* allocation = ::operator new(sizeof(ListBoxCell));
                ListBoxCell* cell = allocation
                    ? new(allocation) ListBoxCell(text, r, g, b, a, 0) : nullptr;
                column->cells.insert(it, cell);
                break;
            }
            ++it;
            ++position;
        }
    }
    return true;
}

bool BfListBoxData::insertWideCell(int index, int row, std::wstring text,
    float r, float g, float b, float a)
{
    ListBoxColumn* column =
        static_cast<unsigned int>(index) < m_columns.size() ? m_columns[index] : nullptr;
    if (column) {
        std::vector<ListBoxCell*>::iterator it = column->cells.begin();
        int position = 0;
        while (it != column->cells.end()) {
            if (position == row) {
                void* allocation = ::operator new(sizeof(ListBoxCell));
                ListBoxCell* cell = allocation
                    ? new(allocation) ListBoxCell(text, r, g, b, a, 0) : nullptr;
                column->cells.insert(it, cell);
                break;
            }
            ++it;
            ++position;
        }
    }
    return true;
}
int BfListBoxData::measureText(System* system, std::wstring text, int column, int row, int* fontMode)
{
    int characters[256];
    std::string narrow;
    std::wstring wide;
    float width = 0;
    int index = 0, mode = *fontMode;
    GlyphMetrics metrics; // retail ignores getGlyphMetrics failure; do not initialize
    while (true) {
        const int character = static_cast<unsigned short>(text.c_str()[index]);
        characters[index] = character;
        if (!character) break;
        const int next = static_cast<unsigned short>(text.c_str()[index + 1]);
        characters[index + 1] = next;
        if (character == 0xA7 && next >= '0' && next <= '3') {
            mode = next - '0'; index += 2;
        } else if (character == 0xA7 && (next == 'C' || next == 'c')) {
            index += 6;
        } else {
            system->getGlyphMetrics(character, m_node->m_fonts[mode], &metrics);
            ++index;
            // Keep all three additions on x87 until the single float store.
#if defined(_M_IX86)
            __asm {
                lea eax, metrics
                fld dword ptr [eax+8]
                fadd dword ptr [eax+4]
                fadd dword ptr [eax]
                fadd width
                fstp width
            }
#else
            // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
            // +8 rightBearing, +4 width, +0 leftBearing.
            width = static_cast<float>(static_cast<double>(metrics.rightBearing) + metrics.width
                                       + metrics.leftBearing + width);
#endif
        }
        if (index >= 250) break;
        float imageX = 0;
        if (static_cast<unsigned int>(column) < m_columns.size()) {
            ListBoxColumn* entry = m_columns[column];
            if (entry && static_cast<unsigned int>(row) < entry->cells.size()) {
                ListBoxCell* cell = entry->cells[row];
                if (cell) imageX = cell->imageX;
            }
        }
        int alignment = 0;
        if (static_cast<unsigned int>(column) < m_columns.size()) {
            ListBoxColumn* entry = m_columns[column];
            if (entry && static_cast<unsigned int>(row) < entry->cells.size()) {
                ListBoxCell* cell = entry->cells[row];
                if (cell) alignment = cell->field064;
            }
        }
        const float used = imageX + width;
        if (getAvailableWidth(column, alignment) < used) break;
    }
    *fontMode = mode;
    return index;
}
// 659EE0, a shared one-byte setter the HUD builder's outline row calls directly.
void BfListBoxData::setOutlineForHud(bool outline) { m_outline = outline; }
} }
#endif
