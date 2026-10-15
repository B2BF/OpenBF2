// [INFERRED] path. Full17-slot primary94CEC8, descriptor94CCCC and all
// differing methods/factory/getter/eager instructions read before recovery.
#include "SetVariableAction.h"
#if !DEDICATED
#include "Data.h"
#include "Stream.h"
#include "TypeDescriptor.h"
namespace dice { namespace meme {
namespace {
// MSVC x86 dispatch captures the destination table before reading the source, then reloads the receiver.
// Preserve the saved table: a normal virtual call could select a replacement receiver's implementation.
// Helper names and isolation are [INFERRED].
using DataSetFloatMethod = bool (__thiscall*)(Data*, System*, float);
using DataMethodTable = void* const*;
DataMethodTable captureDataMethods(Data* data)
{ return *reinterpret_cast<DataMethodTable const*>(data); }
// Slot 0x4C/4 is setFloat in both MSVC table layouts (one destructor slot); on x64 the
// __thiscall spelling is ignored and the same table index is read.
void callCapturedSetFloat(DataMethodTable methods, Data* receiver, System* system, float value)
{ reinterpret_cast<DataSetFloatMethod>(methods[0x4C / 4])(receiver, system, value); }
#if defined(_M_IX86)
static_assert(sizeof(void*) == 4, "PE Data dispatch is x86");
#endif
}
SetVariableAction::SetVariableAction() : m_variable(nullptr), m_data(nullptr) {}
SetVariableAction::~SetVariableAction() {}
TypeDescriptor* SetVariableAction::getClassType()
{
    static ClassTypeDescriptor<SetVariableAction, Action> type("dice::meme::SetVariableAction");
    return &type;
}
TypeDescriptor* SetVariableAction::getType() { return getClassType(); }
unsigned int SetVariableAction::getObjectSize() { return sizeof(SetVariableAction); }
namespace {
// CRT96E16C ->870E40, same lazy descriptorA39D54, independent counterA39D34.
TypeDescriptor* const registeredSetVariableActionType = SetVariableAction::getClassType();
}
bool SetVariableAction::streamProperties(System* system, Stream* stream)
{
    if (!Action::streamProperties(system, stream)) return false;
    if (!stream->streamData(system, m_variable, "Variable", nullptr)) return false;
    return stream->streamData(system, m_data, "Data", nullptr) != false;
}
bool SetVariableAction::execute(System* system, const Rect*, const NodeState*, const NodeInputEvent*)
{
    if (m_variable && m_data)
    {
        DataMethodTable methods = captureDataMethods(m_variable);
        const float value = m_data->getFloat(system);
        callCapturedSetFloat(methods, m_variable, system, value);
    }
    return false; // The destination setter's result is discarded in retail.
}
#if defined(_M_IX86)
static_assert(sizeof(SetVariableAction) == 12, "client SetVariableAction");
#endif
} }
#endif
