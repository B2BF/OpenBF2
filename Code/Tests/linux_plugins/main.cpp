// [TEMP] [CHOSEN, not recovered] Strong globals and weak COMDAT stay image-private.
#include "image.h"
#include <cstdio>
#include <typeinfo>
extern "C" bf2PluginFixture::Image* hostImage();
extern "C" bf2PluginFixture::Image* pluginA();
extern "C" bf2PluginFixture::Image* pluginB();

int main() {
    auto* host=hostImage(); auto* a=pluginA(); auto* b=pluginB();
    const auto* hostType=&typeid(*host); const auto* aType=&typeid(*a); const auto* bType=&typeid(*b);
    if(hostType==aType || hostType==bType || aType==bType) {
        std::fputs("FAIL: RTTI COMDAT coalesced between images\n",stderr); return 1;
    }
    if(host->factor()!=1 || a->factor()!=10 || b->factor()!=100) {
        std::fputs("FAIL: virtual implementations coalesced between images\n",stderr); return 1;
    }
    if(host->advance(1)!=101 || a->advance(1)!=1010 || b->advance(1)!=10100 ||
       b->advance(2)!=10300 || host->advance(3)!=104 || a->advance(4)!=1050 ||
       bf2PluginFixture::imageState!=104) {
        std::fputs("FAIL: host/plugin global state leaked between images\n",stderr); return 1;
    }
    std::puts("PASS: host and two ELF plugins retain separate globals, virtual methods, vtables and RTTI");
    return 0;
}
