// Path/name [INFERRED]. PE83B430,83B7D0,83B850,83B890 read in full.
#include "EventQueue.h"
#if !DEDICATED
#include "InputNodeVisitors.h"
#include "System.h"
#include <new>
namespace dice { namespace meme {
QueuedEvent::QueuedEvent(int eventType, int eventIndex, float a, float b, std::uint32_t payload)
    : next(nullptr), type(eventType), index(eventIndex), first(a), second(b), field14(payload) {}
namespace {
QueuedEvent* allocateEvent()
{
    void* allocation = ::operator new(sizeof(QueuedEvent));
    return allocation ? new(allocation) QueuedEvent(0, 0, 0.0f, 0.0f, 0) : nullptr;
}
}
EventQueue::EventQueue(unsigned int additionalNodes) : m_first(nullptr), m_last(nullptr)
{
    m_free = allocateEvent();
    QueuedEvent* node = m_free;
    if (additionalNodes) {
        do {
            QueuedEvent* next = allocateEvent();
            node->next = next; // Retail does not guard a failed allocation here.
            node = next;
        } while (--additionalNodes);
    }
}
EventQueue::~EventQueue()
{
    QueuedEvent* node = m_free;
    while (node) {
        QueuedEvent* next = node->next;
        ::operator delete(node);
        node = next;
    }
    node = m_first;
    while (node) {
        QueuedEvent* next = node->next;
        ::operator delete(node);
        node = next;
    }
}
void EventQueue::enqueue(System*, const QueuedEvent& event)
{
    if (!m_free) m_free = allocateEvent();
    if (!m_first) {
        m_last = m_free;
        m_first = m_free;
        m_free = m_free->next;
        m_last->next = nullptr;
    } else {
        m_last->next = m_free;
        m_free = m_free->next;
        m_last = m_last->next;
        m_last->next = nullptr;
    }
    *m_last = event; // All six words copied, including next; do not clear it again.
}
void EventQueue::processEvent1(System* system, int first, int index, int second)
{
    if (second == 1 && first == 1 && !system->getLockCount()) {
        PressStateNodeVisitor visitor;
        NodeState* state = system->getNodeState();
        const Rect* rect = system->getBounds();
        visitor.NodeVisitor::traverse(system, system->getRootNode(), rect, state);
    }
    Event1NodeVisitor visitor(first, index, second);
    NodeState* state = system->getNodeState();
    const Rect* rect = system->getBounds();
    visitor.NodeVisitor::traverse(system, system->getRootNode(), rect, state);
}
void EventQueue::processEvent2(System* system, int first, int index)
{
    Event2NodeVisitor visitor(first, index);
    NodeState* state = system->getNodeState();
    const Rect* rect = system->getBounds();
    visitor.NodeVisitor::traverse(system, system->getRootNode(), rect, state);
    if (first == 1) {
        ReleaseStateNodeVisitor release;
        state = system->getNodeState();
        rect = system->getBounds();
        release.NodeVisitor::traverse(system, system->getRootNode(), rect, state);
    }
}
void EventQueue::processEvent4(System* system, int first, int index, int second)
{
    Event4NodeVisitor visitor(first, index, second);
    NodeState* state = system->getNodeState();
    const Rect* rect = system->getBounds();
    visitor.NodeVisitor::traverse(system, system->getRootNode(), rect, state);
}
bool EventQueue::process(System* system)
{
    while (m_first) {
        QueuedEvent* saved = m_first;
        switch (saved->type) {
        case 1: {
            const int second = static_cast<int>(saved->second);
            const int index = saved->index;
            const int first = static_cast<int>(saved->first);
            processEvent1(system, first, index, second);
            break;
        }
        case 2: {
            const int index = saved->index;
            const int first = static_cast<int>(saved->first);
            processEvent2(system, first, index);
            break;
        }
        case 4: {
            const int second = static_cast<int>(saved->second);
            const int index = saved->index;
            const int first = static_cast<int>(saved->first);
            processEvent4(system, first, index, second);
            break;
        }
        case 5: {
            Event5NodeVisitor visitor(static_cast<int>(saved->first));
            NodeState* state = system->getNodeState();
            const Rect* rect = system->getBounds();
            visitor.NodeVisitor::traverse(system, system->getRootNode(), rect, state);
            break;
        }
        default: break; // Includes event3: it still causes the zero-delta update.
        }
        if (system->getRootNode()) {
            Node* node = system->getRootNode();
            const Rect* rect = system->getBounds();
            const NodeState* state = system->getNodeState();
            node->update(system, state, rect, 0.0f); // Return is deliberately ignored.
        }
        // Dispatch can mutate the pending chain. Retail tests/reloads the live
        // head, but recycles the saved node. This early return leaves m_last.
        if (!m_first) return true;
        m_first = m_first->next;
        saved->next = m_free;
        m_free = saved;
    }
    m_last = nullptr;
    return true;
}
#if defined(_M_IX86)
static_assert(sizeof(QueuedEvent) == 24, "client queue record");
static_assert(sizeof(EventQueue) == 12, "client queue owner");
#endif
} }
#endif
