// Editor preview objects live in the level view but are never saved, picked, or counted
// as edits. Refresh recreates the preview; camera framing, turntable, grid, bounds, and
// center aids operate on it. Level unload destroys it with other objects.
#include "Framework/EditorConfig.h"
#include "App/EditSession.h"

#include "App/FreeCamera.h"
#include "Framework/EditorLog.h"

#include "Game/Objects/IObject.h"
#include "Core/Ids.h"
#include "Game/Objects/IObjectTemplate.h"
#include "IO/Console/Console.h"
#include "Scene/Object/ObjectManager.h"
#include "Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace bf2editor
{

namespace
{

using dice::hfe::world::IObject;
using dice::hfe::world::IObjectTemplate;

std::string previewLine(const std::string& line)
{
    std::string answer;
    dice::hfe::io::mainConsole->executeLine(line + "\n", answer, false, false, false, false, true);
    const size_t begin = answer.find_first_not_of(" \t\r\n");
    return begin == std::string::npos ? std::string() : answer.substr(begin, answer.find_last_not_of(" \t\r\n") - begin + 1);
}

} // namespace

bool EditSession::makePreview(const std::string& name, const float at[3], float yaw, std::string& result)
{
    IObjectTemplate* objectTemplate =
        dice::hfe::world::objectTemplateManager ? dice::hfe::world::objectTemplateManager->getTemplate(name) : 0;
    if (!objectTemplate || !dice::hfe::io::mainConsole || !dice::hfe::world::objectManager)
    {
        result = "no template named " + name;
        return false;
    }
    std::string ignored;
    clearPreview(ignored);
    IObject* previous = dice::hfe::world::objectManager->getActiveObject();
    const std::string answer = previewLine("Object.create " + objectTemplate->getName());
    IObject* made = dice::hfe::world::objectManager->getActiveObject();
    if (!made || made == previous || made->getTemplate() != objectTemplate)
    {
        dice::hfe::world::objectManager->setActiveObject(previous);
        result = "the engine made no " + objectTemplate->getName() + " (Object.create answered '" + answer + "')";
        return false;
    }
    char line[160];
    std::snprintf(line, sizeof(line), "Object.absolutePosition %.3f/%.3f/%.3f", at[0], at[1] == 0.0f ? 0.01f : at[1], at[2]);
    const std::string placed = previewLine(line);
    std::snprintf(line, sizeof(line), "Object.rotation %.1f/0/0", yaw);
    const std::string turned = placed.empty() ? previewLine(line) : std::string();
    if (placed.empty() && turned.empty())
        previewLine("Object.layer 1");
    m_preview = made->getObjectId();
    dice::hfe::world::objectManager->setActiveObject(previous);
    if (!placed.empty() || !turned.empty())
    {
        clearPreview(ignored);
        result = "the engine refused the preview's place: " + (placed.empty() ? turned : placed);
        return false;
    }
    m_previewTemplate = objectTemplate->getName();
    for (int k = 0; k < 3; ++k)
        m_previewAt[k] = at[k];
    m_previewYaw = yaw;
    std::snprintf(line, sizeof(line), " at %.1f/%.1f/%.1f (object #%u, not part of the level)", at[0], at[1], at[2], m_preview);
    result = "previewing " + m_previewTemplate + line;
    return true;
}

bool EditSession::clearPreview(std::string& result)
{
    if (m_preview == 0)
    {
        result = "there is no preview";
        return true;
    }
    const unsigned id = m_preview;
    m_preview = 0;
    m_selection.remove(id);
    IObject* o = object(id);
    if (o && dice::hfe::world::objectManager)
    {
        o->forceDestroy();
        dice::hfe::world::objectManager->destroyObjects();
    }
    result = "the preview of " + m_previewTemplate + " is gone";
    return true;
}

bool EditSession::previewCommand(const std::string& action, const std::string& argument, std::string& result)
{
    if (action == "show")
    {
        float at[3];
        pointAtViewCentre(at);
        m_pickedPart = -1;
        return makePreview(argument, at, 0.0f, result);
    }
    if (action == "clear")
    {
        m_turntable = 0.0f;
        m_pickedPart = -1;
        return clearPreview(result);
    }
    IObject* preview = m_preview ? object(m_preview) : 0;
    if (!preview)
    {
        result = "there is no preview (EditorPreview.show <template>)";
        return false;
    }
    if (action == "refresh")
    {
        // Made again from the template where it stood - after an edit of the template, its children or components.
        const std::string name = m_previewTemplate;
        float at[3] = { m_previewAt[0], m_previewAt[1], m_previewAt[2] };
        if (!makePreview(name, at, m_previewYaw, result))
            return false;
        result = "refreshed: " + result;
        return true;
    }
    if (action == "reset")
    {
        m_turntable = 0.0f;
        m_previewYaw = 0.0f;
        dice::hfe::Mat4 m = preview->getAbsoluteTransformation();
        dice::hfe::Vec3 none;
        none.x = none.y = none.z = 0.0f;
        setRotation(m, none);
        for (int k = 0; k < 3; ++k)
            m.m[12 + k] = m_previewAt[k];
        preview->setAbsoluteTransformation(m);
        result = "the preview is back where it was made, unturned";
        return true;
    }
    if (action == "frame")
    {
        // The camera on it: in front of it, above, at two and a half times its size (at least 4 m).
        const dice::hfe::Mat4& m = preview->getAbsoluteTransformation();
        float r = preview->getVisualBoundingRadius();
        if (!(r > 0.0f))
            r = 2.0f;
        const float d = r * 2.5f < 4.0f ? 4.0f : r * 2.5f;
        m_camera.place(m.m[12], m.m[13] + d * 0.6f, m.m[14] - d, 0.0f, -31.0f);
        char text[128];
        std::snprintf(text, sizeof(text), "the camera looks at the preview from %.1f m", d);
        result = text;
        return true;
    }
    if (action == "turntable")
    {
        const float rate = static_cast<float>(std::atof(argument.c_str()));
        if (!(rate >= -720.0f && rate <= 720.0f))
        {
            result = "the turntable turns -720 to 720 degrees a second (0 stops it)";
            return false;
        }
        m_turntable = rate;
        QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&m_turntableLast));
        char text[96];
        std::snprintf(text, sizeof(text), rate == 0.0f ? "the turntable stopped" : "the turntable turns %g degrees a second", rate);
        result = text;
        return true;
    }
    if (action == "camera")
    {
        // Through one of the preview's cameras. `argument` picks which.
        std::vector<IObject*> cameras;
        std::vector<IObject*> open;
        open.push_back(preview);
        while (!open.empty() && cameras.size() < 32)
        {
            IObject* o = open.back();
            open.pop_back();
            for (IObject* child = o->getChild(); child; child = child->getSibling())
            {
                const IObjectTemplate* t = child->getTemplate();
                if (t && t->getClassID() == dice::hfe::world::CID_CameraTemplate)
                    cameras.push_back(child);
                open.push_back(child);
            }
        }
        const int which = argument.empty() ? 0 : std::atoi(argument.c_str());
        if (cameras.empty() || which < 0 || which >= static_cast<int>(cameras.size()))
        {
            char text[96];
            std::snprintf(text, sizeof(text), "the preview has %u camera%s", static_cast<unsigned>(cameras.size()),
                          cameras.size() == 1 ? "" : "s");
            result = cameras.empty() ? m_previewTemplate + " has no Camera child to look through" : std::string(text);
            return false;
        }
        const dice::hfe::Mat4& m = cameras[static_cast<size_t>(which)]->getAbsoluteTransformation();
        const float fx = m.m[8], fy = m.m[9], fz = m.m[10];
        const float yaw = std::atan2(fx, fz) * 57.29578f;
        const float pitch = std::atan2(fy, std::sqrt(fx * fx + fz * fz)) * 57.29578f;
        m_camera.place(m.m[12], m.m[13], m.m[14], yaw, pitch);
        char text[200];
        std::snprintf(text, sizeof(text), "the camera looks through %s (camera %d of %u) at %.2f/%.2f/%.2f",
                      cameras[static_cast<size_t>(which)]->getTemplate()->getName().c_str(), which,
                      static_cast<unsigned>(cameras.size()), m.m[12], m.m[13], m.m[14]);
        result = text;
        return true;
    }
    if (action == "showParts")
    {
        m_previewParts = std::atoi(argument.c_str()) != 0;
        if (!m_previewParts)
            m_pickedPart = -1;
        result = m_previewParts ? "the preview's parts are shown: a click on one picks it, and the gizmo moves it"
                                : "the preview's parts are hidden";
        return true;
    }
    if (action == "parts")
    {
        // One part a row: its number, what it is, where it is in the world and in its line's frame.
        const std::vector<PreviewPart> parts = previewParts();
        result = m_previewTemplate + ": " + std::to_string(parts.size()) + " part" + (parts.size() == 1 ? "" : "s");
        for (size_t i = 0; i < parts.size(); ++i)
        {
            char where[96];
            std::snprintf(where, sizeof(where), " at %.2f/%.2f/%.2f", parts[i].world.m[12], parts[i].world.m[13],
                          parts[i].world.m[14]);
            const std::string what = parts[i].kind == PreviewPart::Child
                                         ? parts[i].child + " of " + parts[i].owner + (parts[i].seat ? " (seat)" : "")
                                               + (parts[i].camera ? " (camera)" : "")
                                         : parts[i].label + " of " + parts[i].owner;
            result += (i ? "; " : ": ") + std::to_string(i) + " " + what + where;
        }
        return true;
    }
    if (action == "pick")
    {
        const int index = std::atoi(argument.c_str());
        const std::vector<PreviewPart> parts = previewParts();
        if (index >= static_cast<int>(parts.size()))
        {
            result = "the preview has " + std::to_string(parts.size()) + " parts";
            return false;
        }
        m_pickedPart = index < 0 ? -1 : index;
        m_previewParts = m_previewParts || index >= 0;
        result = index < 0 ? std::string("no part picked")
                           : "picked part " + std::to_string(index) + ": "
                                 + (parts[static_cast<size_t>(index)].kind == PreviewPart::Child
                                        ? parts[static_cast<size_t>(index)].child
                                        : parts[static_cast<size_t>(index)].label)
                                 + " of " + parts[static_cast<size_t>(index)].owner;
        return true;
    }
    if (action == "movePart" || action == "turnPart")
    {
        // "<n> <dx/dy/dz>": the part moved in the world by that much; "<n> <degrees>": turned about the world's up -
        // as the view's gizmo moves and turns it.
        const size_t blank = argument.find(' ');
        const std::vector<PreviewPart> parts = previewParts();
        const int index = std::atoi(argument.c_str());
        if (blank == std::string::npos || index < 0 || index >= static_cast<int>(parts.size()))
        {
            result = "EditorPreview." + action + " takes a part's number (EditorPreview.parts) and "
                     + (action == "movePart" ? "x/y/z metres" : "degrees");
            return false;
        }
        dice::hfe::Mat4 world = parts[static_cast<size_t>(index)].world;
        if (action == "movePart")
        {
            float delta[3] = { 0.0f, 0.0f, 0.0f };
            std::sscanf(argument.c_str() + blank + 1, "%f/%f/%f", &delta[0], &delta[1], &delta[2]);
            for (int k = 0; k < 3; ++k)
                world.m[12 + k] += delta[k];
        }
        else
        {
            if (parts[static_cast<size_t>(index)].kind != PreviewPart::Child)
            {
                result = "an exit point has only a position to move";
                return false;
            }
            const float radians = static_cast<float>(std::atof(argument.c_str() + blank + 1)) * 0.0174532925f;
            const float c = std::cos(radians), s = std::sin(radians);
            // Its axes turned about the world's up, its place kept.
            for (int row = 0; row < 3; ++row)
            {
                const float x = world.m[row * 4 + 0], z = world.m[row * 4 + 2];
                world.m[row * 4 + 0] = x * c + z * s;
                world.m[row * 4 + 2] = -x * s + z * c;
            }
        }
        m_pickedPart = index;
        return movePreviewPart(static_cast<size_t>(index), world, result);
    }
    if (action == "info")
    {
        const dice::hfe::Mat4& m = preview->getAbsoluteTransformation();
        char text[200];
        std::snprintf(text, sizeof(text), " #%u at %.2f/%.2f/%.2f, yaw %.1f, radius %.2f, turntable %g deg/s", m_preview,
                      m.m[12], m.m[13], m.m[14], m_previewYaw, preview->getVisualBoundingRadius(), m_turntable);
        result = m_previewTemplate + text;
        return true;
    }
    result = "no preview action '" + action + "'";
    return false;
}

void EditSession::updatePreview()
{
    // The turntable: the preview turned about its upright by the rate times the time since the last frame.
    if (m_turntable == 0.0f || !m_preview)
        return;
    IObject* preview = object(m_preview);
    long long now = 0, frequency = 0;
    QueryPerformanceCounter(reinterpret_cast<LARGE_INTEGER*>(&now));
    QueryPerformanceFrequency(reinterpret_cast<LARGE_INTEGER*>(&frequency));
    const double seconds = frequency ? static_cast<double>(now - m_turntableLast) / static_cast<double>(frequency) : 0.0;
    m_turntableLast = now;
    if (!preview || seconds <= 0.0 || seconds > 1.0)
        return;
    m_previewYaw += m_turntable * static_cast<float>(seconds);
    while (m_previewYaw > 180.0f)
        m_previewYaw -= 360.0f;
    while (m_previewYaw <= -180.0f)
        m_previewYaw += 360.0f;
    dice::hfe::Mat4 m = preview->getAbsoluteTransformation();
    dice::hfe::Vec3 rotation = rotationOf(m);
    rotation.x = m_previewYaw;
    setRotation(m, rotation);
    preview->setAbsoluteTransformation(m);
}

} // namespace bf2editor
