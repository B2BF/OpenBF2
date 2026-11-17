// [TEMP] [CHOSEN, not recovered] Same C++ names deliberately occur in each image.
#pragma once
#ifndef FIXTURE_FACTOR
#define FIXTURE_FACTOR 1
#endif
namespace bf2PluginFixture {
extern int imageState;
class Image {
public:
    virtual ~Image() {}
    __attribute__((noinline)) virtual int advance(int delta) { imageState+=delta*FIXTURE_FACTOR; return imageState; }
    __attribute__((noinline)) virtual int factor() const { return FIXTURE_FACTOR; }
};
}
