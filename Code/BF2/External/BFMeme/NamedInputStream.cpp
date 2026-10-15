// All distinct instructions read from BF2.exe before writing. The dedicated
// ELF has no client BFMeme counterpart. No stream descriptor is published.
#include "NamedInputStream.h"
#if !DEDICATED
#include "OStream.h"
namespace dice { namespace meme {
TeeIStream::TeeIStream(IStream* input, OStream* output)
    : ForwardingIStream(input, true), m_output(output) {}
TeeIStream::~TeeIStream() {} // base owns input; cache output remains borrowed.
int TeeIStream::read(void* bytes, int count)
{
    const int transferred = ForwardingIStream::read(bytes, count);
    m_output->write(bytes, transferred); // Called even for zero; result ignored.
    return transferred;
}
RelativeIStream::RelativeIStream(IStream* input)
    : ForwardingIStream(input, false)
{
    m_initialPosition = input->getPosition(); // No source null guard.
    m_skippedBytes = 0;
}
RelativeIStream::~RelativeIStream() {} // only the forwarding base destructor.
int RelativeIStream::skip(int bytes)
{
    m_input->m_source = this;
    m_skippedBytes = static_cast<int>(static_cast<unsigned int>(m_skippedBytes)
        + static_cast<unsigned int>(bytes));
    return bytes; // Does not call the underlying skip.
}
int RelativeIStream::read(void* bytes, int count)
{
    m_input->m_source = this;
    return m_input->read(bytes, count);
}
int RelativeIStream::getPosition()
{
    m_input->m_source = this;
    const int position = m_input->getPosition();
    return static_cast<int>(static_cast<unsigned int>(position)
        + static_cast<unsigned int>(m_skippedBytes)
        - static_cast<unsigned int>(m_initialPosition));
}
#if defined(_M_IX86)
static_assert(sizeof(TeeIStream) == 20, "client recording input layout");
static_assert(sizeof(RelativeIStream) == 24, "client relative input layout");
#endif
} }
#endif
