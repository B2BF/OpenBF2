// [CHOSEN, not recovered] Cached native movie images retain CPU pixels and bind
// separately in each player's texture registry. Native cache lifetime follows
// issue #7. No retail layout is exposed.
#ifndef BF2_SWIFF_BITMAP_CACHE_H
#define BF2_SWIFF_BITMAP_CACHE_H

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

#include "../../../Menu/ISwiffRender.h"
#include "base/image.h"
#include "gameswf/gameswf.h"

namespace dice { namespace hfe { namespace ui {

class SwiffBitmapRegistry;

// The saved pixels precede the renderer's destructive RGBA channel conversion.
struct SwiffBitmapPixels : gameswf::bitmap_info::render_resource
{
    enum Format { Alpha, Rgba };
    Format format;
    int width, height, pitch;
    std::vector<unsigned char> bytes;
    std::vector<std::weak_ptr<SwiffBitmapRegistry> > registries;

    SwiffBitmapPixels(Format f, int w, int h, int p, const unsigned char* data)
        : format(f), width(w), height(h), pitch(p), bytes(data, data + p * h) {}
    ~SwiffBitmapPixels() override;
};

// A player owns this lease; cached descriptors keep only weak references to it.
// Retaining the renderer keeps its registry valid through menu-disable ordering.
class SwiffBitmapRegistry
{
    ISwiffRender* m_renderer;
    std::map<const SwiffBitmapPixels*, int> m_handles;

public:
    explicit SwiffBitmapRegistry(ISwiffRender* renderer) : m_renderer(renderer)
    {
        m_renderer->addRef();
    }
    SwiffBitmapRegistry(const SwiffBitmapRegistry&) = delete;
    SwiffBitmapRegistry& operator=(const SwiffBitmapRegistry&) = delete;
    ~SwiffBitmapRegistry()
    {
        for (const auto& entry : m_handles)
            m_renderer->releaseTexture(entry.second);
        m_renderer->release();
    }

    void erase(const SwiffBitmapPixels* pixels)
    {
        const auto found = m_handles.find(pixels);
        if (found != m_handles.end())
        {
            m_renderer->releaseTexture(found->second);
            m_handles.erase(found);
        }
    }

    // A failed upload remains -1 for this lease, just like the retail descriptor.
    static void bind(gameswf::bitmap_info* bitmap,
                     const std::shared_ptr<SwiffBitmapRegistry>& registry)
    {
        if (!bitmap || !bitmap->m_render_resource)
            return;
        SwiffBitmapPixels& pixels = static_cast<SwiffBitmapPixels&>(*bitmap->m_render_resource);
        auto found = registry->m_handles.find(&pixels);
        if (found == registry->m_handles.end())
        {
            int handle;
            if (pixels.format == SwiffBitmapPixels::Alpha)
                handle = registry->m_renderer->uploadAlpha(pixels.width, pixels.height, pixels.bytes.data());
            else
            {
                // RendDX9's RGBA upload swaps R/B in place. Every registry must
                // receive the original RGBA bytes, including after a reload.
                std::vector<unsigned char> upload(pixels.bytes);
                image::image_base image(upload.data(), pixels.width, pixels.height, pixels.pitch);
                handle = registry->m_renderer->uploadRgba(reinterpret_cast<unsigned int*>(&image));
            }
            found = registry->m_handles.emplace(&pixels, handle).first;
            auto& owners = pixels.registries;
            owners.erase(std::remove_if(owners.begin(), owners.end(),
                [](const std::weak_ptr<SwiffBitmapRegistry>& owner) { return owner.expired(); }), owners.end());
            owners.push_back(registry);
        }
        bitmap->m_texture_id = static_cast<unsigned int>(found->second);
    }
};

inline SwiffBitmapPixels::~SwiffBitmapPixels()
{
    for (const auto& owner : registries)
        if (const auto registry = owner.lock())
            registry->erase(this);
}

} } }
#endif
