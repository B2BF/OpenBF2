// Builds HUD nodes and binds their properties to game data. Some Node descriptor operations remain unresolved.
#include "Bf2HudBuilder.h"
#if !DEDICATED
#include "Bf2SplitNode.h"
#include "Bf2CullNode.h"
#include "Bf2PictureNode.h"
#include "Bf2BarNode.h"
#include "Bf2CompassNode.h"
#include "Bf2BlendFuncEffect.h"
#include "Bf2ColorEffect.h"
#include "Bf2EffectNode.h"
#include "Bf2HoverNode.h"
#include "Bf2ButtonNode.h"
#include "Bf2NewListBoxNode.h"
#include "Bf2EditNode.h"
#include "Bf2MiniMenuMapNode.h"
#include "Bf2TransformListNode.h"
#include "Bf2FixedSliderNode.h"
#include "BfEditData.h"
#include "BfListBoxData.h"
#include "Bf2CallFunctionAction.h"
#include "Bf2ActionListAction.h"
#include "Bf2TextNode.h"
#include "Bf2Style.h"
#include "BF2HudManager.h"
#include "Bf2MenuMapNode.h"
#include "Bf2MoveEffect.h"
#include "BfOccupiedVehicleData.h"
#include "BfOccupiedVehicleNode.h"
#include "Bf2ObjectMarkerNode.h"
#include "Bf2ObjectSelectionNode.h"
#include "Bf2TransformNode.h"
#include "../Core/Debug.h"
#include "../Core/StringUtil.h"
#include "../Game/HudInformationLayer.h"
#include "../Game/Main/BF2ClientSeams.h"
#include "../External/BFMeme/Function.h"
#include "../External/BFMeme/AndData.h"
#include "../External/BFMeme/Data.h"
#include "../External/BFMeme/EqualData.h"
#include "../External/BFMeme/FloatData.h"
#include "../External/BFMeme/IntData.h"
#include "../External/BFMeme/NotData.h"
#include "../External/BFMeme/OrData.h"
#include "../External/BFMeme/ShowEffectNode.h"
#include "../External/BFMeme/AlphaFadeEffect.h"
#include "../External/BFMeme/System.h"
#include "../External/BFMeme/Node.h"
#include "../External/BFMeme/TypeDescriptor.h"
#include <new>
namespace dice { namespace hfe { namespace hud {
namespace {
const char* const builderSource =
    "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Menu\\Bf2HudBuilder.cpp";

// PE794F10 (Node) /794ED0 (Object). Source helper spelling [INFERRED].
// The requested descriptor precedes the object's virtual getType. Do not use
// isDerivedFrom here: that helper repeats getBaseType, unlike these casts.
// PE794D00..794D35, table932800,18 slots. A dice::meme::Function that calls one
// of the information layer's registered string functions with a fixed argument.
// [INFERRED] name: the class is local to this translation unit and has no
// descriptor of its own, so its getType still answers "dice::meme::Function".
class HudStringFunction : public meme::Function {
public:
    HudStringFunction(HudInformationLayer::StringFunction function,
                      const char* name, std::string text)
        : meme::Function(name), m_function(function), m_text(text) {}
    ~HudStringFunction() override {}
    // 82E480, the +40 override. The System the base body would print to
    // is accepted and ignored; only the stored text reaches the function.
    bool call(meme::System*) override { return m_function(m_text); }
    // 794D40, the added +44 slot - a tail jump straight into operator=. Nothing
    // in this image calls it; [INFERRED] void return.
    virtual void setText(const std::string& text) { m_text = text; }
private:
    HudInformationLayer::StringFunction m_function;         // +08
    std::string m_text;                                     // +0C
};

template<class T> T* builderCast(meme::Object* object)
{
    if (!object) return nullptr;
    meme::TypeDescriptor* requested = T::getClassType();
    meme::TypeDescriptor* type = object->getType();
    while (type) {
        if (type == requested) return static_cast<T*>(object);
        type = type->getBaseType();
    }
    return nullptr;
}

// [INFERRED] shared spelling for the identical nested allocations at79A548,
// 79AF49 and79B159. The outer allocation precedes the BoolRefData allocation
// and the name concatenation; neither allocation-null arm gains a guard.
meme::Bf2CullNode* createVisibilityNode(const std::string& name, bool* value)
{
    void* allocation = ::operator new(sizeof(meme::Bf2CullNode));
    if (!allocation) return nullptr;
    void* dataAllocation = ::operator new(sizeof(meme::BoolRefData));
    meme::BoolRefData* data = dataAllocation
        ? new(dataAllocation) meme::BoolRefData(value, nullptr) : nullptr;
    return new(allocation) meme::Bf2CullNode((name + "CullNode").c_str(), data);
}

// [INFERRED] shared spelling for the six identical operand allocations inside
// Each repeats one shape: allocate, construct on success, and leave a
// null object on failure. The shipped callers then dispatch through that
// possibly null pointer without a guard - do not add one here or at the call.
template<class T> T* allocateLogicalOperand()
{
    void* allocation = ::operator new(sizeof(T));
    return allocation ? new(allocation) T(nullptr) : nullptr;
}
meme::IntData* allocateIntConstant(int value)
{
    void* allocation = ::operator new(sizeof(meme::IntData));
    return allocation ? new(allocation) meme::IntData(value, nullptr) : nullptr;
}
meme::IntRefData* allocateIntReference(int* value)
{
    void* allocation = ::operator new(sizeof(meme::IntRefData));
    return allocation ? new(allocation) meme::IntRefData(value, nullptr) : nullptr;
}
meme::BoolRefData* allocateBoolReference(bool* value)
{
    void* allocation = ::operator new(sizeof(meme::BoolRefData));
    return allocation ? new(allocation) meme::BoolRefData(value, nullptr) : nullptr;
}
}

Bf2HudBuilder::Bf2HudBuilder()
    : m_system(nullptr), m_field008(0), m_field00c(0), m_field010(0), m_field014(0),
      m_field018("NULL"), m_field034("NULL"), m_field050("NULL"), m_field06c("NULL"),
      m_field088(true)
{
    // Retail+89..9B, including four trailing words, remain untouched.
}
Bf2HudBuilder::~Bf2HudBuilder()
{
    // Four strings destroy in reverse declaration order. No handle/System
    // ownership, registration, node traversal or cleanup occurs in794E90.
}
void Bf2HudBuilder::setSystem(meme::System* system)
{
    m_system = system;
}

// PE79C420..79C54D. Lookup makes another pair of by-value copies before
// allocation. The allocation-null arm still calls registerNode with null.
bool Bf2HudBuilder::createSplitNode(std::string parent, std::string name)
{
    if (findNodePredecessor(parent, name)) {
        Debug(DebugTypeError, builderSource, 667, "")
            << "Node: " << name << ", already exists!!";
        return false;
    }
    void* allocation = ::operator new(sizeof(meme::Bf2SplitNode));
    meme::Bf2SplitNode* node = allocation
        ? new(allocation) meme::Bf2SplitNode(name.c_str()) : nullptr;
    return registerNode(parent, node, nullptr, 2);
}

// Creates and registers a map node.
bool Bf2HudBuilder::createMapNode(std::string parent, std::string name)
{
    if (!m_system) return false;
    void* allocation = ::operator new(sizeof(meme::Bf2MenuMapNode));
    meme::Bf2MenuMapNode* map = allocation
        ? new(allocation) meme::Bf2MenuMapNode(m_system, name.c_str(), 0, 0, 0, 0) : nullptr;
    m_field008 = map;
    allocation = ::operator new(sizeof(meme::Bf2TransformNode));
    meme::Bf2TransformNode* transform = allocation
        ? new(allocation) meme::Bf2TransformNode(parent.c_str(), 0, 0, 800, 600) : nullptr;
    transform->setChild(1, m_field008);
    return registerNode(parent, transform, m_field008, 0);
}

// Creates a picture inside a named transform node and registers it beneath parent.
// The transform uses name; its picture child uses the PictureNode suffix.
bool Bf2HudBuilder::createPictureNode(std::string parent, std::string name,
                                     int x, int y, int width, int height)
{
    if (findNodePredecessor(parent, name)) {
        Debug(DebugTypeError, builderSource, 1091, "")
            << "Node: " << name << ", already exists!!";
        return false;
    }
    void* allocation = ::operator new(sizeof(meme::Bf2PictureNode));
    meme::Bf2PictureNode* picture = nullptr;
    if (allocation) {
        const std::string pictureName = name + "PictureNode";
        picture = new(allocation) meme::Bf2PictureNode(pictureName.c_str());
    }
    allocation = ::operator new(sizeof(meme::Bf2TransformNode));
    meme::Bf2TransformNode* transform = allocation
        ? new(allocation) meme::Bf2TransformNode(name.c_str(), x, y, width, height)
        : nullptr;
    transform->setChild(1, picture);
    return registerNode(parent, transform, picture, 0);
}

// PE797C30..797CF6. The selected registry object is cast to the picture class;
// a miss reports and returns false. The argument's own text pointer is handed
// to the node, which compares it by identity against its bound name.
bool Bf2HudBuilder::setPictureNodeTexture(std::string texture)
{
    meme::Bf2PictureNode* picture = builderCast<meme::Bf2PictureNode>(m_field014);
    if (picture) return picture->setTextureNameForHud(texture.c_str());
    Debug(DebugTypeError, builderSource, 1101, "")
        << "Failed setting texture on node: " << m_field018;
    return false;
}

// PE797DF0..797EE1. The HUD string variable supplies a pointer the node keeps;
// an unknown name falls through to the same diagnostic as a wrong node type.
bool Bf2HudBuilder::setPictureNodeVariableTexture(std::string variable)
{
    meme::Bf2PictureNode* picture = builderCast<meme::Bf2PictureNode>(m_field014);
    if (picture) {
        const char** binding = static_cast<HudInformationLayer*>(g_hudInformationLayer)
            ->getStringByName(std::string(variable.c_str()));
        if (binding) return picture->setTextureNameBindingForHud(binding);
    }
    Debug(DebugTypeError, builderSource, 1132, "")
        << "Failed setting variable texture on node: " << m_field018;
    return false;
}

// PE798160..798251; the float-variable twin of the texture binding above.
bool Bf2HudBuilder::setPictureNodeRotateVariable(std::string variable)
{
    meme::Bf2PictureNode* picture = builderCast<meme::Bf2PictureNode>(m_field014);
    if (picture) {
        float* binding = static_cast<HudInformationLayer*>(g_hudInformationLayer)
            ->getFloatByName(std::string(variable.c_str()));
        if (binding) return picture->setRotationBindingForHud(binding);
    }
    Debug(DebugTypeError, builderSource, 1180, "")
        << "Failed setting rotate variable on node: " << m_field018;
    return false;
}

// PE798260..79830B. The console integer becomes the node's degree rotation.
bool Bf2HudBuilder::setPictureNodeRotation(int degrees)
{
    meme::Bf2PictureNode* picture = builderCast<meme::Bf2PictureNode>(m_field014);
    if (picture) return picture->setAdditionalRotationForHud(static_cast<float>(degrees));
    Debug(DebugTypeError, builderSource, 1190, "")
        << "Failed setting rotation on node: " << m_field018;
    return false;
}


bool Bf2HudBuilder::setPictureNodeCenterPoint(int x, int y)
{
    meme::Bf2PictureNode* picture = builderCast<meme::Bf2PictureNode>(m_field014);
    if (picture) return picture->setCenterPointForHud(x, y);
    Debug(DebugTypeError, builderSource, 1165, "")
        << "Failed setting centerpoint on node: " << m_field018;
    return false;
}

// PE797EF0..797FE1. The console order is top/bottom/left/right, and the node
// takes left/right/top/bottom. The named-object lookup before the call has no
// use for its result; it is in the shipped body, so it stays.
bool Bf2HudBuilder::setPictureNodeBorder(int top, int bottom, int left, int right)
{
    meme::Bf2PictureNode* picture = builderCast<meme::Bf2PictureNode>(m_field014);
    if (picture) {
        m_system->findNamedObject(
            formatString("%sColorEffect", m_field050.c_str()).c_str());
        return picture->setBorderForHud(left, right, top, bottom);
    }
    // Retail reuses the variable-texture text here; do not "correct" it.
    Debug(DebugTypeError, builderSource, 1145, "")
        << "Failed setting variable texture on node: " << m_field018;
    return false;
}

// PE797FF0..7980A1; the same borrowed diagnostic text as the border row.
bool Bf2HudBuilder::setPictureNodeBorderColor(float r, float g, float b, float a)
{
    meme::Bf2PictureNode* picture = builderCast<meme::Bf2PictureNode>(m_field014);
    if (picture) return picture->setBorderColorForHud(r, g, b, a);
    Debug(DebugTypeError, builderSource, 1155, "")
        << "Failed setting variable texture on node: " << m_field018;
    return false;
}

// PE79D7D0..79D992 (460 B). Same shape as createPictureNode: a Bf2BarNode named
// "<name>BarNode" carrying the direction, then a Bf2TransformNode named after the
// second argument, then registerNode with the bar as the data object.
bool Bf2HudBuilder::createBarNode(std::string parent, std::string name, int direction,
                                 int x, int y, int width, int height)
{
    if (findNodePredecessor(parent, name)) {
        Debug(DebugTypeError, builderSource, 1584, "")
            << "Node: " << name << ", already exists!!";
        return false;
    }
    void* allocation = ::operator new(sizeof(meme::Bf2BarNode));
    meme::Bf2BarNode* bar = nullptr;
    if (allocation) {
        const std::string barName = name + "BarNode";
        bar = new(allocation) meme::Bf2BarNode(barName.c_str(), direction);
    }
    allocation = ::operator new(sizeof(meme::Bf2TransformNode));
    meme::Bf2TransformNode* transform = allocation
        ? new(allocation) meme::Bf2TransformNode(name.c_str(), x, y, width, height)
        : nullptr;
    transform->setChild(1, bar);
    return registerNode(parent, transform, bar, 0);
}

// PE799590..79965A. Index1 is the bar's own picture texture, anything else the
// fill texture. The compass row forwards straight into this body.
bool Bf2HudBuilder::setBarNodeTexture(int index, std::string texture)
{
    meme::Bf2BarNode* bar = builderCast<meme::Bf2BarNode>(m_field014);
    if (bar) return bar->setIndexedTextureNameForHud(index, texture.c_str());
    Debug(DebugTypeError, builderSource, 1594, "")
        << "Failed setting texture on node: " << m_field018;
    return false;
}

// PE799660..799751. Unlike the other rows this one checks the System first and
// refuses an empty variable name before the cast.
bool Bf2HudBuilder::setBarNodeVariableTexture(int index, std::string variable)
{
    if (m_system && variable != "") {
        meme::Bf2BarNode* bar = builderCast<meme::Bf2BarNode>(m_field014);
        if (bar) {
            const char** binding = static_cast<HudInformationLayer*>(g_hudInformationLayer)
                ->getStringByName(std::string(variable.c_str()));
            if (binding) return bar->setIndexedTextureBindingForHud(index, binding);
        }
    }
    Debug(DebugTypeError, builderSource, 1611, "")
        << "Failed setting variable texture on node: " << m_field018;
    return false;
}

// PE799780..799831. The console order is top/bottom/left/right and the node
// stores it unchanged. The compass row is a straight jump into this body.
bool Bf2HudBuilder::setBarNodeBorder(int top, int bottom, int left, int right)
{
    meme::Bf2BarNode* bar = builderCast<meme::Bf2BarNode>(m_field014);
    if (bar) return bar->setFillBorderForHud(top, bottom, left, right);
    Debug(DebugTypeError, builderSource, 1621, "")
        << "Failed setting border on node: " << m_field018;
    return false;
}


bool Bf2HudBuilder::setBarNodeSnap(int snap)
{
    meme::Bf2BarNode* bar = builderCast<meme::Bf2BarNode>(m_field014);
    if (bar) return bar->setSnapForHud(snap);
    Debug(DebugTypeError, builderSource, 1631, "")
        << "Failed setting snap on node: " << m_field018;
    return false;
}


bool Bf2HudBuilder::setBarNodeSnapDir(bool towardsEnd)
{
    meme::Bf2BarNode* bar = builderCast<meme::Bf2BarNode>(m_field014);
    if (bar) return bar->setSnapDirectionForHud(towardsEnd);
    Debug(DebugTypeError, builderSource, 1641, "")
        << "Failed setting snap direction on node: " << m_field018;
    return false;
}

// PE7999A0..799A97. The float variable is bound and range-checked by the node,
// which zeroes an out-of-range value in place and answers false.
bool Bf2HudBuilder::setBarNodeValueVariable(std::string variable)
{
    meme::Bf2BarNode* bar = builderCast<meme::Bf2BarNode>(m_field014);
    if (bar) {
        float* binding = static_cast<HudInformationLayer*>(g_hudInformationLayer)
            ->getFloatByName(std::string(variable.c_str()));
        if (binding) return bar->setValueBindingForHud(binding);
    }
    Debug(DebugTypeError, builderSource, 1655, "")
        << "Failed setting variable on node: " << m_field018;
    return false;
}

// PE79D9A0..79DB86 (468 B). The compass takes the direction and the two flags;
// the transform still takes the rectangle. Note the compass node is also named
// "<name>BarNode", not "<name>CompassNode".
bool Bf2HudBuilder::createCompassNode(std::string parent, std::string name, int direction,
                                     int x, int y, int width, int height,
                                     bool snap, bool wrap)
{
    if (findNodePredecessor(parent, name)) {
        Debug(DebugTypeError, builderSource, 1670, "")
            << "Node: " << name << ", already exists!!";
        return false;
    }
    void* allocation = ::operator new(sizeof(meme::Bf2CompassNode));
    meme::Bf2CompassNode* compass = nullptr;
    if (allocation) {
        const std::string compassName = name + "BarNode";
        compass = new(allocation) meme::Bf2CompassNode(compassName.c_str(), direction, snap, wrap);
    }
    allocation = ::operator new(sizeof(meme::Bf2TransformNode));
    meme::Bf2TransformNode* transform = allocation
        ? new(allocation) meme::Bf2TransformNode(name.c_str(), x, y, width, height)
        : nullptr;
    transform->setChild(1, compass);
    return registerNode(parent, transform, compass, 0);
}

// PE799AA0..799AD1, 799B20 and 799CE0: three compass rows are pure forwards into
// the bar bodies, so they also report the bar rows' line numbers.
bool Bf2HudBuilder::setCompassNodeTexture(int index, std::string texture)
{ return setBarNodeTexture(index, texture); }
bool Bf2HudBuilder::setCompassNodeBorder(int top, int bottom, int left, int right)
{ return setBarNodeBorder(top, bottom, left, right); }
bool Bf2HudBuilder::setCompassNodeValueVariable(std::string variable)
{ return setBarNodeValueVariable(variable); }


bool Bf2HudBuilder::setCompassNodeSnapOffset(int a, int b, int c, int d)
{
    meme::Bf2CompassNode* compass = builderCast<meme::Bf2CompassNode>(m_field014);
    if (compass) return compass->setSnapOffsetForHud(a, b, c, d);
    Debug(DebugTypeError, builderSource, 1698, "")
        << "Failed setting offset on node: " << m_field018;
    return false;
}

// PE799BF0..799CD1. The texture is loaded before the node is resolved, and a
// zero handle is refused as firmly as a wrong node type.
bool Bf2HudBuilder::setCompassNodeSnapTexture(bool primary, std::string texture)
{
    if (m_system) {
        const meme::TextureHandle handle = m_system->loadTexture(texture.c_str(), true);
        if (handle) {
            meme::Bf2CompassNode* compass = builderCast<meme::Bf2CompassNode>(m_field014);
            if (compass) return compass->setSnapTextureForHud(primary, handle);
        }
    }
    Debug(DebugTypeError, builderSource, 1715, "")
        << "Failed setting snap texture on node:" << m_field018;
    return false;
}

// PE799D10..799DB1; the same diagnostic text as the snap-offset row.
bool Bf2HudBuilder::setCompassNodeOffset(int offset)
{
    meme::Bf2CompassNode* compass = builderCast<meme::Bf2CompassNode>(m_field014);
    if (compass) return compass->setOffsetForHud(offset);
    Debug(DebugTypeError, builderSource, 1731, "")
        << "Failed setting offset on node: " << m_field018;
    return false;
}

// PE799DC0..799E71. Both console integers become floats before the call.
bool Bf2HudBuilder::setCompassNodeTextureSize(int width, int height)
{
    meme::Bf2CompassNode* compass = builderCast<meme::Bf2CompassNode>(m_field014);
    if (compass)
        return compass->setTextureSizeForHud(static_cast<float>(width),
                                             static_cast<float>(height));
    Debug(DebugTypeError, builderSource, 1741, "")
        << "Failed setting texture size on node: " << m_field018;
    return false;
}

// PE79A440..79A69A. Resolve the HUD variable before checking System. The
// lookup uses formatted C text, whereas allocation concatenates the full name.
bool Bf2HudBuilder::setNodeShowVariable(std::string name)
{
    bool* value = static_cast<HudInformationLayer*>(g_hudInformationLayer)
        ->getBoolByName(std::string(name.c_str()));
    if (value && m_system) {
        meme::Object* existing = m_system->findNamedObject(
            formatString("%sCullNode", m_field050.c_str()).c_str());
        if (existing) {
            meme::Bf2CullNode* cull = builderCast<meme::Bf2CullNode>(existing);
            if (cull) {
                meme::Data* data = builderCast<meme::Data>(cull->getDataForHud());
                if (data) {
                    // The construction path supplies BoolRefData; bind through its typed callback after the ancestry check.
                    static_cast<meme::BoolRefData*>(data)->bind(m_system, value);
                    return true;
                }
            }
        }
        meme::Node* node = findNode(m_field050);
        if (node) {
            meme::Bf2CullNode* cull = createVisibilityNode(m_field050, value);
            cull->setChild(0, node->getChild(1));
            node->setChild(1, cull);
            return m_system->registerObject(cull, nullptr) != false;
        }
    }
    Debug(DebugTypeError, builderSource, 1875, "")
        << "Failed setting cull on node: " << m_field050;
    return false;
}

// Assigns the comparison used to control a node's visibility from a variable.
bool Bf2HudBuilder::setNodeLogicShowVariable(std::string operation, std::string variable, int value)
{
    meme::Data* operand = nullptr;
    meme::Node* node = nullptr;
    if (!m_system) goto failed;
    node = findNode(m_field050);
    if (!node) goto failed;

    if (operation == "EQUAL") {
        int* const number = static_cast<HudInformationLayer*>(g_hudInformationLayer)
            ->getIntByName(std::string(variable.c_str()));
        if (number) {
            meme::IntData* const constant = allocateIntConstant(value);
            meme::IntRefData* const reference = allocateIntReference(number);
            meme::EqualData* const equal = allocateLogicalOperand<meme::EqualData>();
            equal->setData(constant, reference);
            operand = equal;
        }
    } else if (operation == "OR") {
        int* const number = static_cast<HudInformationLayer*>(g_hudInformationLayer)
            ->getIntByName(std::string(variable.c_str()));
        if (number) {
            meme::Bf2CullNode* const cull = builderCast<meme::Bf2CullNode>(
                m_system->findNamedObject(
                    formatString("%sCullNode", m_field050.c_str()).c_str()));
            if (cull) {
                meme::Data* const existing = cull->getDataForHud();
                if (existing) {
                    meme::IntData* const constant = allocateIntConstant(value);
                    meme::IntRefData* const reference = allocateIntReference(number);
                    meme::EqualData* const equal = allocateLogicalOperand<meme::EqualData>();
                    equal->setData(constant, reference);
                    meme::OrData* const disjunction = allocateLogicalOperand<meme::OrData>();
                    disjunction->setData(equal, existing);
                    return cull->setDataForHud(disjunction);
                }
            }
        } else {
            bool* const flag = static_cast<HudInformationLayer*>(g_hudInformationLayer)
                ->getBoolByName(std::string(variable.c_str()));
            if (flag) {
                meme::BoolRefData* const reference = allocateBoolReference(flag);
                meme::OrData* const disjunction = allocateLogicalOperand<meme::OrData>();
                operand = disjunction; // survives the lookup failure, as in retail
                meme::Bf2CullNode* const cull = builderCast<meme::Bf2CullNode>(
                    m_system->findNamedObject(
                        formatString("%sCullNode", m_field050.c_str()).c_str()));
                if (cull) {
                    meme::Data* const existing = cull->getDataForHud();
                    if (existing) {
                        if (value == 0) {
                            meme::NotData* const negation =
                                allocateLogicalOperand<meme::NotData>();
                            negation->setData(reference, nullptr);
                            disjunction->setData(existing, negation);
                        } else {
                            disjunction->setData(existing, reference);
                        }
                        return cull->setDataForHud(disjunction);
                    }
                }
            }
        }
    } else if (operation == "NOT") {
        bool* const flag = static_cast<HudInformationLayer*>(g_hudInformationLayer)
            ->getBoolByName(std::string(variable.c_str()));
        if (flag) {
            meme::BoolRefData* const reference = allocateBoolReference(flag);
            meme::NotData* const negation = allocateLogicalOperand<meme::NotData>();
            negation->setData(reference, nullptr);
            operand = negation;
        } else {
            int* const number = static_cast<HudInformationLayer*>(g_hudInformationLayer)
                ->getIntByName(std::string(variable.c_str()));
            if (number) {
                meme::IntData* const constant = allocateIntConstant(value);
                meme::IntRefData* const reference = allocateIntReference(number);
                meme::EqualData* const equal = allocateLogicalOperand<meme::EqualData>();
                equal->setData(constant, reference);
                meme::NotData* const negation = allocateLogicalOperand<meme::NotData>();
                negation->setData(equal, nullptr);
                operand = negation;
            }
        }
    } else if (operation == "AND") {
        bool* const flag = static_cast<HudInformationLayer*>(g_hudInformationLayer)
            ->getBoolByName(std::string(variable.c_str()));
        if (flag) {
            meme::BoolRefData* const reference = allocateBoolReference(flag);
            meme::AndData* const conjunction = allocateLogicalOperand<meme::AndData>();
            operand = conjunction; // allocated before the lookup; see the note above
            meme::Bf2CullNode* const cull = builderCast<meme::Bf2CullNode>(
                m_system->findNamedObject(
                    formatString("%sCullNode", m_field050.c_str()).c_str()));
            if (cull) {
                meme::Data* const existing = cull->getDataForHud();
                if (existing) {
                    if (value == 0) {
                        meme::NotData* const negation =
                            allocateLogicalOperand<meme::NotData>();
                        negation->setData(reference, nullptr);
                        conjunction->setData(existing, negation);
                    } else {
                        conjunction->setData(existing, reference);
                    }
                    return cull->setDataForHud(conjunction);
                }
            }
        }
    } else {
        Debug(DebugTypeError, builderSource, 2034, "")
            << "Logical operation " << operation << " not found!";
        return false;
    }

    {
        // The concatenated name exists only while the allocation did.
        void* allocation = ::operator new(sizeof(meme::Bf2CullNode));
        meme::Bf2CullNode* cull = nullptr;
        if (allocation) {
            const std::string name = m_field050 + "CullNode";
            cull = new(allocation) meme::Bf2CullNode(name.c_str(), operand);
        }
        if (operation != "AND") {
            cull->setChild(0, node->getChild(1));
            node->setChild(1, cull);
            return m_system->registerObject(cull, nullptr) != false;
        }
    }
failed:
    Debug(DebugTypeError, builderSource, 2048, "")
        << "Failed setting logical cull on node: " << m_field050;
    return false;
}

// PE79D220..79D3F2 (510 B). Same shape as the other create rows, with two
// differences: the registered data object is the text node, the transform is
// inserted with mode 1 (not 0), and the node is given a fresh IntData(0) as its
// "type" so paintSelf's gate passes. The body also compares the parent with
// "KilledInfo" and discards the result; keep the shipped comparison.
bool Bf2HudBuilder::createTextNode(std::string parent, std::string name,
                                  int x, int y, int width, int height)
{
    const bool killedInfo = parent == "KilledInfo";
    (void)killedInfo; // retail computes this and never reads it
    if (findNodePredecessor(parent, name)) {
        Debug(DebugTypeError, builderSource, 1214, "")
            << "Node: " << name << ", already exists!!";
        return false;
    }
    void* allocation = ::operator new(sizeof(meme::Bf2TextNode));
    meme::Bf2TextNode* text = nullptr;
    if (allocation) {
        const std::string textName = name + "TextNode";
        text = new(allocation) meme::Bf2TextNode(textName.c_str());
    }
    allocation = ::operator new(sizeof(meme::Bf2TransformNode));
    meme::Bf2TransformNode* transform = allocation
        ? new(allocation) meme::Bf2TransformNode(name.c_str(), x, y, width, height)
        : nullptr;
    allocation = ::operator new(sizeof(meme::IntData));
    meme::IntData* type = allocation
        ? new(allocation) meme::IntData(0, nullptr) : nullptr;
    transform->setChild(1, text);
    text->setTypeDataForHud(type);
    return registerNode(parent, transform, text, 1);
}

// PE798310..798454 (461 B). The console text is widened, wrapped in a WstringData
// and attached, and then the same node's data is immediately overwritten with the
// HUD manager's localised text for that key. The row returns the second setter's
// result, not the attach.
bool Bf2HudBuilder::setTextNodeString(std::string text)
{
    if (m_system && text != "") {
        meme::Bf2TextNode* node = builderCast<meme::Bf2TextNode>(m_field014);
        if (node) {
            void* allocation = ::operator new(sizeof(meme::WstringData));
            meme::WstringData* data = nullptr;
            if (allocation) {
                const std::wstring wide = toWideString(std::string(text.c_str()));
                data = new(allocation) meme::WstringData(wide.c_str(), "");
            }
            node->setStringDataForHud(data);
            meme::Data* const stored = node->getStringDataForHud();
            const std::wstring localised = hudManager->getLocalizedString(text);
            return stored->setWideString(m_system, localised.c_str());
        }
    }
    Debug(DebugTypeError, builderSource, 1233, "")
        << "Failed setting Data on node: " << m_field018;
    return false;
}

// PE7984E0..798742 (629 B). The font is loaded first and cached on the builder,
// then the alignment picks one of the three style classes. An alignment outside
// 0..2 reports "Invalid style!" on line1275 with no node text; a font that will
// not load reports line1283 with the path; no System reports line1286.
bool Bf2HudBuilder::setTextNodeStyle(std::string font, int alignment)
{
    if (m_system && font != "") {
        m_field090 = m_system->loadFont(font.c_str());
        if (m_field090) {
            meme::Bf2Style* style = nullptr;
            void* allocation = nullptr;
            switch (alignment) {
            case 0:
                allocation = ::operator new(sizeof(meme::Bf2CenterAlignedStyle));
                style = allocation
                    ? new(allocation) meme::Bf2CenterAlignedStyle("", m_field090) : nullptr;
                break;
            case 1:
                allocation = ::operator new(sizeof(meme::Bf2RightAlignedStyle));
                style = allocation
                    ? new(allocation) meme::Bf2RightAlignedStyle("", m_field090) : nullptr;
                break;
            case 2:
                allocation = ::operator new(sizeof(meme::Bf2Style));
                style = allocation
                    ? new(allocation) meme::Bf2Style("", m_field090) : nullptr;
                break;
            default:
                Debug(DebugTypeError, builderSource, 1275, "") << "Invalid style!";
                return false;
            }
            meme::Bf2TextNode* node = builderCast<meme::Bf2TextNode>(m_field014);
            if (node) return node->setStyleForHud(style);
            // A missing node shares the font diagnostic below.
        }
        Debug(DebugTypeError, builderSource, 1283, "")
            << "Font: " << font << "not found!";
        return false;
    }
    Debug(DebugTypeError, builderSource, 1286, "")
        << "Failed setting Style on node: " << m_field018;
    return false;
}

// PE7988E0..7989A0. The offset reaches the node's style, not the node, and the
// row answers a hard true once it finds one - never the setter's own result.
bool Bf2HudBuilder::setTextNodeOutLineOffset(float x, float y)
{
    meme::Bf2TextNode* node = builderCast<meme::Bf2TextNode>(m_field014);
    if (node) {
        meme::Bf2Style* style = builderCast<meme::Bf2Style>(node->getStyleForHud());
        if (style) {
            style->setShadowOffset(x, y);
            return true;
        }
    }
    Debug(DebugTypeError, builderSource, 1326, "")
        << "Failed setting outline offset on node: " << m_field018;
    return false;
}

// PE7989A0..798A91 (389 B). A narrow information-layer slot becomes a
// StringRefData and a wide one a WstringRefData; neither found is the failure.
bool Bf2HudBuilder::setTextNodeStringVariable(std::string variable)
{
    meme::Bf2TextNode* node = builderCast<meme::Bf2TextNode>(m_field014);
    if (node) {
        const char** narrow = static_cast<HudInformationLayer*>(g_hudInformationLayer)
            ->getStringByName(std::string(variable.c_str()));
        if (narrow) {
            void* allocation = ::operator new(sizeof(meme::StringRefData));
            meme::Data* data = allocation
                ? new(allocation) meme::StringRefData(narrow, "") : nullptr;
            return node->setStringDataForHud(data);
        }
        const wchar_t** wide = static_cast<HudInformationLayer*>(g_hudInformationLayer)
            ->getWideStringByName(std::string(variable.c_str()));
        if (wide) {
            void* allocation = ::operator new(sizeof(meme::WstringRefData));
            meme::Data* data = allocation
                ? new(allocation) meme::WstringRefData(wide, "") : nullptr;
            return node->setStringDataForHud(data);
        }
    }
    Debug(DebugTypeError, builderSource, 1352, "")
        << "Failed setting variable (" << variable << ") on node: " << m_field018;
    return false;
}

// PE79A210..79A432 (549 B). `setNodeColor <r> <g> <b> <a>` either retints the
// Bf2ColorEffect the selected node already carries or builds one. The existing
// path returns true unconditionally - not the setter's own result - while the
// creation path returns whether the new effect node registered. Neither
// allocation-null arm is guarded afterwards.
bool Bf2HudBuilder::setNodeColor(float r, float g, float b, float a)
{
    if (m_system) {
        meme::Object* const existing = m_system->findNamedObject(
            formatString("%sColorEffect", m_field050.c_str()).c_str());
        if (existing) {
            meme::Bf2EffectNode* const carrier = builderCast<meme::Bf2EffectNode>(existing);
            if (carrier) {
                carrier->getEffectForHud()->setColor(r, g, b, a);
                return true;
            }
        }
        meme::Object* const object = m_system->findNamedObject(m_field050.c_str());
        if (object) {
            meme::Node* const node = builderCast<meme::Node>(object);
            if (node) {
                void* carrierAllocation = ::operator new(sizeof(meme::Bf2EffectNode));
                meme::Bf2EffectNode* carrier = nullptr;
                if (carrierAllocation) {
                    void* effectAllocation = ::operator new(sizeof(meme::Bf2ColorEffect));
                    meme::Bf2ColorEffect* effect = effectAllocation
                        ? new(effectAllocation) meme::Bf2ColorEffect(r, g, b, a) : nullptr;
                    const std::string name = m_field050 + "ColorEffect";
                    carrier = new(carrierAllocation) meme::Bf2EffectNode(name.c_str(), effect);
                }
                carrier->setChild(0, node->getChild(1));
                node->setChild(1, carrier);
                return m_system->registerObject(carrier, nullptr) != false;
            }
        }
    }
    Debug(DebugTypeError, builderSource, 1838, "")
        << "Failed setting color on node: " << m_field050;
    return false;
}

// PE79AEA0..79B0A2. Existing-node setter result is returned directly. Creation
// precedes the child check; failure leaves the allocation alive, as in retail.
bool Bf2HudBuilder::setNodeInTime(float seconds)
{
    if (m_system) {
        meme::Object* existing = m_system->findNamedObject(
            formatString("%sCullNode", m_field050.c_str()).c_str());
        meme::Bf2CullNode* cull = builderCast<meme::Bf2CullNode>(existing);
        if (cull) return cull->setInTimeForHud(seconds);
        meme::Node* node = findNode(m_field050);
        if (node) {
            cull = createVisibilityNode(m_field050, nullptr);
            if (node->getChild(1)) {
                cull->setChild(0, node->getChild(1));
                node->setChild(1, cull);
                cull->setInTimeForHud(seconds);
                return m_system->registerObject(cull, nullptr) != false;
            }
        }
    }
    Debug(DebugTypeError, builderSource, 2075, "")
        << "Failed setting in time on node: " << m_field050;
    return false;
}

// PE79B0B0..79B2B2; same ordering as in-time, with the distinct +D0 setter.
bool Bf2HudBuilder::setNodeOutTime(float seconds)
{
    if (m_system) {
        meme::Object* existing = m_system->findNamedObject(
            formatString("%sCullNode", m_field050.c_str()).c_str());
        meme::Bf2CullNode* cull = builderCast<meme::Bf2CullNode>(existing);
        if (cull) return cull->setOutTimeForHud(seconds);
        meme::Node* node = findNode(m_field050);
        if (node) {
            cull = createVisibilityNode(m_field050, nullptr);
            if (node->getChild(1)) {
                cull->setChild(0, node->getChild(1));
                node->setChild(1, cull);
                cull->setOutTimeForHud(seconds);
                return m_system->registerObject(cull, nullptr) != false;
            }
        }
    }
    Debug(DebugTypeError, builderSource, 2102, "")
        << "Failed setting out time on node: " << m_field050;
    return false;
}

// PE79DB80..79DD98. The effect is linked but not named or registered. The
// duplicate-name diagnostic falls through to the second error when nonfatal.
bool Bf2HudBuilder::addNodeAlphaShowEffect()
{
    if (m_system) {
        meme::Object* existing = m_system->findNamedObject(
            formatString("%sAlphaShowEffect", m_field050.c_str()).c_str());
        if (existing) {
            Debug(DebugTypeError, builderSource, 2113, "")
                << m_field050 << "already has an alphaShowEffect!";
        } else {
            meme::Object* object = m_system->findNamedObject(
                formatString("%sCullNode", m_field050.c_str()).c_str());
            if (object) {
                void* allocation = ::operator new(sizeof(meme::ShowEffectNode));
                meme::ShowEffectNode* effectNode = nullptr;
                if (allocation) {
                    void* effectAllocation = ::operator new(sizeof(meme::AlphaFadeEffect));
                    meme::AlphaFadeEffect* effect = effectAllocation
                        ? new(effectAllocation) meme::AlphaFadeEffect : nullptr;
                    effectNode = new(allocation) meme::ShowEffectNode(effect);
                }
                meme::Node* node = builderCast<meme::Node>(object);
                if (node) {
                    effectNode->setChild(0, node->getChild(0));
                    node->setChild(0, effectNode);
                    return true;
                }
            }
        }
    }
    Debug(DebugTypeError, builderSource, 2127, "")
        << "Failed setting alphaShoweffect on node: " << m_field050;
    return false;
}

// Adds an unnamed blend effect to the active tree.
// Duplicate effects report both diagnostics before returning false.
bool Bf2HudBuilder::addNodeBlendEffect(int source, int destination)
{
    if (m_system) {
        meme::Object* existing = m_system->findNamedObject(
            formatString("%sBlendEffect", m_field050.c_str()).c_str());
        if (existing) {
            Debug(DebugTypeError, builderSource, 2166, "")
                << m_field050 << "already has a BlendEffect!";
        } else {
            // Unlike the alpha variant this one falls back to the bare node when
            // there is no cull node, so an uncalled setNodeShowVariable is fine.
            meme::Object* object = m_system->findNamedObject(
                formatString("%sCullNode", m_field050.c_str()).c_str());
            if (!object) object = m_system->findNamedObject(m_field050.c_str());
            if (object) {
                meme::Node* node = builderCast<meme::Node>(object);
                if (node) {
                    void* allocation = ::operator new(sizeof(meme::EffectNode));
                    meme::EffectNode* effectNode = nullptr;
                    if (allocation) {
                        void* effectAllocation = ::operator new(sizeof(meme::Bf2BlendFuncEffect));
                        meme::Bf2BlendFuncEffect* effect = effectAllocation
                            ? new(effectAllocation) meme::Bf2BlendFuncEffect(source, destination)
                            : nullptr;
                        effectNode = new(allocation) meme::EffectNode(effect);
                    }
                    // Neither allocation-null arm gains a guard here either.
                    effectNode->setChild(0, node->getChild(0));
                    node->setChild(0, effectNode);
                    return true;
                }
            }
        }
    }
    Debug(DebugTypeError, builderSource, 2186, "")
        << "Failed setting BlendEffect on node: " << m_field050;
    return false;
}

// Binds a node's alpha to variable. Requires m_system.
// Creates a white color effect when absent and retries once after successful creation.
bool Bf2HudBuilder::setNodeAlphaVariable(std::string variable)
{
    meme::Object* object = m_system->findNamedObject(
        formatString("%sColorEffect", m_field050.c_str()).c_str());
    if (!object) {
        if (setNodeColor(1.0f, 1.0f, 1.0f, 1.0f)) return setNodeAlphaVariable(variable);
    } else {
        meme::Bf2EffectNode* carrier = builderCast<meme::Bf2EffectNode>(object);
        if (carrier) {
            meme::Bf2ColorEffect* effect =
                builderCast<meme::Bf2ColorEffect>(carrier->getEffectForHud());
            if (effect) {
                float* binding = static_cast<HudInformationLayer*>(g_hudInformationLayer)
                    ->getFloatByName(std::string(variable.c_str()));
                if (binding) return effect->setAlphaBinding(binding);
            }
        }
    }
    // The diagnostic names the node, not the variable that failed to resolve.
    Debug(DebugTypeError, builderSource, 2249, "")
        << "Failed setting alpha variable on node: " << m_field050;
    return false;
}

// Binds red, green and blue variables to the selected color effect. Requires m_system.
// All three lookups run before missing bindings are tested.
bool Bf2HudBuilder::setNodeRGBVariables(std::string red, std::string green, std::string blue)
{
    meme::Object* object = m_system->findNamedObject(
        formatString("%sColorEffect", m_field050.c_str()).c_str());
    if (!object) {
        if (setNodeColor(1.0f, 1.0f, 1.0f, 1.0f))
            return setNodeRGBVariables(red, green, blue);
    } else {
        meme::Bf2EffectNode* carrier = builderCast<meme::Bf2EffectNode>(object);
        if (carrier) {
            meme::Bf2ColorEffect* effect =
                builderCast<meme::Bf2ColorEffect>(carrier->getEffectForHud());
            if (effect) {
                float* redBinding = static_cast<HudInformationLayer*>(g_hudInformationLayer)
                    ->getFloatByName(std::string(red.c_str()));
                float* greenBinding = static_cast<HudInformationLayer*>(g_hudInformationLayer)
                    ->getFloatByName(std::string(green.c_str()));
                float* blueBinding = static_cast<HudInformationLayer*>(g_hudInformationLayer)
                    ->getFloatByName(std::string(blue.c_str()));
                if (redBinding && greenBinding && blueBinding)
                    return effect->setRgbBindings(redBinding, greenBinding, blueBinding);
            }
        }
    }
    Debug(DebugTypeError, builderSource, 2276, "")
        << "Failed setting RGB variable on node: " << m_field050;
    return false;
}

// Binds one position component to an integer variable. Requires m_system and the named node.
bool Bf2HudBuilder::setNodePosVariable(int index, std::string variable)
{
    meme::Object* object = m_system->findNamedObject(m_field050.c_str());
    if (object) {
        meme::Bf2TransformNode* node = builderCast<meme::Bf2TransformNode>(object);
        if (node) {
            int* binding = static_cast<HudInformationLayer*>(g_hudInformationLayer)
                ->getIntByName(std::string(variable.c_str()));
            if (binding) return node->setPositionBindingForHud(index, binding);
        }
    }
    Debug(DebugTypeError, builderSource, 1795, "")
        << "Failed setting position on node: " << m_field050;
    return false;
}

// PE79CCC0..79CE7B (444 B). `createObjectMarkerNode <parent> <name> <x> <y> <w> <h>`
// builds the marker as "<name>ObjectMarkerNode", wraps it in a transform named
// after the second argument and registers with mode 0. Unlike createTextNode it
// does test the System first.
bool Bf2HudBuilder::createObjectMarkerNode(std::string parent, std::string name,
                                          int x, int y, int width, int height)
{
    if (m_system && !findNodePredecessor(parent, name)) {
        void* allocation = ::operator new(sizeof(meme::Bf2ObjectMarkerNode));
        meme::Bf2ObjectMarkerNode* marker = nullptr;
        if (allocation) {
            const std::string markerName = name + "ObjectMarkerNode";
            marker = new(allocation) meme::Bf2ObjectMarkerNode(m_system, markerName.c_str(), 5);
        }
        allocation = ::operator new(sizeof(meme::Bf2TransformNode));
        meme::Bf2TransformNode* transform = allocation
            ? new(allocation) meme::Bf2TransformNode(name.c_str(), x, y, width, height)
            : nullptr;
        // Neither allocation-null arm is guarded, exactly as in createTextNode.
        transform->setChild(1, marker);
        return registerNode(parent, transform, marker, 0);
    }
    Debug(DebugTypeError, builderSource, 956, "")
        << "Node: " << name << ", already exists!!";
    return false;
}

// PE797500..7975F5 (246 B). An empty texture name is refused before the cast,
// and the handle is loaded with the menu path flag.
// There is no m_system guard: once the cast succeeds the load dispatches on
// whatever the field holds.
bool Bf2HudBuilder::setObjectMarkerNodeTexture(int index, std::string texture)
{
    if (texture != "") {
        meme::Bf2ObjectMarkerNode* marker =
            builderCast<meme::Bf2ObjectMarkerNode>(m_field014);
        if (marker)
            return marker->setTextureForHud(index,
                m_system->loadTexture(texture.c_str(), true));
    }
    Debug(DebugTypeError, builderSource, 968, "")
        << "Failed setting texture on node: " << m_field018;
    return false;
}

// PE797600..7976D0 (209 B). Both integers become floats before the slot sees
// them; the slot silently drops an index of 4 or more and still answers true.
bool Bf2HudBuilder::setObjectMarkerNodeTextureSize(int index, int width, int height)
{
    meme::Bf2ObjectMarkerNode* marker =
        builderCast<meme::Bf2ObjectMarkerNode>(m_field014);
    if (marker)
        return marker->setTextureSizeForHud(index, static_cast<float>(width),
                                            static_cast<float>(height));
    // The only row in this family whose diagnostic names the index as well.
    Debug(DebugTypeError, builderSource, 978, "")
        << "Failed setting texture " << index << " size on node: " << m_field018;
    return false;
}

// PE797870..79791C (175 B). A negative count is refused; nothing caps it.
bool Bf2HudBuilder::setObjectMarkerNodeObjects(int objects)
{
    if (objects >= 0) {
        meme::Bf2ObjectMarkerNode* marker =
            builderCast<meme::Bf2ObjectMarkerNode>(m_field014);
        if (marker) return marker->setObjectCountForHud(objects);
    }
    Debug(DebugTypeError, builderSource, 1010, "")
        << "Failed setting number of objects on node: " << m_field018;
    return false;
}

// PE797790..797864 (213 B). The prefix reaches the node by value; the slot
// registers four information-layer entries per marker slot only when enabled.
bool Bf2HudBuilder::setObjectMarkerNodeLockText(bool enabled, std::string prefix)
{
    meme::Bf2ObjectMarkerNode* marker =
        builderCast<meme::Bf2ObjectMarkerNode>(m_field014);
    if (marker) return marker->setLockTextForHud(enabled, prefix);
    Debug(DebugTypeError, builderSource, 998, "")
        << "Failed enabling/disabling lock text on node: " << m_field018;
    return false;
}


bool Bf2HudBuilder::setObjectMarkerNodeLockTextOffset(int x, int y)
{
    meme::Bf2ObjectMarkerNode* marker =
        builderCast<meme::Bf2ObjectMarkerNode>(m_field014);
    if (marker) return marker->setLockTextOffsetForHud(x, y);
    Debug(DebugTypeError, builderSource, 988, "")
        << "Failed setting lock text offset on node: " << m_field018;
    return false;
}

// PE7979D0..797AB5 (230 B). The named node must already be registered and be a
// Bf2TransformNode; the marker keeps a borrowed pointer to it.
bool Bf2HudBuilder::addObjectMarkerNodeLockTextNode(std::string name)
{
    meme::Bf2ObjectMarkerNode* marker =
        builderCast<meme::Bf2ObjectMarkerNode>(m_field014);
    if (marker) {
        meme::Object* object = m_system->findNamedObject(name.c_str());
        if (object) {
            meme::Bf2TransformNode* node = builderCast<meme::Bf2TransformNode>(object);
            if (node) return marker->addLockTextNodeForHud(node);
        }
    }
    Debug(DebugTypeError, builderSource, 1047, "")
        << "Failed adding node to: " << m_field018;
    return false;
}

// PE797AC0..797B98 (168 B). No range test of any kind on the type.
bool Bf2HudBuilder::setObjectMarkerNodeLockOnType(int type)
{
    meme::Bf2ObjectMarkerNode* marker =
        builderCast<meme::Bf2ObjectMarkerNode>(m_field014);
    if (marker) return marker->setLockOnTypeForHud(type);
    Debug(DebugTypeError, builderSource, 1079, "")
        << "Failed setting lock option on node: " << m_field018;
    return false;
}

// PE797920..7979C8 (175 B). A negative weapon index is refused, as in the
// object-count row.
bool Bf2HudBuilder::setObjectMarkerNodeWeapon(int weapon)
{
    if (weapon >= 0) {
        meme::Bf2ObjectMarkerNode* marker =
            builderCast<meme::Bf2ObjectMarkerNode>(m_field014);
        if (marker) return marker->setWeaponForHud(weapon);
    }
    Debug(DebugTypeError, builderSource, 1022, "")
        << "Failed setting active weapon on node: " << m_field018;
    return false;
}

// PE79C2E0..79C41F (320 B). `createTransformNode <parent> <name> <x> <y> <w> <h>` is the
// plainest create row in the family: one Bf2TransformNode named after the second
// argument, registered under the parent with no data object and mode 2 - the same
// mode createSplitNode uses. There is no System test, and the allocation-null arm
// still calls registerNode with a null node.
// The console row declares an INT return type, and its invoker stores the
// zero-extended bool in an int slot; the builder body itself answers bool.
bool Bf2HudBuilder::createTransformNode(std::string parent, std::string name,
                                       int x, int y, int width, int height)
{
    if (findNodePredecessor(parent, name)) {
        Debug(DebugTypeError, builderSource, 655, "")
            << "Node: " << name << ", already exists!!";
        return false;
    }
    void* allocation = ::operator new(sizeof(meme::Bf2TransformNode));
    meme::Bf2TransformNode* transform = allocation
        ? new(allocation) meme::Bf2TransformNode(name.c_str(), x, y, width, height)
        : nullptr;
    return registerNode(parent, transform, nullptr, 2);
}

// Adds a show effect with pixel displacement and an angle in radians.
bool Bf2HudBuilder::addNodeMoveShowEffect(float direction, int length)
{
    if (m_system) {
        meme::Object* existing = m_system->findNamedObject(
            formatString("%sMoveEffect", m_field050.c_str()).c_str());
        if (existing) {
            Debug(DebugTypeError, builderSource, 2138, "")
                << m_field050 << "already has a MoveShowEffect!";
        } else {
            meme::Object* object = m_system->findNamedObject(
                formatString("%sCullNode", m_field050.c_str()).c_str());
            if (object) {
                void* allocation = ::operator new(sizeof(meme::FloatData));
                meme::FloatData* directionData = allocation
                    ? new(allocation) meme::FloatData(direction, nullptr) : nullptr;
                allocation = ::operator new(sizeof(meme::IntData));
                meme::IntData* lengthData = allocation
                    ? new(allocation) meme::IntData(length, nullptr) : nullptr;
                allocation = ::operator new(sizeof(meme::ShowEffectNode));
                meme::ShowEffectNode* effectNode = nullptr;
                if (allocation) {
                    void* effectAllocation = ::operator new(sizeof(meme::Bf2MoveEffect));
                    meme::Bf2MoveEffect* effect = effectAllocation
                        ? new(effectAllocation) meme::Bf2MoveEffect(directionData, lengthData)
                        : nullptr;
                    effectNode = new(allocation) meme::ShowEffectNode(effect);
                }
                meme::Node* node = builderCast<meme::Node>(object);
                if (node) {
                    effectNode->setChild(0, node->getChild(0));
                    node->setChild(0, effectNode);
                    return true;
                }
            }
        }
    }
    Debug(DebugTypeError, builderSource, 2155, "")
        << "Failed setting MoveShowEffect on node: " << m_field050;
    return false;
}

// PE79D420..79D54E (461 B). `createOccupiedNode <parent> <name> <seat> <x> <y> <w> <h>` builds a
// BfOccupiedVehicleNode named "<name>OccupiedNode" carrying the seat index and the icon size, wraps
// it in a Bf2TransformNode named after the second argument as child 1, and registers with mode 0.
// The third console argument is the seat, not a coordinate: the transform takes arguments 4..7
// as its geometry while the node takes 3, 6 and 7. There is no System test, and neither
// allocation-null arm is guarded afterwards.
bool Bf2HudBuilder::createOccupiedNode(std::string parent, std::string name, int seat,
                                      int x, int y, int width, int height)
{
    if (findNodePredecessor(parent, name)) {
        Debug(DebugTypeError, builderSource, 1368, "")
            << "Node: " << name << ", already exists!!";
        return false;
    }
    void* allocation = ::operator new(sizeof(meme::BfOccupiedVehicleNode));
    meme::BfOccupiedVehicleNode* occupied = nullptr;
    if (allocation) {
        const std::string occupiedName = name + "OccupiedNode";
        occupied = new(allocation) meme::BfOccupiedVehicleNode(occupiedName.c_str(),
                                                              seat, width, height);
    }
    allocation = ::operator new(sizeof(meme::Bf2TransformNode));
    meme::Bf2TransformNode* transform = allocation
        ? new(allocation) meme::Bf2TransformNode(name.c_str(), x, y, width, height)
        : nullptr;
    transform->setChild(1, occupied);
    return registerNode(parent, transform, occupied, 0);
}

// PE798C50..798D4?. (198 B). The console integer is a selector into the HUD
// manager's own vehicle-occupancy data, not a handle: the row asks the manager
// for the matching BfOccupiedVehicleData and hands the node that object.
bool Bf2HudBuilder::setOccupiedNodeData(int selector)
{
    if (m_system) {
        meme::BfOccupiedVehicleData* data = hudManager->getOccupiedVehicleData(selector);
        if (data) {
            meme::BfOccupiedVehicleNode* node =
                builderCast<meme::BfOccupiedVehicleNode>(m_field014);
            if (node) return node->setDataForHud(data);
        }
    }
    Debug(DebugTypeError, builderSource, 1402, "")
        << "Failed setting data on node: " << m_field018;
    return false;
}

// PE798B30..798C41 (273 B). An empty variable name is refused before the cast.
// The binding is an INT that paintSelf FILDs into the icon position each frame.
// No m_system guard: the cast and the lookup both run regardless.
bool Bf2HudBuilder::setOccupiedNodePosVariable(int index, std::string variable)
{
    if (variable != "") {
        meme::BfOccupiedVehicleNode* node =
            builderCast<meme::BfOccupiedVehicleNode>(m_field014);
        if (node) {
            int* binding = static_cast<HudInformationLayer*>(g_hudInformationLayer)
                ->getIntByName(std::string(variable.c_str()));
            if (binding) return node->setPositionBindingForHud(index, binding);
        }
    }
    Debug(DebugTypeError, builderSource, 1385, "")
        << "Failed setting position variable on node: " << m_field018;
    return false;
}

// Sets the node's position and dimensions.
bool Bf2HudBuilder::setNodePos(int x, int y)                  // 799E80, line 1754
{
    meme::Object* object = m_system->findNamedObject(m_field050.c_str());
    if (object) {
        meme::Bf2TransformNode* node = builderCast<meme::Bf2TransformNode>(object);
        if (node)
            return node->setPositionForHud(static_cast<float>(x), static_cast<float>(y));
    }
    Debug(DebugTypeError, builderSource, 1754, "")
        << "Failed setting position on node: " << m_field050;
    return false;
}

bool Bf2HudBuilder::setNodeOffset(int x, int y)               // 799F50, line 1767
{
    meme::Object* object = m_system->findNamedObject(m_field050.c_str());
    if (object) {
        meme::Bf2TransformNode* node = builderCast<meme::Bf2TransformNode>(object);
        if (node) return node->setOffsetForHud(x, y);   // the ints go through raw
    }
    Debug(DebugTypeError, builderSource, 1767, "")
        << "Failed setting offset on node: " << m_field050;
    return false;
}

bool Bf2HudBuilder::setNodeSize(int width, int height)        // 79A140, line 1808
{
    meme::Object* object = m_system->findNamedObject(m_field050.c_str());
    if (object) {
        meme::Bf2TransformNode* node = builderCast<meme::Bf2TransformNode>(object);
        if (node)
            return node->setSizeForHud(static_cast<float>(width),
                                       static_cast<float>(height));
    }
    Debug(DebugTypeError, builderSource, 1808, "")
        << "Failed setting size on node: " << m_field050;
    return false;
}

// PE79CE80..79D01?. (450 B). `createObjectSelectionNode <parent> <name> <x> <y> <w> <h>` builds a
// Bf2ObjectSelectionNode named "<name>SelectionNode" - which takes only that name, its geometry
// coming from the transform - wraps it as child 1 of a Bf2TransformNode named after the second
// argument, and registers with mode 0. It does test the System first.
// The duplicate message reads "Node: <name> already exists!!" with a space where every other
// create row in this file writes a comma. Both spellings are shipped.
bool Bf2HudBuilder::createObjectSelectionNode(std::string parent, std::string name,
                                             int x, int y, int width, int height)
{
    if (m_system && !findNodePredecessor(parent, name)) {
        void* allocation = ::operator new(sizeof(meme::Bf2ObjectSelectionNode));
        meme::Bf2ObjectSelectionNode* selection = nullptr;
        if (allocation) {
            const std::string selectionName = name + "SelectionNode";
            selection = new(allocation) meme::Bf2ObjectSelectionNode(selectionName.c_str());
        }
        allocation = ::operator new(sizeof(meme::Bf2TransformNode));
        meme::Bf2TransformNode* transform = allocation
            ? new(allocation) meme::Bf2TransformNode(name.c_str(), x, y, width, height)
            : nullptr;
        transform->setChild(1, selection);
        return registerNode(parent, transform, selection, 0);
    }
    Debug(DebugTypeError, builderSource, 1066, "")
        << "Node: " << name << " already exists!!";
    return false;
}

// PE797B70..797C2? (180 B). Both console integers become floats before the slot
// sees them. No m_system guard, and no range test of any kind.
bool Bf2HudBuilder::setObjectSelectionNodePointerSize(int width, int height)
{
    meme::Bf2ObjectSelectionNode* node =
        builderCast<meme::Bf2ObjectSelectionNode>(m_field014);
    if (node)
        return node->setPointerSizeForHud(static_cast<float>(width),
                                          static_cast<float>(height));
    // Preserve the existing texture-error diagnostic for this failure path.
    Debug(DebugTypeError, builderSource, 1076, "")
        << "Failed setting texture on node: " << m_field018;
    return false;
}

// PE79C900..79CA?? (481 B). `createHoverNode <parent> <name> <x> <y> <w> <h>` builds a
// Bf2HoverNode named "<name>HoverNode" under a transform named after the second argument, mode 0,
// and then hands the node the same four geometry integers a second time through its +D4 slot - the
// transform gets them as its own position and size, and the node keeps its own float copy.
// Its duplicate message uses a space, like createObjectSelectionNode's, not the comma the rest
// of the create rows write.
bool Bf2HudBuilder::createHoverNode(std::string parent, std::string name,
                                   int x, int y, int width, int height)
{
    if (m_system && !findNodePredecessor(parent, name)) {
        void* allocation = ::operator new(sizeof(meme::Bf2HoverNode));
        meme::Bf2HoverNode* hover = nullptr;
        if (allocation) {
            const std::string hoverName = name + "HoverNode";
            hover = new(allocation) meme::Bf2HoverNode(m_system, hoverName.c_str());
        }
        allocation = ::operator new(sizeof(meme::Bf2TransformNode));
        meme::Bf2TransformNode* transform = allocation
            ? new(allocation) meme::Bf2TransformNode(name.c_str(), x, y, width, height)
            : nullptr;
        transform->setChild(1, hover);
        hover->setBoundsForHud(x, y, width, height);
        return registerNode(parent, transform, hover, 0);
    }
    Debug(DebugTypeError, builderSource, 805, "")
        << "Node: " << name << " already exists!!";
    return false;
}

// PE796EA0..796F5? (178 B). Two plain integers, stored in argument order.
bool Bf2HudBuilder::setHoverInMiddlePos(int x, int y)
{
    if (m_system) {
        meme::Bf2HoverNode* node = builderCast<meme::Bf2HoverNode>(m_field014);
        if (node) return node->setMiddlePositionForHud(x, y);
    }
    Debug(DebugTypeError, builderSource, 817, "")
        << "Failed setting in the middle positions on node: " << m_field018;
    return false;
}

// PE796F60..796FF? (174 B).
bool Bf2HudBuilder::setHoverMaxValue(float value)
{
    if (m_system) {
        meme::Bf2HoverNode* node = builderCast<meme::Bf2HoverNode>(m_field014);
        if (node) return node->setMaxValueForHud(value);
    }
    Debug(DebugTypeError, builderSource, 829, "")
        << "Failed setting max on node: " << m_field018;
    return false;
}

// PE797010..79706? (178 B). The slot it calls stores the arguments crossed:
// the width lands at +18 and the length at +14.
bool Bf2HudBuilder::setHoverWidthLength(float width, float length)
{
    if (m_system) {
        meme::Bf2HoverNode* node = builderCast<meme::Bf2HoverNode>(m_field014);
        if (node) return node->setWidthLengthForHud(width, length);
    }
    // The diagnostic calls it "scale", which is neither of the two names.
    Debug(DebugTypeError, builderSource, 841, "")
        << "Failed setting scale on node: " << m_field018;
    return false;
}

// PE794EC0..794ECA - eleven bytes, the smallest row in the file. It clears both
// insertion cursors and answers a hard true, so the next create row starts a new
// chain under its parent instead of appending to the previous one. Nothing else
// is touched: no System test, no diagnostic, no failure path at all.
bool Bf2HudBuilder::newLayer()
{
    m_field00c = nullptr;
    m_field010 = nullptr;
    return true;
}

// PE795E30..795E79. No System-null guard; callers enforce it where observed.
meme::Node* Bf2HudBuilder::findNode(std::string name)
{
    meme::Object* object = m_system->findNamedObject(name.c_str());
    if (!object) return nullptr;
    return builderCast<meme::Node>(object);
}

// PE79BEC0..79C10B. Return the predecessor of the matching named successor.
// Leading '*' prints named successors and returns the last traversed node if
// any name was printed. Shipped error diagnostics can abort when enabled.
meme::Node* Bf2HudBuilder::findNodePredecessor(std::string parent, std::string name)
{
    bool listed = false;
    meme::Object* object = m_system->findNamedObject(parent.c_str());
    if (!object) {
        Debug(DebugTypeError, builderSource, 2384, "") << parent << " doesn't exist...";
        return nullptr;
    }
    meme::Node* current = builderCast<meme::Node>(object);
    if (!current) {
        Debug(DebugTypeError, builderSource, 2381, "") << parent << " doesn't exist...";
        return nullptr;
    }
    // Both virtual reads are present, even if they ordinarily return one link.
    if (!current->getChild(0) || !current->getChild(0)) return nullptr;
    do {
        if (current->getChild(0)->slot_0x24()) {
            const std::string childName(current->getChild(0)->getName());
            if (name.c_str()[0] == '*') {
                meme::Node* child = current->getChild(0);
                {
                    Debug diagnostic(DebugTypeError, builderSource, 2366, "");
                    const char* printedName = child->getName(); // After Debug construction, PE79BFA3..79BFAE.
                    diagnostic << "->" << printedName;
                }
                listed = true;
            } else if (name == childName) {
                return current;
            }
        }
        current = current->getChild(0);
    } while (current->getChild(0));
    return listed ? current : nullptr;
}

// PE795FC0..7960A3. A branching parent starts at child1; otherwise start at
// the parent. The extra named-successor child read has a discarded result in
// retail; preserve the virtual call rather than treating it as dead code.
meme::Node* Bf2HudBuilder::findLastNode(std::string parent)
{
    meme::Object* object = m_system->findNamedObject(parent.c_str());
    if (!object) return nullptr;
    meme::Node* current = builderCast<meme::Node>(object);
    if (!current) return nullptr;
    if (current->getChildCount() > 1) {
        if (!current->getChild(1)) return nullptr;
        current = current->getChild(1);
    }
    if (current->getChild(0)) {
        do {
            if (current->getChild(0)->slot_0x24() && current->getChild(0)->getChild(0))
                current->getChild(0)->getChild(0);
            current = current->getChild(0);
        } while (current->getChild(0));
    }
    return current;
}

// PE7960C0..7963EB. Cursor/link writes precede later casts and possible false
// results. There is no rollback, node-null guard or ownership cleanup. Mode2
// leaves the cursor pair unchanged after its initial lookup.
bool Bf2HudBuilder::insertNode(std::string parent, meme::Node* node, int mode)
{
    if (parent != m_field06c) {
        m_field06c = parent;
        m_field00c = nullptr;
        m_field010 = nullptr;
    }
    if (!m_field00c && !m_field010) {
        meme::Node* last = findLastNode(parent);
        if (last) {
            meme::Object* object = builderCast<meme::Object>(last);
            if (object) {
                m_field00c = object;
                m_field010 = object;
                m_field088 = true;
            }
        } else {
            m_field00c = nullptr;
            m_field010 = nullptr;
            m_field088 = true;
        }
    }
    if (mode == 0 || mode == 1) {
        meme::Node* cursor = builderCast<meme::Node>(mode == 0 ? m_field00c : m_field010);
        if (cursor) {
            node->setChild(0, cursor->getChild(0));
            cursor->setChild(0, node);
        } else {
            meme::Object* object = m_system->findNamedObject(parent.c_str());
            if (!object) return false;
            cursor = builderCast<meme::Node>(object);
            if (!cursor || cursor->getChildCount() <= 1) return false;
            node->setChild(0, cursor->getChild(1));
            cursor->setChild(1, node);
        }
        meme::Object* object = builderCast<meme::Object>(node);
        if (!object) return false;
        if (mode == 0) {
            m_field00c = object;
            if (m_field088) m_field010 = object;
        } else {
            m_field010 = object;
            m_field088 = false;
        }
        return true;
    }
    if (mode == 2) {
        meme::Node* last = findLastNode(parent);
        if (last) {
            node->setChild(0, last->getChild(0));
            last->setChild(0, node);
            return true;
        }
        meme::Object* object = m_system->findNamedObject(parent.c_str());
        if (!object) return false;
        meme::Node* parentNode = builderCast<meme::Node>(object);
        if (!parentNode || parentNode->getChildCount() <= 1) return false;
        node->setChild(0, parentNode->getChild(1));
        parentNode->setChild(1, node);
        return true;
    }
    return false;
}

// PE7963F0..796523. Insert first, register without a container, then select by
// data name or node name. Registration failure leaves the inserted node alive.
bool Bf2HudBuilder::registerNode(std::string parent, meme::Node* node, meme::Object* data, int mode)
{
    m_field050 = "NULL";
    if (!m_system || !insertNode(parent, node, mode) || !node ||
        !m_system->registerObject(node, nullptr)) return false;
    if (data && m_system->registerObject(data, nullptr)) {
        if (!setActiveObject(parent, std::string(data->getName()))) return false;
        if (node->slot_0x24()) m_field050 = node->getName();
        return true;
    }
    if (node->slot_0x24()) return setActiveObject(parent, std::string(node->getName()));
    return false;
}

// PE795480..795591. Parent is selection metadata; registry lookup uses name
// alone. A null System preserves the old selected pointer; a miss stores null.
// Both failure paths preserve m_field050.
bool Bf2HudBuilder::setActiveObject(std::string parent, std::string name)
{
    m_field018 = name + "_notFound";
    m_field034 = parent + "_notFound";
    if (!m_system) return false;
    m_field014 = m_system->findNamedObject(name.c_str());
    if (!m_field014) return false;
    m_field018 = name;
    m_field050 = name;
    m_field034 = parent;
    return true;
}

// Creates a named button beneath a positioned transform. Requires m_system and a unique node name.
// The button stores width and height; the transform stores the complete rectangle.
bool Bf2HudBuilder::createButtonNode(std::string parent, std::string name,
                                     int x, int y, int width, int height)
{
    if (!findNodePredecessor(parent, name)) {
        void* allocation = ::operator new(sizeof(meme::Bf2ButtonNode));
        meme::Bf2ButtonNode* button = nullptr;
        if (allocation) {
            const std::string buttonName = name + "ButtonNode";
            button = new(allocation) meme::Bf2ButtonNode(buttonName.c_str(), x, y, width, height);
        }
        allocation = ::operator new(sizeof(meme::Bf2TransformNode));
        meme::Bf2TransformNode* transform = allocation
            ? new(allocation) meme::Bf2TransformNode(name.c_str(), x, y, width, height)
            : nullptr;
        transform->setChild(1, button);
        return registerNode(parent, transform, button, 0);
    }
    Debug(DebugTypeError, builderSource, 1418, "")
        << "Node: " << name << ", already exists!!";
    return false;
}

// PE798D20..798E0B (235 B). The texture is loaded before the receiver is
// cast, so a wrong selected node still costs a load, and the handle is parked on
// the builder at retail +8C on its way through. The diagnostic is the
// picture row's: it says "Picture", not "button texture".
bool Bf2HudBuilder::setButtonNodeTexture(int index, std::string texture)
{
    if (m_system) {
        m_buttonTexture = static_cast<std::uintptr_t>(m_system->loadTexture(texture.c_str(), true));
        meme::Bf2ButtonNode* node = builderCast<meme::Bf2ButtonNode>(m_field014);
        if (node) return node->setTextureForHud(index, static_cast<meme::TextureHandle>(m_buttonTexture));
    }
    Debug(DebugTypeError, builderSource, 1434, "")
        << "Failed setting Picture on node: " << m_field018;
    return false;
}


bool Bf2HudBuilder::setButtonNodeMouseArea(int x, int y, int width, int height)
{
    if (m_system) {
        meme::Bf2ButtonNode* node = builderCast<meme::Bf2ButtonNode>(m_field014);
        if (node) return node->setMouseAreaForHud(x, y, width, height);
    }
    Debug(DebugTypeError, builderSource, 1446, "")
        << "Failed setting mouse area on node: " << m_field018;
    return false;
}


bool Bf2HudBuilder::setButtonNodeDebug(bool enabled)
{
    if (m_system) {
        meme::Bf2ButtonNode* node = builderCast<meme::Bf2ButtonNode>(m_field014);
        if (node) return node->setDebugForHud(enabled);
    }
    Debug(DebugTypeError, builderSource, 1470, "")
        << "Failed setting debug on node: " << m_field018;
    return false;
}

// Builds the console command executed by a button action.
bool Bf2HudBuilder::setButtonNodeConCmd(std::string command, int index)
{
    if (m_system && index < 4 && command != "") {
        void* allocation = ::operator new(sizeof(meme::Bf2CallFunctionAction));
        meme::Bf2CallFunctionAction* action = allocation
            ? new(allocation) meme::Bf2CallFunctionAction() : nullptr;
        HudInformationLayer* const layer =
            static_cast<HudInformationLayer*>(g_hudInformationLayer);
        const HudInformationLayer::StringFunction execute =
            layer->getStringFunction(std::string("executeConsoleCommand"));
        if (!execute) return false;
        allocation = ::operator new(sizeof(HudStringFunction));
        HudStringFunction* const function = allocation
            ? new(allocation) HudStringFunction(execute, "", command + "\n") : nullptr;
        action->setFunction(function);
        meme::Bf2ButtonNode* node = builderCast<meme::Bf2ButtonNode>(m_field014);
        if (node) {
            meme::Action* const installed = node->getActionForHud(index);
            if (installed)
                return static_cast<meme::Bf2ActionListAction*>(installed)
                    ->addAction(m_system, action);
            allocation = ::operator new(sizeof(meme::Bf2ActionListAction));
            meme::Bf2ActionListAction* const list = allocation
                ? new(allocation) meme::Bf2ActionListAction() : nullptr;
            node->setActionForHud(list, index);
            return setButtonNodeConCmd(command, index);
        }
    }
    Debug(DebugTypeError, builderSource, 1535, "")
        << "Failed setting console command on node: " << m_field018;
    return false;
}

// PE7993A0..79958B (493 B). The same body against the button's second action
// array (+FC/+F4 instead of +F8/+F0), with its own diagnostic and line number.
bool Bf2HudBuilder::setButtonNodeAltConCmd(std::string command, int index)
{
    if (m_system && index < 4 && command != "") {
        void* allocation = ::operator new(sizeof(meme::Bf2CallFunctionAction));
        meme::Bf2CallFunctionAction* action = allocation
            ? new(allocation) meme::Bf2CallFunctionAction() : nullptr;
        HudInformationLayer* const layer =
            static_cast<HudInformationLayer*>(g_hudInformationLayer);
        const HudInformationLayer::StringFunction execute =
            layer->getStringFunction(std::string("executeConsoleCommand"));
        if (!execute) return false;
        allocation = ::operator new(sizeof(HudStringFunction));
        HudStringFunction* const function = allocation
            ? new(allocation) HudStringFunction(execute, "", command + "\n") : nullptr;
        action->setFunction(function);
        meme::Bf2ButtonNode* node = builderCast<meme::Bf2ButtonNode>(m_field014);
        if (node) {
            meme::Action* const installed = node->getAltActionForHud(index);
            if (installed)
                return static_cast<meme::Bf2ActionListAction*>(installed)
                    ->addAction(m_system, action);
            allocation = ::operator new(sizeof(meme::Bf2ActionListAction));
            meme::Bf2ActionListAction* const list = allocation
                ? new(allocation) meme::Bf2ActionListAction() : nullptr;
            node->setAltActionForHud(list, index);
            return setButtonNodeAltConCmd(command, index);
        }
    }
    Debug(DebugTypeError, builderSource, 1568, "")
        << "Failed setting alternative console command on node: " << m_field018;
    return false;
}

// Creates a list node and loads its font. Requires m_system; duplicate names are not checked.
bool Bf2HudBuilder::createListNode(std::string parent, std::string name, int x, int y,
                                   int width, int height, int rowHeight, bool selectable)
{
    void* allocation = ::operator new(sizeof(meme::Bf2NewListBoxNode));
    meme::Bf2NewListBoxNode* list = nullptr;
    if (allocation) {
        const std::string listName = name + "ListNode";
        list = new(allocation) meme::Bf2NewListBoxNode(listName.c_str());
    }
    allocation = ::operator new(sizeof(meme::Bf2TransformNode));
    meme::Bf2TransformNode* transform = allocation
        ? new(allocation) meme::Bf2TransformNode(name.c_str(), x, y, width, height)
        : nullptr;
    // The shipped defaults, in the shipped order.
    list->setFontForHud(m_system->loadFont("Fonts/vehicleHudFont_6.dif"), 0);
    list->setSelectActionForHud(nullptr);
    list->setFocusActionForHud(nullptr);
    list->setSound070ForHud(0);
    list->setWidthForHud(200.0f);
    list->setSelectableForHud(selectable);
    list->setSound068ForHud(0);
    list->setSound06cForHud(0);
    list->setRowHeightForHud(static_cast<float>(rowHeight));
    list->setBorderEnabledForHud(false);
    list->setBackgroundRedForHud(1.0f);
    list->setBackgroundGreenForHud(0.0f);
    list->setBackgroundBlueForHud(0.0f);
    list->setBackgroundAlphaForHud(0.0f);
    list->setFrameRedForHud(1.0f);
    list->setFrameGreenForHud(1.0f);
    list->setFrameBlueForHud(0.0f);
    list->setFrameAlphaForHud(0.0f);
    list->setSelectionRedForHud(1.0f);
    list->setSelectionGreenForHud(1.0f);
    list->setSelectionBlueForHud(1.0f);
    list->setSelectionAlphaForHud(0.5f);
    list->setScrollOffsetForHud(1.0f);
    list->setField109ForHud(false);
    transform->setChild(1, list);
    return registerNode(parent, transform, list, 0);
}

// Selects list data using the supplied selector. The existing contradictory bounds test admits every value.
bool Bf2HudBuilder::setListNodeData(int selector)
{
    if (m_system) {
        bool inRange = true;
        if (selector <= 0 && selector >= 12) inRange = false;
        if (inRange) {
            meme::Bf2NewListBoxNode* node = builderCast<meme::Bf2NewListBoxNode>(m_field014);
            if (node) {
                // PE795 8FB loads ds:A10890 - the HUD manager, not the layer -
                // and calls its +1CC slot with the selector, unguarded.
                meme::BfListBoxData* const data = hudManager->getListData(selector);
                if (data) {
                    node->setDataForHud(data);
                    node->bindDataToSelfForHud();
                    return true;
                }
            }
        }
    }
    Debug(DebugTypeError, builderSource, 530, "") << "Failed setting Data!";
    return false;
}

// PE7959A0..795C9? (786 B). The same shape as setButtonNodeConCmd, against the
// list's two action slots: `which == 1` picks the focus action (+50), anything
// else the select action (+48). Same four shipped behaviours: a silent false
// when the layer has no "executeConsoleCommand", the first-pass leak, the
// self-recursion, and an empty command refused before any allocation.
bool Bf2HudBuilder::setListNodeConCmd(int which, std::string command)
{
    if (m_system && command != "") {
        void* allocation = ::operator new(sizeof(meme::Bf2CallFunctionAction));
        meme::Bf2CallFunctionAction* action = allocation
            ? new(allocation) meme::Bf2CallFunctionAction() : nullptr;
        HudInformationLayer* const layer =
            static_cast<HudInformationLayer*>(g_hudInformationLayer);
        const HudInformationLayer::StringFunction execute =
            layer->getStringFunction(std::string("executeConsoleCommand"));
        if (!execute) return false;
        allocation = ::operator new(sizeof(HudStringFunction));
        HudStringFunction* const function = allocation
            ? new(allocation) HudStringFunction(execute, "", command + "\n") : nullptr;
        action->setFunction(function);
        meme::Bf2NewListBoxNode* node = builderCast<meme::Bf2NewListBoxNode>(m_field014);
        if (node) {
            meme::Action* const installed = which == 1 ? node->getFocusActionForHud()
                                                       : node->getSelectActionForHud();
            if (installed)
                return static_cast<meme::Bf2ActionListAction*>(installed)
                    ->addAction(m_system, action);
            allocation = ::operator new(sizeof(meme::Bf2ActionListAction));
            meme::Bf2ActionListAction* const list = allocation
                ? new(allocation) meme::Bf2ActionListAction() : nullptr;
            if (which == 1) node->setFocusActionForHud(list);
            else node->setSelectActionForHud(list);
            return setListNodeConCmd(which, command);
        }
    }
    Debug(DebugTypeError, builderSource, 617, "") << "Failed setting Action!";
    return false;
}

// PE795600/795660/7956C0/795810/795870 (88 B each). Four separate one-float
// slots per colour, in argument order. No m_system test and no diagnostic:
// a failed cast is a bare false.
bool Bf2HudBuilder::setListNodeBackgroundColor(float r, float g, float b, float a)
{
    meme::Bf2NewListBoxNode* node = builderCast<meme::Bf2NewListBoxNode>(m_field014);
    if (!node) return false;
    node->setBackgroundRedForHud(r);
    node->setBackgroundGreenForHud(g);
    node->setBackgroundBlueForHud(b);
    node->setBackgroundAlphaForHud(a);
    return true;
}
bool Bf2HudBuilder::setListNodeSelectColor(float r, float g, float b, float a)
{
    meme::Bf2NewListBoxNode* node = builderCast<meme::Bf2NewListBoxNode>(m_field014);
    if (!node) return false;
    node->setSelectionRedForHud(r);
    node->setSelectionGreenForHud(g);
    node->setSelectionBlueForHud(b);
    node->setSelectionAlphaForHud(a);
    return true;
}
bool Bf2HudBuilder::setListNodeBorderColor(float r, float g, float b, float a)
{
    meme::Bf2NewListBoxNode* node = builderCast<meme::Bf2NewListBoxNode>(m_field014);
    if (!node) return false;
    node->setFrameRedForHud(r);
    node->setFrameGreenForHud(g);
    node->setFrameBlueForHud(b);
    node->setFrameAlphaForHud(a);
    return true;
}
// Row 52 is named "scrollbar" but writes the +D8..E4 colour the header used to
// call m_text; row 53 writes +E8..F4, which the constructor never touches.
bool Bf2HudBuilder::setListNodeScrollbarColor(float r, float g, float b, float a)
{
    meme::Bf2NewListBoxNode* node = builderCast<meme::Bf2NewListBoxNode>(m_field014);
    if (!node) return false;
    node->setScrollbarRedForHud(r);
    node->setScrollbarGreenForHud(g);
    node->setScrollbarBlueForHud(b);
    node->setScrollbarAlphaForHud(a);
    return true;
}
bool Bf2HudBuilder::setListNodeScrollbarBackgroundColor(float r, float g, float b, float a)
{
    meme::Bf2NewListBoxNode* node = builderCast<meme::Bf2NewListBoxNode>(m_field014);
    if (!node) return false;
    node->setScrollbarBackRedForHud(r);
    node->setScrollbarBackGreenForHud(g);
    node->setScrollbarBackBlueForHud(b);
    node->setScrollbarBackAlphaForHud(a);
    return true;
}

// PE795720..795783 (100 B). It turns the border on with a constant before it
// stores any of the four widths.
bool Bf2HudBuilder::setListNodeBorder(int top, int bottom, int left, int right)
{
    meme::Bf2NewListBoxNode* node = builderCast<meme::Bf2NewListBoxNode>(m_field014);
    if (!node) return false;
    node->setBorderEnabledForHud(true);
    node->setBorderTopForHud(top);
    node->setBorderBottomForHud(bottom);
    node->setBorderLeftForHud(left);
    node->setBorderRightForHud(right);
    return true;
}

// PE7957C0..795801 (66 B). Both integers are fild'd; the width slot runs first.
bool Bf2HudBuilder::setListNodeScrollbar(int width, int offset)
{
    meme::Bf2NewListBoxNode* node = builderCast<meme::Bf2NewListBoxNode>(m_field014);
    if (!node) return false;
    node->setScrollWidthForHud(static_cast<float>(width));
    node->setScrollOffsetForHud(static_cast<float>(offset));
    return true;
}

// PE795790..7957BB (44 B). The smallest row in the family.
bool Bf2HudBuilder::setListNodeRowSpacing(int spacing)
{
    meme::Bf2NewListBoxNode* node = builderCast<meme::Bf2NewListBoxNode>(m_field014);
    if (!node) return false;
    node->setRowSpacingForHud(static_cast<float>(spacing));
    return true;
}

// PE7955A0..7955FD (94 B). No m_system test even though it loads through it,
// and the node's own index guard lets a negative index through.
bool Bf2HudBuilder::setListNodeFont(std::string font, int index)
{
    meme::Bf2NewListBoxNode* node = builderCast<meme::Bf2NewListBoxNode>(m_field014);
    if (!node) return false;
    node->setFontForHud(m_system->loadFont(font.c_str()), index);
    return true;
}

// PE796970..796A2? (187 B). The only row in the family that reaches the data
// rather than the node, and the only one besides the two above with a guard.
bool Bf2HudBuilder::setListNodeOutline(bool enabled)
{
    if (m_system) {
        meme::Bf2NewListBoxNode* node = builderCast<meme::Bf2NewListBoxNode>(m_field014);
        if (node) {
            meme::BfListBoxData* const data = node->getDataForHud();
            if (data) { data->setOutlineForHud(enabled); return true; }
        }
    }
    Debug(DebugTypeError, builderSource, 548, "")
        << "Failed setting outline on node: " << m_field018;
    return false;
}

// Creates a named edit node beneath a positioned transform. Requires m_system and a unique node name.
bool Bf2HudBuilder::createEditNode(std::string parent, std::string name,
                                   int x, int y, int width, int height)
{
    if (!findNodePredecessor(parent, name)) {
        void* allocation = ::operator new(sizeof(meme::Bf2EditNode));
        meme::Bf2EditNode* edit = nullptr;
        if (allocation) {
            const std::string editName = name + "EditNode";
            edit = new(allocation) meme::Bf2EditNode(editName.c_str());
        }
        allocation = ::operator new(sizeof(meme::Bf2TransformNode));
        meme::Bf2TransformNode* transform = allocation
            ? new(allocation) meme::Bf2TransformNode(name.c_str(), x, y, width, height)
            : nullptr;
        transform->setChild(1, edit);
        return registerNode(parent, transform, edit, 0);
    }
    Debug(DebugTypeError, builderSource, 856, "")
        << "Node: " << name << ", already exists!!";
    return false;
}

// PE7970D0..797124 (212 B). The data comes from the HUD singleton's +1C8 slot, and
// the node is written back into the data's own +14. The selector test here is
// `>= 0`, where setEditNodeString's is `> 0` - the two rows disagree about zero.
bool Bf2HudBuilder::setEditNodeData(int selector)
{
    if (m_system && selector >= 0) {
        meme::Bf2EditNode* node = builderCast<meme::Bf2EditNode>(m_field014);
        if (node) {
            meme::BfEditData* const data = hudManager->getEditData(selector);
            if (data) {
                node->setEditDataForHud(data);
                data->m_activeNode = node;      // the direct store at 797119
                return true;
            }
        }
    }
    Debug(DebugTypeError, builderSource, 878, "")
        << "Failed setting Data on node: " << m_field018;
    return false;
}

// PE7971B0..79727? (205 B). This one refuses a selector of zero, unlike the
// data row above.
bool Bf2HudBuilder::setEditNodeString(int selector)
{
    if (m_system && selector > 0) {
        meme::Bf2EditNode* node = builderCast<meme::Bf2EditNode>(m_field014);
        if (node) {
            meme::Data* const string = hudManager->getStringData(selector);
            if (string) return node->setStringForHud(string);
        }
    }
    Debug(DebugTypeError, builderSource, 894, "")
        << "Failed setting string on node: " << m_field018;
    return false;
}

// PE797280..79732? (174 B).
bool Bf2HudBuilder::setEditNodeMaxLength(int characters)
{
    if (m_system) {
        meme::Bf2EditNode* node = builderCast<meme::Bf2EditNode>(m_field014);
        if (node) return node->setMaxLengthForHud(characters);
    }
    Debug(DebugTypeError, builderSource, 906, "")
        << "Failed max length on node: " << m_field018;
    return false;
}

// PE797330..7973E? (186 B). One call, four floats.
bool Bf2HudBuilder::setEditNodeColor(float r, float g, float b, float a)
{
    if (m_system) {
        meme::Bf2EditNode* node = builderCast<meme::Bf2EditNode>(m_field014);
        if (node) return node->setColorForHud(r, g, b, a);
    }
    Debug(DebugTypeError, builderSource, 918, "")
        << "Failed set color on node: " << m_field018;
    return false;
}

// PE7973F0..7974F? (261 B). The row is called `setEditNodeFont` but its second
// argument is the node's outline flag, and the font is loaded before the receiver
// is cast - so a wrong selection still costs a load.
bool Bf2HudBuilder::setEditNodeFont(std::string font, bool outline)
{
    if (m_system && font != "") {
        const meme::FontHandle handle = m_system->loadFont(font.c_str());
        meme::Bf2EditNode* node = builderCast<meme::Bf2EditNode>(m_field014);
        if (node) return node->setFontForHud(handle, outline);
    }
    Debug(DebugTypeError, builderSource, 938, "")
        << "Failed setting font on node: " << m_field018;
    return false;
}

// Creates a minimap beneath a transform named from parent. Returns false when m_system is absent.
// The node retains name without a suffix; duplicate names are not checked.
bool Bf2HudBuilder::createMiniMapNode(std::string parent, std::string name,
                                      int x, int y, int width, int height)
{
    if (!m_system) return false;
    void* allocation = ::operator new(sizeof(meme::Bf2MiniMenuMapNode));
    meme::Bf2MiniMenuMapNode* mini = allocation
        ? new(allocation) meme::Bf2MiniMenuMapNode(m_system, name.c_str(), x, y, width, height)
        : nullptr;
    allocation = ::operator new(sizeof(meme::Bf2TransformNode));
    meme::Bf2TransformNode* transform = allocation
        ? new(allocation) meme::Bf2TransformNode(parent.c_str(), x, y, width, height)
        : nullptr;
    transform->setChild(1, mini);
    return registerNode(parent, transform, mini, 0);
}

// Creates a transform-list node with the supplied position and count beneath a sized transform.
// Requires m_system and a unique name; node position uses floats.
bool Bf2HudBuilder::createTransformListNode(std::string parent, std::string name,
                                            int x, int y, int width, int height, int count)
{
    if (!findNodePredecessor(parent, name)) {
        void* allocation = ::operator new(sizeof(meme::Bf2TransformListNode));
        meme::Bf2TransformListNode* list = nullptr;
        if (allocation) {
            const std::string listName = name + "TransformListNode";
            list = new(allocation) meme::Bf2TransformListNode(
                listName.c_str(), static_cast<float>(x), static_cast<float>(y), count);
        }
        allocation = ::operator new(sizeof(meme::Bf2TransformNode));
        meme::Bf2TransformNode* transform = allocation
            ? new(allocation) meme::Bf2TransformNode(name.c_str(), x, y, width, height)
            : nullptr;
        transform->setChild(1, list);
        return registerNode(parent, transform, list, 0);
    }
    Debug(DebugTypeError, builderSource, 682, "")
        << "Node: " << name << ", already exists!!";
    return false;
}

// PE796A30..796A91 (230 B). No m_system test at all, even though it looks the
// name up through it. The node it finds must be a Bf2TransformNode; anything else
// - or a miss - reports.
bool Bf2HudBuilder::addTransformListNode(std::string name)
{
    meme::Bf2TransformListNode* node = builderCast<meme::Bf2TransformListNode>(m_field014);
    if (node) {
        meme::Object* const found = m_system->findNamedObject(name.c_str());
        if (found) {
            meme::Bf2TransformNode* const child = builderCast<meme::Bf2TransformNode>(found);
            if (child) return node->addChildForHud(child);
        }
    }
    Debug(DebugTypeError, builderSource, 697, "")
        << "Failed adding node to node: " << m_field018;
    return false;
}

// PE796B20..796B9? (251 B). The binding comes from the information layer's
// getIntByName, so the variable must already be registered.
bool Bf2HudBuilder::setTranformListNodePosVariable(int index, std::string variable)
{
    meme::Bf2TransformListNode* node = builderCast<meme::Bf2TransformListNode>(m_field014);
    if (node) {
        HudInformationLayer* const layer =
            static_cast<HudInformationLayer*>(g_hudInformationLayer);
        int* const binding = layer->getIntByName(std::string(variable));
        if (binding) return node->setPositionBindingForHud(index, binding);
    }
    Debug(DebugTypeError, builderSource, 713, "")
        << "Failed setting position variable on node: " << m_field018;
    return false;
}

// PE795D80..795DB? (171 B). No m_system test; a failed cast is the only refusal.
bool Bf2HudBuilder::setTranformListNodeOffset(int x, int y)
{
    meme::Bf2TransformListNode* node = builderCast<meme::Bf2TransformListNode>(m_field014);
    if (node) return node->setOffsetForHud(x, y);
    Debug(DebugTypeError, builderSource, 723, "")
        << "Failed setting offset on node: " << m_field018;
    return false;
}

// Creates a slider together with its transform node.
// The float arguments configure its range, thumb span and step; geometry is set separately.
bool Bf2HudBuilder::createSliderNode(std::string parent, std::string name, float minimum,
                                     float maximum, float thumbSpan, float step)
{
    if (findNodePredecessor(parent, name)) {
        Debug(DebugTypeError, builderSource, 740, "")
            << "Node: " << m_field018 << ", already exists!!";
        return false;
    }
    void* allocation = ::operator new(sizeof(meme::Bf2TransformNode));
    meme::Bf2TransformNode* transform = allocation
        ? new(allocation) meme::Bf2TransformNode(name.c_str(), 0, 0, 0, 0) : nullptr;
    allocation = ::operator new(sizeof(meme::Bf2FixedSliderNode));
    meme::Bf2FixedSliderNode* slider = nullptr;
    if (allocation) {
        std::string full(name.c_str());
        full += "SliderNode";
        slider = new(allocation) meme::Bf2FixedSliderNode(full.c_str(), minimum, maximum,
                                                          thumbSpan, step);
    }
    transform->setChild(1, slider);
    return registerNode(parent, transform, slider, 0);
}

// Binds the active slider to a named information-layer float.
// Returns false for an incompatible node or missing variable; requires an initialized HUD system.
bool Bf2HudBuilder::setSliderNodeData(std::string variable)
{
    meme::Bf2FixedSliderNode* node = builderCast<meme::Bf2FixedSliderNode>(m_field014);
    if (node) {
        float* const value = static_cast<HudInformationLayer*>(g_hudInformationLayer)
            ->getFloatByName(std::string(variable.c_str()));
        if (value) {
            void* allocation = ::operator new(sizeof(meme::FloatRefData));
            meme::FloatRefData* const data = allocation
                ? new(allocation) meme::FloatRefData(value, "sliderData") : nullptr;
            return node->setDataForHud(data);
        }
    }
    Debug(DebugTypeError, builderSource, 757, "")
        << "Failed setting data on node: " << m_field018;
    return false;
}

// Relinks child zero beneath the slider; the child is moved, not copied.
bool Bf2HudBuilder::setSliderNodeChild(std::string name)
{
    meme::Object* const object = m_system->findNamedObject(name.c_str());
    if (object) {
        meme::Node* const found = builderCast<meme::Node>(object);
        if (found) {
            meme::Bf2FixedSliderNode* const slider =
                builderCast<meme::Bf2FixedSliderNode>(m_field014);
            if (slider && found->getChild(0)) {
                meme::Node* const child = found->getChild(0);
                if (child) {
                    meme::Bf2TransformNode* const transform =
                        builderCast<meme::Bf2TransformNode>(child);
                    if (transform) {
                        found->setChild(0, transform->getChild(0));
                        transform->setChild(0, nullptr);
                        slider->setChild(1, transform);
                        return true;
                    }
                }
            }
        }
    }
    Debug(DebugTypeError, builderSource, 786, "")
        << "Failed adding child - " << name << " to sliderNode: " << m_field018;
    return false;
}

#if defined(_M_IX86)
static_assert(sizeof(Bf2HudBuilder) == 0x8c, "native x86 HUD builder; retail0x9C uses larger strings");
#endif
} } }
#endif
