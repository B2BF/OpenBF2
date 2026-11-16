// [TEMP] Records the production native window translator's output without an engine window.
#ifndef TESTS_NATIVE_HOST_WINDOWMESSAGERECORDER_H
#define TESTS_NATIVE_HOST_WINDOWMESSAGERECORDER_H
#include "../../BF2/Platform/Host/WindowMessageSource.h"
#include <vector>

struct WindowMessageRecorder : dice::hfe::host::WindowMessageSource
{
    struct Message {
        dice::hfe::UINT id;
        dice::hfe::WPARAM wParam;
        dice::hfe::LPARAM lParam;
    };
    std::vector<Message> messages;
    std::vector<unsigned int> values(dice::hfe::UINT id) const
    {
        std::vector<unsigned int> result;
        for (const auto& message : messages)
            if (message.id == id) result.push_back(static_cast<unsigned int>(message.wParam));
        return result;
    }
protected:
    void send(dice::hfe::UINT id, dice::hfe::WPARAM wParam, dice::hfe::LPARAM lParam) override
    { messages.push_back({id, wParam, lParam}); }
};
#endif
