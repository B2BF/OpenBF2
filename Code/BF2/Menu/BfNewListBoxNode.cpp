#include "BfNewListBoxNode.h"
#if !DEDICATED
#include "BfListBoxData.h"
#include "DifFontMetadata.h"
#include "Font.h"
#include "../External/BFMeme/Action.h"
#include "../External/BFMeme/Effect.h"
#include "../External/BFMeme/Style.h"
#include "../External/BFMeme/System.h"
#include "../External/BFMeme/TypeDescriptor.h"
#include "../IO/Input/Input.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#if !defined(_M_IX86)
#include <emmintrin.h>
#endif
namespace dice { namespace hfe { namespace io {
extern IBase* inputDeviceManager;
} } }
namespace dice { namespace meme {
namespace {
// Floors then converts to int with truncating x87 control, preserving NaN and out-of-range results.
int floorToInt(double value)
{
    const double floored = std::floor(value);
#if defined(_M_IX86)
    unsigned short original, truncating;
    int result;
    __asm {
        fnstcw original
        mov ax, original
        or ax, 0C00h
        mov truncating, ax
        fldcw truncating
        fld floored
        fistp result
        fldcw original
    }
    return result;
#else
    // cvttsd2si truncates like the chopped fistp, including the 0x80000000 indefinite result.
    return _mm_cvttsd_si32(_mm_set_sd(floored));
#endif
}
int floorFloatToInt(float value) { return floorToInt(static_cast<double>(value)); }
}

BfNewListBoxNode::BfNewListBoxNode()
    : Node(), m_firstRow(0), m_rowCount(0), m_visibleRows(0), m_data(nullptr),
      m_rowHeight(12), m_spacing(1), m_borderWidth(1), m_scrollWidth(4),
      m_field028(0), m_field02c(0), m_field030(0), m_selectAction(nullptr),
      m_field04c(nullptr), m_focusAction(nullptr), m_field054(nullptr), m_field058(0),
      m_field05c(0), m_field060(0), m_field064(-1), m_sound068(0), m_sound06c(0),
      m_sound070(0), m_field074(false), m_field078(0), m_field07c(false),
      m_field080(-1), m_pendingEvents(), m_width(0), m_selectable(false),
      m_border(true), m_top(2), m_bottom(2), m_left(2), m_right(2),
      m_background{0,0,0,0}, m_frame{1,1,1,0},
      m_selection{0,0.1875f,0.5389999747276306f,0.8f}, m_scrollbar{1,1,1,1},
      m_scrollOffset(0), m_field0fc(0), m_field100(0), m_field104(0),
      m_field108(false), m_field109(false), m_field128(false)
{
    for (auto& word : m_field10c) word = 0;
    for (FontHandle& font : m_fonts) font = 0;
}
BfNewListBoxNode::~BfNewListBoxNode() {} // list destruction, then Node/Object
unsigned int BfNewListBoxNode::getObjectSize() { return sizeof(BfNewListBoxNode); }
bool BfNewListBoxNode::enqueueEvent(int type)
{
    m_pendingEvents.push_back(type);
    return true;
}
TypeDescriptor* BfNewListBoxNode::getClassType()          // 7C3320, singletonA22244
{
    static ClassTypeDescriptor<BfNewListBoxNode, Node> type("dice::meme::BfNewListBoxNode");
    return &type;
}
TypeDescriptor* BfNewListBoxNode::getType() { return getClassType(); }

// 7C39B0, the single added slot at +C8. Void return, no guard.
void BfNewListBoxNode::setSelectableForHud(bool selectable) { m_selectable = selectable; }

// The System argument is carried by the queued-event caller but is not
// read. A successful row change can enqueue event 7 into the same live list.
bool BfNewListBoxNode::selectPreviousRow(System*)
{
    const int row = m_data->m_row;
    if (row > 0) {
        const int previous = row - 1;
        m_data->selectRow(previous, true);
        if (previous < m_firstRow) --m_firstRow;
    }
    return true;
}

// The sound precedes the selection callback and the first-row update.
bool BfNewListBoxNode::selectNextRow(System* system, const Rect* bounds)
{
    const int row = m_data->m_row;
    if (row >= m_data->getElementCount(system) - 1) return true;

    system->playSound(m_sound068, 1.0f);
    const int next = row + 1;
    m_data->selectRow(next, true);
    const float step = m_spacing + m_rowHeight;
    if ((bounds->height - (step + step)) / step
            + static_cast<float>(m_firstRow) - 1.0f <= static_cast<float>(next))
        ++m_firstRow;
    return true;
}

// Retail repeats getElementCount when the clamped arm is selected.
bool BfNewListBoxNode::advanceVisiblePage(System* system)
{
    const int next = m_firstRow + m_visibleRows;
    if (next > m_data->getElementCount(system) - m_visibleRows)
        m_firstRow = m_data->getElementCount(system) - m_visibleRows;
    else
        m_firstRow = next;
    return true;
}

// Scrolls one row down, clamped by the stored row count. The System argument is unused.
bool BfNewListBoxNode::scrollDownOneRow(System*)
{
    const int limit = m_rowCount - m_visibleRows;
    ++m_firstRow;
    if (m_firstRow > limit) m_firstRow = limit;
    return true;
}

// [FRONTIER] [TEMP] [CHOSEN, not recovered] Remaining stream and event methods stop on invocation.
// [RETIRE-WHEN: hud-list-box-frame] Replace after all remaining list-box stream and event operations are implemented and validated.
bool BfNewListBoxNode::streamProperties(System*, Stream*)
{
    std::fprintf(stderr, "[FRONTIER] dice::meme::BfNewListBoxNode::streamProperties reached\n");
    std::fflush(stderr);
    std::abort();
}
bool BfNewListBoxNode::updateSelf(System* system, const NodeState* state,
                                  const Rect* bounds, float delta)
{
    m_field058 += delta;
    m_width = bounds->width;
    if (m_field064 != -1 && m_field058 - m_field060 > 1.0f) m_field064 = -1;
    if (!m_data) return true;

    if (m_data->m_row == -1 && m_data->getElementCount(system) > 0)
        m_data->selectRow(0, true);

    // Iteration remains live: row-selection callbacks may append event 7 and
    // retail processes that new node before it reaches the list sentinel.
    for (std::list<int>::iterator it = m_pendingEvents.begin();
         it != m_pendingEvents.end(); ++it) {
        switch (*it) {
        case 1:
            selectPreviousRow(system);
            break;
        case 2:
            selectNextRow(system, bounds);
            break;
        case 3:
            --m_firstRow;
            if (m_firstRow < 0) m_firstRow = 0;
            break;
        case 4: {
            ++m_firstRow;
            const int last = m_rowCount - m_visibleRows;
            if (m_firstRow > last) m_firstRow = last;
            break;
        }
        case 5:
            m_firstRow -= m_visibleRows;
            if (m_firstRow < 0) m_firstRow = 0;
            break;
        case 6:
            advanceVisiblePage(system);
            break;
        case 7:
            if (m_selectable) {
                const NodeInputEvent event = {1, 0, 0.0f, 0.0f, 0};
                if (m_focusAction)
                    m_focusAction->execute(system, bounds, state, &event);
            }
            break;
        case 8:
            m_field128 = true;
            break;
        default:
            break;
        }
    }
    m_pendingEvents.clear();

    if (m_field074) {
        const float* pointer = system->getPointerPosition();
        const float pointerY = pointer[1];
        if (m_selectable) system->setPointerOwner(this);

        m_field078 += delta;
        if (m_field078 > 0.25f) {
            hfe::io::IInputDevice* mouse =
                static_cast<hfe::io::IInputDeviceManager*>(hfe::io::inputDeviceManager)
                    ->getInputDevice(hfe::io::IDFMouse);
            if (mouse && !static_cast<hfe::io::IAdvancedInputDevice*>(mouse)->buttonDown(
                    hfe::io::IDButton_0))
                m_field074 = false;
            m_field078 = 0.0f;
        }
        if (!m_field074) return true;

        double distance = std::fabs(static_cast<double>(m_field028))
            - std::fabs(static_cast<double>(pointerY));
        const float step = (bounds->height + bounds->y + m_borderWidth)
            / static_cast<float>(m_rowCount) * m_data->m_field030;
        if (!(std::fabs(distance) > static_cast<double>(step)) || !(step > 0.0f))
            return true;
        if (distance < 0.0) distance = -distance;

        if (pointerY > m_field028) {
            int row = m_firstRow;
            const int last = m_rowCount - m_visibleRows;
            do {
                ++row;
                if (row > last) row = last;
                distance -= step;
            } while (distance > step);
            m_firstRow = row;
        } else {
            int row = m_firstRow;
            do {
                --row;
                if (row < 0) row = 0;
                distance -= step;
            } while (distance > step);
            m_firstRow = row;
        }
        m_field028 = pointerY;
        return true;
    }

    if (m_field07c) {
        const float pointerX = system->getPointerPosition()[0];
        const int distance = floorToInt(std::fabs(
            static_cast<double>(m_field028) - static_cast<double>(pointerX)));
        if (m_selectable) system->setPointerOwner(this);

        if (distance) {
            const bool shrinking = pointerX < m_field028 && pointerX > bounds->x;
            for (int index = m_field080; index < m_data->m_columnCount; ++index) {
                m_data->selectActiveColumnForNode(static_cast<unsigned int>(index));
                ListBoxColumn* column = m_data->m_activeColumn;
                const int width = column ? column->width : 0;
                const int trailing = column ? column->field01c : 0;
                if (bounds->width - static_cast<float>(trailing) - m_scrollWidth * 3.0f
                        > static_cast<float>(width + distance)) {
                    column = m_data->m_activeColumn;
                    if (column)
                        column->width = shrinking ? width - distance : width + distance;
                }
            }
        }
        m_field028 = pointerX;
        return true;
    }

    const float* pointer = system->getPointerPosition();
    const float pointerX = pointer[0];
    const float pointerY = pointer[1];
    if (!(pointerX > bounds->x && pointerX < bounds->x + bounds->width
          && pointerY > bounds->y && pointerY < bounds->y + bounds->height)) {
        m_field108 = false;
        return true;
    }

    if (m_selectable) system->setPointerOwner(this);
    if (pointerX == m_field100 && pointerY == m_field104) {
        m_field0fc += delta;
    } else {
        m_field100 = pointerX;
        m_field104 = pointerY;
        m_field0fc = 0.0f;
    }
    if (!(m_field0fc > 1.0f)) {
        m_field108 = false;
        return true;
    }

    const float rowPosition = static_cast<float>(
        (std::fabs(static_cast<double>(bounds->y) - static_cast<double>(pointerY))
            - static_cast<double>(m_rowHeight)) / static_cast<double>(m_rowHeight));
    m_field10c[0] = m_firstRow + floorFloatToInt(rowPosition);

    int columnIndex = 1;
    if (m_data->m_columnCount >= 1) {
        for (;;) {
            m_data->selectActiveColumnForNode(static_cast<unsigned int>(columnIndex));
            ListBoxColumn* column = m_data->m_activeColumn;
            int edge = column ? column->width : 0;
            if (!column || !column->width) edge = floorFloatToInt(m_width);

            if (static_cast<double>(edge)
                    > std::fabs(static_cast<double>(bounds->x)) + pointerX) {
                m_field10c[1] = columnIndex - 1;
                break;
            }
            if (++columnIndex > m_data->m_columnCount) {
                m_field108 = true;
                return true;
            }
        }
    }
    m_field108 = true;
    return true;
}
// This is the complete retail painter. Several behaviours that look
// accidental are observable and intentionally retained: the background is
// unconditional, highlighted rows are submitted twice, missing cells retain
// the previous cell colour, wrapped rows mutate every column while painting,
// and the pending-row upper clamp can make m_firstRow negative.
void BfNewListBoxNode::paintSelf(System* system, const NodeState*, const Rect* bounds,
                                 const Rect* clip, RenderContext* context)
{
    const auto drawSolid = [&](float x, float y, float width, float height, float z,
                               const Color& color, int texture) {
        const Rect rectangle = {x, y, width, height, z};
        Quad quad(&rectangle, clip, &color);
        drawContextTexture(context, system, texture, &quad);
    };
    const auto setPlainQuad = [&](Quad& quad, float left, float top,
                                  float right, float bottom, float z,
                                  const Color& color) {
        quad.clip = clip;
        quad.corners[0].x = left;
        quad.corners[0].y = top;
        quad.corners[0].z = z;
        quad.corners[0].color = color;
        quad.corners[1].x = right;
        quad.corners[1].y = top;
        quad.corners[1].z = z;
        quad.corners[1].color = color;
        quad.corners[2].x = right;
        quad.corners[2].y = bottom;
        quad.corners[2].z = z;
        quad.corners[2].color = color;
        quad.corners[3].x = left;
        quad.corners[3].y = bottom;
        quad.corners[3].z = z;
        quad.corners[3].color = color;
    };

    Quad background;
    setPlainQuad(background, bounds->x, bounds->y,
                 bounds->x + bounds->width, bounds->y + bounds->height,
                 0.0f, m_background);
    drawContextTexture(context, system, 0, &background);

    if (m_border) {
        Quad frame;
        setPlainQuad(frame, bounds->x, bounds->y,
                     bounds->x + bounds->width,
                     bounds->y + static_cast<float>(m_top), 0.0f, m_frame);
        drawContextTexture(context, system, 0, &frame);
        setPlainQuad(frame,
                     bounds->x + bounds->width - static_cast<float>(m_right),
                     bounds->y + static_cast<float>(m_top),
                     bounds->x + bounds->width, bounds->y + bounds->height,
                     0.0f, m_frame);
        drawContextTexture(context, system, 0, &frame);
        setPlainQuad(frame, bounds->x,
                     bounds->y + bounds->height - static_cast<float>(m_bottom),
                     bounds->x + bounds->width - static_cast<float>(m_right),
                     bounds->y + bounds->height, 0.0f, m_frame);
        drawContextTexture(context, system, 0, &frame);
        setPlainQuad(frame, bounds->x, bounds->y + static_cast<float>(m_top),
                     bounds->x + static_cast<float>(m_left),
                     bounds->y + bounds->height, 0.0f, m_frame);
        drawContextTexture(context, system, 0, &frame);
    }

    if (!m_data) return;

    const auto columnAt = [&](int index) -> ListBoxColumn* {
        return static_cast<unsigned int>(index) < m_data->m_columns.size()
            ? m_data->m_columns[index] : nullptr;
    };
    const auto cellAt = [&](int column, int row) -> ListBoxCell* {
        ListBoxColumn* const entry = columnAt(column);
        return entry && static_cast<unsigned int>(row) < entry->cells.size()
            ? entry->cells[row] : nullptr;
    };

    m_visibleRows = 0;
    Color currentColor = {1.0f, 1.0f, 1.0f, 1.0f};
    m_rowCount = m_data->getElementCount(system);
    const int selectedRow = m_data->m_row;

    hfe::menu::Font* const firstFont =
        reinterpret_cast<hfe::menu::Font*>(m_fonts[0]);
    const std::string firstFontName(firstFont->getName()); // retail has no null guard
    float fontScale = 1.0f;
    if (firstFontName.find(".dif", 0, 4) != std::string::npos)
        fontScale = static_cast<hfe::menu::DifFont*>(firstFont)
            ->getMetadata()->getScale();
    const float reciprocalScale = static_cast<float>(1.0 / fontScale);

    int configuredRows = m_data->m_visibleRows;
    if (m_rowCount <= configuredRows) configuredRows = m_rowCount;
    std::list<int> highlightedRows;

    for (int columnIndex = 0; columnIndex < m_data->m_columnCount; ++columnIndex) {
        float rowY = bounds->y + static_cast<float>(m_top) + m_spacing;
        for (int row = m_firstRow; row < m_firstRow + configuredRows; ++row) {
            ListBoxCell* const firstColumnCell = cellAt(0, row);
            if (firstColumnCell && firstColumnCell->field06c
                    && std::find(highlightedRows.begin(), highlightedRows.end(), row)
                       == highlightedRows.end()) {
                highlightedRows.push_back(row);
                const float left = bounds->x + static_cast<float>(m_left)
                    + firstColumnCell->highlightField084;
                const float top = rowY + firstColumnCell->highlightField088;
                Quad highlight;
                setPlainQuad(highlight, left, top,
                             bounds->x + bounds->width - static_cast<float>(m_right),
                             top + firstColumnCell->highlightField080,
                             0.0f, firstColumnCell->highlight);
                drawContextTexture(context, system, 0, &highlight);
                drawContextTexture(context, system, 0, &highlight); // shipped duplicate
            }

            m_data->selectActiveColumnForNode(static_cast<unsigned int>(columnIndex));
            ListBoxColumn* const activeColumn = m_data->m_activeColumn;
            const float lastVisibleY = bounds->y + bounds->height
                + static_cast<float>(m_top - m_bottom) - m_rowHeight;
            if (rowY > lastVisibleY || !activeColumn || !activeColumn->enabled)
                continue;

            ListBoxCell* cell = cellAt(columnIndex, row);
            if (cell) currentColor = cell->color;

            int characters[256]; // retail's fixed stack buffer; tooltip use is also unbounded
            int characterIndex = 0;
            int fontMode = cell ? cell->field090 : 0;
            const int initialFontMode = fontMode;
            float measuredWidth = 0.0f;
            float maximumBottom = 0.0f;
            float minimumVerticalOffset = 10.0f;

            int character = m_data->getCharacterElement(system, row, characterIndex);
            characters[characterIndex] = character;
            while (character) {
                const int next = m_data->getCharacterElement(system, row, characterIndex + 1);
                characters[characterIndex + 1] = next;
                int controlLength = 0;
                const bool sentinel = character == 0xA7 || character == -89;
                if (sentinel && next >= '0' && next <= '3') {
                    fontMode = next - '0';
                    controlLength = 2;
                }
                if (sentinel && (next == 'C' || next == 'c')) {
                    characterIndex += 6;
                } else if (controlLength) {
                    characterIndex += controlLength;
                } else {
                    GlyphMetrics metrics; // failure leaves the shipped output uninitialized
                    system->getGlyphMetrics(character, m_fonts[fontMode], &metrics);
                    measuredWidth = static_cast<float>(
                        ((static_cast<double>(metrics.rightBearing) + metrics.width)
                            + metrics.leftBearing) + measuredWidth);
                    const float bottom = metrics.height + metrics.verticalOffset;
                    if (bottom > maximumBottom) maximumBottom = bottom;
                    if (metrics.verticalOffset < minimumVerticalOffset)
                        minimumVerticalOffset = metrics.verticalOffset;

                    ++characterIndex;
                    if (characterIndex >= 250) break;
                    if (activeColumn->field01c < measuredWidth)
                        activeColumn->field01c = floorFloatToInt(measuredWidth);

                    cell = cellAt(columnIndex, row);
                    const float imageX = cell ? cell->imageX : 0.0f;
                    const int alignment = cell ? cell->field064 : 0;
                    if (static_cast<float>(m_data->getAvailableWidth(alignment))
                            < imageX + measuredWidth) {
                        if (!activeColumn->field01a) break;

                        cell = cellAt(columnIndex, row);
                        if (cell) {
                            const Color wrappedColor = cell->color;
                            if (cell->isWide) {
                                std::wstring suffix(cell->wide);
                                suffix.erase(0, static_cast<std::wstring::size_type>(characterIndex));
                                cell->wide.erase(static_cast<std::wstring::size_type>(characterIndex),
                                                 cell->wide.size() - characterIndex);
                                for (int later = columnIndex + 1;
                                     later < m_data->m_columnCount; ++later)
                                    m_data->insertCell(later, row, std::string(),
                                                       1.0f, 1.0f, 1.0f, 1.0f);
                                for (int earlier = 0; earlier < columnIndex; ++earlier)
                                    m_data->insertCell(earlier, row + 1, std::string(),
                                                       1.0f, 1.0f, 1.0f, 1.0f);
                                m_data->insertWideCell(columnIndex, row + 1, suffix,
                                    wrappedColor.r, wrappedColor.g,
                                    wrappedColor.b, wrappedColor.a);
                            } else {
                                std::string suffix(cell->narrow);
                                suffix.erase(0, static_cast<std::string::size_type>(characterIndex));
                                cell->narrow.erase(static_cast<std::string::size_type>(characterIndex),
                                                   cell->narrow.size() - characterIndex);
                                for (int later = columnIndex + 1;
                                     later < m_data->m_columnCount; ++later)
                                    m_data->insertCell(later, row, std::string(),
                                                       1.0f, 1.0f, 1.0f, 1.0f);
                                for (int earlier = 0; earlier < columnIndex; ++earlier)
                                    m_data->insertCell(earlier, row + 1, std::string(),
                                                       1.0f, 1.0f, 1.0f, 1.0f);
                                m_data->insertCell(columnIndex, row + 1, suffix,
                                    wrappedColor.r, wrappedColor.g,
                                    wrappedColor.b, wrappedColor.a);
                            }
                        }
                    }
                }

                character = m_data->getCharacterElement(system, row, characterIndex);
                characters[characterIndex] = character;
            }
            characters[characterIndex] = 0;
            characters[characterIndex + 1] = 0;
            fontMode = initialFontMode;

            if (selectedRow == row && columnIndex == 0 && m_selectable) {
                drawSolid(bounds->x + static_cast<float>(m_left), rowY,
                          bounds->width - static_cast<float>(m_left + m_right),
                          m_rowHeight, 0.0f, m_selection, 0);
            }

            cell = cellAt(columnIndex, row);
            if (cell && cell->isImage) {
                const Rect imageBounds = {
                    bounds->x + static_cast<float>(activeColumn->width) + cell->imageX,
                    rowY + cell->imageY,
                    cell->imageWidth,
                    cell->imageHeight,
                    bounds->z
                };
                Quad image(&imageBounds, clip, &currentColor);
                drawContextTexture(context, system, cell->texture, &image);
            } else {
                const int alignment = cell ? cell->field064 : 0;
                const float imageX = cell ? cell->imageX : 0.0f;
                float cursorX = bounds->x;
                if (alignment == 0) {
                    cursorX = bounds->x + static_cast<float>(activeColumn->width) + imageX;
                } else if (alignment == 1) {
                    cursorX = bounds->x + static_cast<float>(activeColumn->width) + imageX
                        + (static_cast<float>(m_data->getColumnRightEdge(columnIndex + 1)
                                              - activeColumn->width) * 0.5f)
                        - measuredWidth * 0.5f;
                } else if (alignment == 2) {
                    cursorX = bounds->x
                        + static_cast<float>(m_data->getColumnRightEdge(columnIndex + 1))
                        - imageX - measuredWidth;
                }

                float alignedY = rowY;
                const int verticalAlignment = cell ? cell->field068 : 1;
                if (verticalAlignment == 1) {
                    alignedY = rowY
                        + (m_rowHeight - (maximumBottom - minimumVerticalOffset)) * 0.5f;
                } else if (verticalAlignment == 3) {
                    minimumVerticalOffset = 0.0f;
                } else if (verticalAlignment == 4) {
                    alignedY = rowY + m_rowHeight;
                }
                cursorX = static_cast<float>(std::floor(static_cast<double>(cursorX)));
                alignedY = static_cast<float>(std::floor(
                    static_cast<double>(alignedY) + m_data->m_field024 + m_data->m_field024));

                bool alternateColor = false;
                int bufferIndex = 0;
                int glyphElementIndex = 0;
                while (characters[bufferIndex]) {
                    character = characters[bufferIndex];
                    const int next = characters[bufferIndex + 1];
                    int controlLength = 0;
                    const bool sentinel = character == 0xA7 || character == -89;
                    if (sentinel && next >= '0' && next <= '3') {
                        fontMode = next - '0';
                        controlLength = 2;
                    }
                    if (sentinel && (next == 'C' || next == 'c')) {
                        alternateColor = !alternateColor;
                        bufferIndex += 6;
                        continue;
                    }
                    if (controlLength) {
                        bufferIndex += controlLength;
                        continue;
                    }

                    if (alternateColor) {
                        currentColor = {0.9960784316f, 0.6392157078f,
                                        0.1686274558f, 1.0f};
                    } else {
                        cell = cellAt(columnIndex, row);
                        if (cell) currentColor = cell->color;
                    }

                    GlyphMetrics metrics; // retail ignores failure
                    system->getGlyphMetrics(character, m_fonts[fontMode], &metrics);
                    cursorX = static_cast<float>(
                        static_cast<double>(cursorX) + metrics.leftBearing);
                    float glyphX = cursorX;
                    float glyphY;
                    if (fontScale == 1.0f) {
                        glyphY = alignedY + metrics.verticalOffset - minimumVerticalOffset;
                    } else {
                        const float scaledX = static_cast<float>(
                            static_cast<double>(glyphX) * fontScale);
                        const double roundedX = std::floor(
                            !(std::fmod(scaledX, 1.0) >= 0.5)
                                ? static_cast<double>(scaledX)
                                : static_cast<double>(scaledX) + 1.0);
                        glyphX = static_cast<float>(roundedX * reciprocalScale);
                        cursorX = glyphX;

                        const float scaledY = static_cast<float>(
                            static_cast<double>(alignedY) * fontScale);
                        const double roundedY = std::floor(
                            !(std::fmod(scaledY, 1.0) >= 0.5)
                                ? static_cast<double>(scaledY)
                                : static_cast<double>(scaledY) + 1.0);
                        glyphY = static_cast<float>(
                            roundedY * reciprocalScale + metrics.verticalOffset);
                    }

                    const Rect glyphBounds = {
                        glyphX, glyphY, metrics.width, metrics.height, bounds->z
                    };
                    if (m_data->m_outline) {
                        const Color outlineColor = {
                            0.15000001f, 0.15000001f, 0.15000001f, currentColor.a
                        };
                        Quad outline(&glyphBounds, clip, &outlineColor);
                        outline.state08 = m_data->m_field034;
                        outline.state0c = m_data->m_field038;
                        outline.state10 = m_data->m_field03c;
                        outline.state18 = m_data->m_field040;
                        translateQuad(&outline, 1.0f, 1.0f, -0.001f);
                        drawContextCharacter(context, system, character, m_fonts[fontMode],
                                             glyphElementIndex, &outline);
                        setQuadBounds(&outline, glyphX + 1.0f, glyphY,
                                      metrics.width, metrics.height);
                        drawContextCharacter(context, system, character, m_fonts[fontMode],
                                             glyphElementIndex, &outline);
                        setQuadBounds(&outline, glyphX + 1.0f, glyphY - 1.0f,
                                      metrics.width, metrics.height);
                        drawContextCharacter(context, system, character, m_fonts[fontMode],
                                             glyphElementIndex, &outline);
                        setQuadBounds(&outline, glyphX, glyphY - 1.0f,
                                      metrics.width, metrics.height);
                        drawContextCharacter(context, system, character, m_fonts[fontMode],
                                             glyphElementIndex, &outline);
                        setQuadBounds(&outline, glyphX - 1.0f, glyphY - 1.0f,
                                      metrics.width, metrics.height);
                        drawContextCharacter(context, system, character, m_fonts[fontMode],
                                             glyphElementIndex, &outline);
                        setQuadBounds(&outline, glyphX - 1.0f, glyphY,
                                      metrics.width, metrics.height);
                        drawContextCharacter(context, system, character, m_fonts[fontMode],
                                             glyphElementIndex, &outline);
                        setQuadBounds(&outline, glyphX - 1.0f, glyphY + 1.0f,
                                      metrics.width, metrics.height);
                        drawContextCharacter(context, system, character, m_fonts[fontMode],
                                             glyphElementIndex, &outline);
                        setQuadBounds(&outline, glyphX, glyphY + 1.0f,
                                      metrics.width, metrics.height);
                        drawContextCharacter(context, system, character, m_fonts[fontMode],
                                             glyphElementIndex, &outline);
                    }

                    Quad glyph(&glyphBounds, clip, &currentColor);
                    glyph.state08 = m_data->m_field034;
                    glyph.state0c = m_data->m_field038;
                    glyph.state10 = m_data->m_field03c;
                    glyph.state18 = m_data->m_field040;
                    drawContextCharacter(context, system, character, m_fonts[fontMode],
                                         glyphElementIndex, &glyph);

                    cursorX = static_cast<float>(
                        (static_cast<double>(metrics.rightBearing) + metrics.width) + cursorX);
                    if (m_data->m_outline) cursorX += 1.0f;
                    ++bufferIndex;
                    ++glyphElementIndex;
                }
            }

            if (columnIndex == 0) ++m_visibleRows;
            rowY += m_spacing + m_rowHeight;
        }
    }

    if (m_data->m_resource) {
        // [FRONTIER] [TEMP] [CHOSEN, not recovered] Resource finalization requires an unidentified interface prefix.
        std::fprintf(stderr, "[FRONTIER] dice::meme::BfNewListBoxNode::paintSelf "
                             "resource-finalization dependency reached\n");
        std::fflush(stderr);
        std::abort();
    }

    if (m_rowCount > m_visibleRows && m_data->m_field021) {
        const float doubledStep = (m_spacing + m_rowHeight) * 2.0f;
        float thumbHeight = (bounds->height - static_cast<float>(m_top + m_bottom)
            - doubledStep - m_borderWidth * 2.0f)
            * (static_cast<float>(m_visibleRows) / static_cast<float>(m_rowCount));
        m_field030 = thumbHeight;
        if (thumbHeight < 4.0f) thumbHeight = 4.0f;
        m_field030 = thumbHeight;
        m_field02c = (static_cast<float>(m_firstRow)
                / static_cast<float>(m_rowCount - m_visibleRows))
            * (bounds->height - (thumbHeight + m_borderWidth * 2.0f
                                 + static_cast<float>(m_top + m_bottom)))
            + bounds->y + static_cast<float>(m_top) + m_borderWidth;

        const float scrollX = bounds->x + bounds->width
            - (m_scrollWidth + m_scrollOffset);
        drawSolid(scrollX, bounds->y + static_cast<float>(m_top), m_scrollWidth,
                  bounds->height - static_cast<float>(m_top + m_bottom),
                  0.0f, m_scrollbarBackground, 0);
        drawSolid(scrollX, m_field02c, m_scrollWidth, m_field030,
                  0.0f, m_scrollbar, 0);
    }

    if (m_field07c) {
        m_data->selectActiveColumnForNode(static_cast<unsigned int>(m_field080));
        const int width = m_data->m_activeColumn ? m_data->m_activeColumn->width : 0;
        const float right = bounds->x + static_cast<float>(width + 10);
        const float top = bounds->y + m_rowHeight + 2.0f;
        const float bottom = bounds->y + bounds->height - m_rowHeight;
        const Color white = {1.0f, 1.0f, 1.0f, 1.0f};
        Quad resizeLine;
        setPlainQuad(resizeLine, right - 1.0f, top, right, bottom,
                     0.0f, white);
        drawContextTexture(context, system, 0, &resizeLine);
    }

    if (m_field108 && m_field109) {
        const int tooltipRow = m_field10c[0];
        const int tooltipColumn = m_field10c[1];
        ListBoxCell* const tooltipCell = cellAt(tooltipColumn, tooltipRow);
        if (!tooltipCell || !tooltipCell->isImage) {
            int tooltipCharacters[256]; // shipped loop has no 250-character cap
            int count = 0;
            float tooltipWidth = 0.0f;
            m_data->selectActiveColumnForNode(static_cast<unsigned int>(tooltipColumn));
            int tooltipCharacter = m_data->getCharacterElement(system, tooltipRow, 0);
            tooltipCharacters[0] = tooltipCharacter;
            while (tooltipCharacter) {
                GlyphMetrics metrics; // retail ignores failure
                system->getGlyphMetrics(tooltipCharacter, m_fonts[0], &metrics);
                tooltipWidth = static_cast<float>(
                    ((static_cast<double>(metrics.rightBearing) + metrics.width)
                        + metrics.leftBearing) + tooltipWidth);
                tooltipCharacter = m_data->getCharacterElement(system, tooltipRow, ++count);
                tooltipCharacters[count] = tooltipCharacter;
            }
            tooltipCharacters[count] = 0;

            const int alignment = tooltipCell ? tooltipCell->field064 : 0;
            if (static_cast<float>(m_data->getAvailableWidth(alignment)) < tooltipWidth) {
                const float* const pointer = system->getPointerPosition();
                const Color black = {0.0f, 0.0f, 0.0f, 1.0f};
                const Color gray = {0.8f, 0.8f, 0.8f, 1.0f};
                const float boxY = pointer[1] + 20.0f;
                drawSolid(pointer[0] - 1.0f, boxY - 1.0f,
                          tooltipWidth + 5.0f, m_rowHeight + 1.0f,
                          0.0f, black, 0);
                drawSolid(pointer[0], boxY, tooltipWidth + 3.0f, m_rowHeight - 2.0f,
                          0.0f, gray, 0);

                const Color textColor = {0.2f, 0.2f, 0.2f, 1.0f};
                float tooltipX = pointer[0] + 2.0f;
                for (int index = 0; tooltipCharacters[index]; ++index) {
                    GlyphMetrics metrics; // retail ignores failure
                    system->getGlyphMetrics(tooltipCharacters[index], m_fonts[0], &metrics);
                    const float glyphX = static_cast<float>(
                        static_cast<double>(tooltipX) + metrics.leftBearing);
                    const Rect glyphBounds = {
                        glyphX,
                        boxY + metrics.verticalOffset + m_rowHeight - 4.0f,
                        metrics.width,
                        metrics.height,
                        bounds->z
                    };
                    Quad glyph(&glyphBounds, clip, &textColor);
                    drawContextCharacter(context, system, tooltipCharacters[index],
                                         m_fonts[0], index, &glyph);
                    tooltipX = static_cast<float>(
                        (static_cast<double>(metrics.rightBearing) + metrics.width)
                            + glyphX);
                }
            }
        }
    }

    if (m_field128) {
        m_field128 = false;
        int pending = m_data->m_field028;
        if (pending < 0) pending = 0;
        const int last = m_rowCount - m_visibleRows;
        if (pending > last) pending = last;
        m_firstRow = pending;
    }
}
// Handles pointer and keyboard input for the list box.
bool BfNewListBoxNode::onEvent1(System* system, const Rect* bounds, const NodeState* state,
                                int first, int, int)
{
    if (!m_data) return false;                                  // 7C40AC, the only guard

    switch (first) {
    case 6:
        selectPreviousRow(system);                              // inlined at 7C40BD; result dropped
        return false;
    case 8:
        selectNextRow(system, bounds);
        return false;
    case 18:
        return false;                                           // 7C4112, swallowed
    case 19:
        --m_firstRow;                                           // 7C411D, stored before the clamp
        if (m_firstRow < 0) m_firstRow = 0;
        return false;
    case 20:
        scrollDownOneRow(system);
        return false;
    case 11:
        m_firstRow -= m_visibleRows;
        if (m_firstRow < 0) m_firstRow = 0;
        return false;
    case 12:
        advanceVisiblePage(system);
        return false;
    default:
        break;
    }
    if (first != 1 && first != 21) return false;

    // the pointer arms ----------------------------------------------------------------------
    const float* const pointer = system->getPointerPosition();
    const float px = pointer[0];
    const float py = pointer[1];

    // The four edges, in the order retail computes them. m_left/m_top/m_bottom are integers and
    // every one of them is widened with fild at the point of use.
    const float rightEdge   = bounds->width + bounds->x;
    const float scrollLeft  = rightEdge - (m_scrollWidth + m_scrollOffset);
    const float leftInset   = bounds->x - static_cast<float>(m_left);
    const float topInset    = static_cast<float>(m_top) + bounds->y;
    const float bottomInset = (bounds->height + bounds->y)
                            - static_cast<float>(m_bottom);

    // the scrollbar strip on the right --------------------------------------------------
    // Every test here is an ordered comparison written the way the shipped `fcomp`/`fnstsw`
    // pairs read it, so a NaN coordinate falls out of the strip rather than into it.
    if (px >= scrollLeft && (rightEdge - m_scrollOffset) >= px
            && py <= bottomInset && py >= topInset
            && system->getPointerOwner() == this)                                   // +C8, 7C4242
    {
        // +2C is the thumb's top and +30 its height, which is what these three arms settle:
        // inside the thumb starts a drag, below it pages down, above it pages up.
        if (py >= m_field02c && (m_field030 + m_field02c) >= py) {
            m_field074 = true;                                  // 7C4272, "dragging the thumb"
            m_field028 = py;                                    // 7C4276, the grab point
            return false;
        }
        if (py >= m_field02c) {                                 // 7C4282, below the thumb
            advanceVisiblePage(system);
            return false;
        }
        if ((m_field030 + m_field02c) >= py) {                  // 7C42A2, above the thumb
            m_firstRow -= m_visibleRows;
            if (m_firstRow < 0) m_firstRow = 0;
        }
        return false;
    }

    // the list body -------------------------------------------------------------------------
    // These are the strict twins of the strip's tests, not their negations: the strip uses >= and
    // <=, the body > and <. A pointer exactly on an edge belongs to neither.
    if (px > leftInset && (scrollLeft - m_scrollOffset) > px
            && py > topInset && py < bottomInset
            && m_selectable && system->getPointerOwner() == this)
    {
        m_field060 = m_field058;                                // 7C4342, before anything else

        // The row under the pointer, measured down from the top inset in whole row pitches.
        // `fabs` on the distance, so a pointer above the inset maps to the same row as one the
        // same distance below it. Shipped.
        const int rowOffset = static_cast<int>(std::floor(static_cast<double>(
            std::fabs(static_cast<float>(m_top) + bounds->y - py)
                / (m_spacing + m_rowHeight))));

        // Select the first column whose stored width covers the pointer distance.
        // [UNRESOLVED] Whether width represents an individual width or a cumulative right edge.
        int column = 0;
        for (int index = 1; index <= m_data->m_columnCount; ++index) {
            m_data->selectActiveColumnForNode(index);
            ListBoxColumn* const active = m_data->m_activeColumn;
            int width = active ? active->width : 0;
            if (!active || active->width == 0)                  // 7C43B3, the fallback
                width = static_cast<int>(std::floor(static_cast<double>(m_width)));
            if (static_cast<float>(width) > std::fabs(bounds->x) + px) {
                column = index - 1;
                break;
            }
        }

        // A row past the end takes the column down with it: both become -1.
        int row = m_firstRow + rowOffset;
        int selectedColumn = column;
        if (row >= m_data->getRowCount()) {
            row = -1;
            selectedColumn = -1;
        }
        if (m_field064 == -1) m_field064 = row;                 // 7C442C, latched once, -1 included
        m_data->setColumn(selectedColumn);

        if (first == 21) {
            // The release. It selects with notify false and then runs the action itself.
            m_data->selectRow(row, false);
            if (m_field04c) {
                const NodeInputEvent event = {1, 0, 0.0f, 0.0f, 0};
                m_field04c->execute(system, bounds, state, &event);
            }
        } else {
            m_data->selectRow(row, true);                       // +80, 7C4496, notify true
        }

        // The sound is chosen by activateSelection's answer, and it plays on both arms - the
        // "refused" one is a different handle, not silence.
        system->playSound(m_data->activateSelection(system) ? m_sound070 : m_sound06c,
                          1.0f);                                // +78 then +A4, 7C44A5/7C44B9
        return false;
    }

    // the pointer is somewhere else ---------------------------------------------------------
    // Reached by a press outside the node, by one inside a node that is not selectable, and by one
    // inside a node that does not own the pointer.
    if (m_field054) {
        const NodeInputEvent event = {1, 0, 0.0f, 0.0f, 0};
        m_field054->execute(system, bounds, state, &event);
    }
    return false;
}
namespace {
// `return state->current < 0.0f;`. A one-line predicate on the NodeState, taken
// through ECX like a member. [INFERRED] name from its single caller, onEvent2's last gate.
bool nodeStateCurrentIsNegative(const NodeState* state)
{
    return state->current < 0.0f;
}
}

// Handles release input for the list box.
bool BfNewListBoxNode::onEvent2(System* system, const Rect* bounds, const NodeState* state,
                                int first, int)
{
    m_field074 = false;                                     // 7C4541, the drag flag
    m_field07c = false;
    m_field080 = -1;

    if (first != 1 && first != 21) return true;
    if (!m_selectable) return true;

    const float* const pointer = system->getPointerPosition();
    const float px = pointer[0];
    const float py = pointer[1];

    const float scrollLeft = (bounds->width + bounds->x) - (m_scrollWidth + m_scrollOffset);
    if (!(px < scrollLeft - m_scrollOffset)) return true;

    const float topInset = static_cast<float>(m_top) + bounds->y;
    if (!(topInset < py)) return true;

    const float bottomInset = (bounds->height + bounds->y) - static_cast<float>(m_bottom);
    if (!(bottomInset > py)) return true;

    if (system->getPointerOwner() != this) return true;                          // +C8, 7C45DA

    const int row = m_data->m_row;
    if (row < 0) return true;
    if (m_data->getRowCount() <= 0) return true;
    if (row > m_data->getRowCount()) return true;                                // 7C4607, `>` not `>=`

    // The click-timing gate. +58 is the time the press started and +5C/+60 are its two marks; the
    // three tests together say "this release belongs to the press that is still open".
    // The middle one is an equality against 0.0 written as `fucompp` + `test ah,44h` + `jnp`,
    // which passes when the two are not equal - including when either is a NaN.
    if (!(m_field058 - m_field060 < 1.0f)) { m_field05c = m_field058; return true; }
    if (m_field05c == 0.0f)                { m_field05c = m_field058; return true; }
    if (!(m_field058 - m_field05c < 1.0f)) { m_field05c = m_field058; return true; }
    if (!nodeStateCurrentIsNegative(state)) { m_field05c = m_field058; return true; }
    if (m_field064 != row)                 { m_field05c = m_field058; return true; }

    // The click is complete. Only input code 1 runs the action; 21 reaches here and falls
    // straight into the reset below.
    if (first == 1 && m_selectAction) {
        const NodeInputEvent event = {1, 0, 0.0f, 0.0f, 0};
        m_selectAction->execute(system, bounds, state, &event);                  // +0x40
    }
    m_field060 = 0.0f;
    m_field05c = 0.0f;
    m_field058 = 0.0f;
    m_field064 = -1;
    return false;
}
bool BfNewListBoxNode::onEvent3(System*, const Rect*, const NodeState*, int, int)
{
    std::fprintf(stderr, "[FRONTIER] dice::meme::BfNewListBoxNode::onEvent3 reached\n");
    std::fflush(stderr);
    std::abort();
}
#if defined(_M_IX86)
static_assert(sizeof(BfNewListBoxNode) == 296, "native BfNewListBoxNode storage");
#endif
} }
#endif
