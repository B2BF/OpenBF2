"""[TEMP] Isolate the exact production action body, never rewrite its statements.

Real class declarations and native C++ lifetimes are retained. Fixture lifetimes
and observers are chosen test code; this is not full AIMain construction or AI work.
"""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import sys

CODE = Path(__file__).resolve().parents[2]
AI = "AI/AIDLL/AI/"


def methods(name, relative):
    source = (CODE / "BF2" / relative).read_text(encoding="utf-8")
    pattern = r"^class " + name + r"(?:\s*:\s*[^\{]+)?\s*\{(.*?)^};"
    found = re.findall(pattern, source, re.M | re.S)
    if len(found) != 1:
        raise RuntimeError("Expected one declaration of " + name)
    body = re.sub(r"//[^\n]*", "", found[0])
    for text in re.findall(r"virtual\s+([^;]+);", body):
        text = re.sub(r"\s*=\s*0$", "", " ".join(text.split()))
        method = re.search(r"(~?\w+)\s*\(", text).group(1)
        if not method.startswith("~"):
            yield method, text


def generate(out):
    source_path = CODE / "BF2" / AI / "Main/AIMain.cpp"
    source = source_path.read_text(encoding="utf-8")
    bodies = re.findall(r"^bool AIMain::action\(\)\s*\{[\s\S]*?^}", source, re.M)
    if len(bodies) != 1 or 'BF2_AI_FRONTIER("dice::hfe::ai::AIMain::action")' in bodies[0]:
        raise RuntimeError("Expected one recovered production AIMain::action body")
    out.mkdir(parents=True, exist_ok=True)
    shared = Path(__file__).with_suffix(".h").as_posix()
    (out / "action_actual.cpp").write_text(
        '// [TEMP] Verbatim current production AIMain::action; no statement substitution.\n'
        '#include "' + shared + '"\n'
        'namespace dice { namespace hfe { namespace ai {\n' + bodies[0] + '\n} } }\n', encoding="utf-8")
    (out / "action_source.json").write_text(json.dumps({
        "source": source_path.as_posix(),
        "source_sha256": hashlib.sha256(source.encode()).hexdigest(),
        "body_sha256": hashlib.sha256(bodies[0].encode()).hexdigest(),
        "body": bodies[0],
    }, indent=2), encoding="utf-8")
    generated = ['// [TEMP] Typed observers from real declarations; unused calls abort.',
                 'namespace dice { namespace hfe { namespace ai {']
    specs = [
        ("IBFTimer", "AI/AIInterface/BFKnowledge/BFTimer.h", "ObservedTimer",
         'int identity = 0;', {
             "getTimeIncrease": 'return observeFrame(identity);',
             "getUpdateTime": 'return observeUpdate(identity);',
             "getGameTime": 'return observeGame(identity);'}),
        ("IAISettings", AI + "Main/AISettingsInterfaces.h", "ObservedSettings", '',
         {"getNSides": 'return observeSides();'}),
        ("IBFCollisionHandler", "AI/AIInterface/BFKnowledge/BFCollisionHandler.h", "ObservedCollision", '',
         {"updateBotProjectiles": 'observeCollision();'}),
        ("IAIMineManager", AI + "AIKnowledge/AIMineManager.h", "ObservedMines", '',
         {"update": 'trace("mines");'}),
        ("IAIObjectEnvironment", "AI/AIInterface/BFKnowledge/BFObjectEnvironment.h", "ObservedEnvironment", '',
         {"damageSeverelyDamagedVehicles": 'trace("damage");'}),
        ("Bot", AI + "AIBot/BotMain.h", "ObservedBot", 'int side = 0; int identity = 0;',
         {"getSide": 'trace("side" + std::to_string(identity)); return side;'})]
    for base, relative, name, fields, impl in specs:
        generated.append('class ' + name + ' : public ' + base + ' { public: ' + fields)
        found = set()
        for method, declaration in methods(base, relative):
            if method in impl:
                found.add(method)
            body = impl.get(method, 'unexpected("' + base + '::' + method + '");')
            generated.append('    ' + declaration + ' override { ' + body + ' }')
        if found != set(impl):
            raise RuntimeError("Missing observed methods in " + base)
        generated.append('};')
    for name, relative, excluded in (
        ("AIMain", AI + "Main/AIMain.h", {"action"}),
        ("SAI", AI + "AIStrategic/SAI.h", {"update", "addBot"}),
        ("BotManager", AI + "Main/BotManager.h", set())):
        for method, declaration in methods(name, relative):
            if method in excluded:
                continue
            signature = re.sub(r"\b" + method + r"(?=\s*\()", name + '::' + method, declaration, count=1)
            generated.append(signature + ' { unexpected("' + name + '::' + method + '"); }')
    generated.append('} } }')
    (out / "action_runtime_mocks.inl").write_text('\n'.join(generated), encoding="utf-8")


def run(exe):
    modes = ['empty_disabled', 'queue_disabled', 'sai_wrap', 'cursor_after_count',
             'cached_dependencies', 'negative_remainder', 'sai_multiply_order']
    for mode in modes:
        result = subprocess.run([str(exe), mode], capture_output=True, text=True, timeout=15)
        output = result.stdout + result.stderr
        (exe.parent / (mode + '.log')).write_text(output, encoding="utf-8")
        if result.returncode != 0 or 'PASS ' + mode not in output or 'UNEXPECTED' in output or '[FRONTIER]' in output:
            raise RuntimeError((mode, result.returncode, output))
        print('PASS', mode)
    print('PASS action: 7 isolated strict x86 cases; exact extracted production body, chosen collaborators')


if __name__ == '__main__':
    if len(sys.argv) != 3 or sys.argv[1] not in ('generate', 'run'):
        raise SystemExit('Usage: action_runtime.py generate OUTPUT_DIR | run EXE')
    target = Path(sys.argv[2]).resolve()
    generate(target) if sys.argv[1] == 'generate' else run(target)

