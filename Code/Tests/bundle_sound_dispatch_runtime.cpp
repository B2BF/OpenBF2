// [TEMP] Test scaffolding. Reuses runtime-pair collaborators and its production source manifest;
// only the portable bundle/sound interface boundary runs here. No engine services are started.
#define BF2_BUNDLE_SOUND_DISPATCH_ONLY 1
#include "runtime_pair.cpp"

namespace {

struct InterfacePrefix
{
    virtual void unused() { std::abort(); }
    int padding = 17;
};

// A genuine, adjusted canonical interface, not a parallel declaration with matching slots.
class AdjustedSoundProbe : public InterfacePrefix, public SoundTemplateProbe
{
public:
    explicit AdjustedSoundProbe(std::vector<std::string>* events) : SoundTemplateProbe(events) {}
};

class NullChildTemplate : public ProbeTemplate
{
public:
    NullChildTemplate(std::vector<std::string>* events, SoundTemplateProbe* sound)
        : ProbeTemplate(events, sound), m_events(events) {}

    IObject* createObject() const override
    {
        m_events->push_back("createObject");
        ++m_createCalls;
        // This is the sound-template contract: it configures sound but creates no world object.
        return 0;
    }

private:
    std::vector<std::string>* m_events;
};

void soundCases(Checks& checks)
{
    std::vector<std::string> events;
    AdjustedSoundProbe sound(&events);
    NullChildTemplate child(&events, &sound);
    HarnessBundleTemplate bundle;
    bundle.addTemplate("BundleSoundDispatch");
    bundle.cacheTemplate(0, child);
    Mat4 transform = Mat4::getIdentity();
    for (int i = 0; i != 16; ++i) transform.m[i] = float(i * 3 - 11);
    bundle.setTransformation(transform, 0);

    checks.expect(static_cast<void*>(static_cast<dice::hfe::ISoundObjectTemplate*>(&sound)) !=
                      static_cast<void*>(&sound), "sound interface is an adjusted secondary base");
    // Null is safe here precisely because the real sound-template createObject contract is null.
    bundle.addBundleChilds(0);
    checks.expect(events == std::vector<std::string>{"soundRelativeTransformation", "createObject"},
                  "canonical sound transformation precedes null child creation");
    checks.expect(sound.m_relativeCalls == 1 && child.m_createCalls == 1,
                  "both sound operations run once");
    checks.expect(sound.m_lastPointer == &bundle.getTransformation(0),
                  "dispatch passes the original TemplateInfo matrix address");
    for (int i = 0; i != 16; ++i)
        checks.expect(sound.m_lastRelative.m[i] == transform.m[i], "matrix value is preserved");
    checks.expect(!bundle.relativeOffsetApplied(0), "null sound child does not apply object offsets");

    events.clear();
    transform.m[12] = 92.0f;
    bundle.setTransformation(transform, 0);
    bundle.addBundleChilds(0);
    checks.expect(sound.m_relativeCalls == 2 && child.m_createCalls == 2,
                  "cached repeated assembly dispatches both operations again");
    checks.expect(sound.m_lastRelative.m[12] == 92.0f,
                  "repeated assembly sees the new matrix rather than stale copied data");
    checks.expect(events == std::vector<std::string>{"soundRelativeTransformation", "createObject"},
                  "repeated assembly preserves ordering");

    events.clear();
    NullChildTemplate plain(&events, 0);
    HarnessBundleTemplate noSound;
    noSound.addTemplate("BundleWithoutSoundInterface");
    noSound.cacheTemplate(0, plain);
    noSound.addBundleChilds(0);
    checks.expect(events == std::vector<std::string>{"createObject"},
                  "missing optional sound interface still reaches createObject");
    checks.expect(plain.m_createCalls == 1 && sound.m_relativeCalls == 2,
                  "missing sound interface performs no sound dispatch");
}

class LocalDriverHost : public DriverHost
{
public:
    LocalDriverHost(const IObjectTemplate* objectTemplate, PlayerControlProbe* control)
        : DriverHost(objectTemplate, control)
    {
        // No object manager or engine lifecycle participates in this interface-only fixture.
        m_flags = 0;
        m_objectCacheId = 0;
    }
    ~LocalDriverHost() { m_isObjectDestroyed = true; }
};

class AdjustedPlayerControlProbe : public InterfacePrefix, public PlayerControlProbe {};

void decalCases(Checks& checks)
{
    std::vector<std::string> events;
    DecalTemplateProbe decal(&events);
    HarnessBundleTemplate bundle;
    bundle.enableCollisionPhysics();
    bundle.addTemplate("BundleDriverDecalDispatch");
    bundle.cacheTemplate(0, decal);
    Mat4 transform = Mat4::getIdentity();
    transform.m[12] = 12.0f;
    transform.m[13] = -13.0f;
    transform.m[14] = 14.0f;
    bundle.setTransformation(transform, 0);
    AdjustedPlayerControlProbe control;
    LocalDriverHost object(&bundle, &control);
    checks.expect(static_cast<void*>(static_cast<dice::hfe::world::IPlayerControlObject*>(&control)) !=
                      static_cast<void*>(&control), "player-control interface is an adjusted secondary base");
    bundle.addDriverDecals(&object);
    checks.expect(decal.m_classIdCalls == 2, "driver decal preserves both class-id queries");
    checks.expect(decal.m_createCalls == 0, "driver decal does not instantiate an object");
    checks.expect(control.m_setCalls == 1 && control.m_driverObject == &object,
                  "canonical player-control setter receives the supplied object");
    checks.expect(control.m_transformCalls == 1, "driver decal appends one transformation");
    for (int i = 0; i != 16; ++i)
        checks.expect(control.m_lastTransformation.m[i] == transform.m[i],
                      "driver decal matrix value is preserved");
}

} // namespace

int main(int argc, char** argv)
{
    const std::string mode = argc == 2 ? argv[1] : "all";
    if (argc > 2 || (mode != "sound" && mode != "decal" && mode != "all")) return 2;
    Checks checks;
    if (mode != "decal") soundCases(checks);
    if (mode != "sound") decalCases(checks);
    std::cout << "bundle sound dispatch: " << checks.checks() << " checks, "
              << checks.failures() << " failures" << std::endl;
    return checks.failures() ? 1 : 0;
}
