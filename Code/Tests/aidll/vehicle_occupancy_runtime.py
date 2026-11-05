"""[TEMP] Whole production occupancy/manager TUs with chosen native forwarding observers."""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import sys

CODE = Path(__file__).resolve().parents[2]
MODES = ('low_words', 'invalid_initial', 'null_primary_unit', 'occupied_primary',
         'empty_secondary', 'first_zero_null_manager', 'next_zero_null_manager',
         'resolved_primary', 'fresh_primary_manager', 'fresh_first_manager',
         'fresh_next_manager', 'secondary_walk', 'secondary_generation_end',
         'primary_live_unit', 'retained_primary')


def record(out):
    out.mkdir(parents=True, exist_ok=True)
    paths = ('AI/AIDLL/AI/AIUtils/VehicleOccupancy.cpp',
             'AI/AIDLL/AI/AIUtils/VehicleOccupancy.h',
             'AI/AIDLL/AI/Main/InformationManager.cpp',
             'AI/AIDLL/AI/Main/InformationManager.h',
             'AI/AIDLL/AI/AIKnowledge/Information.h',
             'AI/AIDLL/AI/AIKnowledge/IPIUnit.h', 'Core/SpHandlePool.h')
    evidence = []
    for relative in paths:
        path = CODE / 'BF2' / relative
        evidence.append({'source': path.as_posix(), 'source_sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    (out / 'source.json').write_text(json.dumps(evidence, indent=2), encoding='utf-8')


def run(exe):
    summary = []
    count = 0
    for mode in MODES:
        result = subprocess.run([str(exe), mode], capture_output=True, text=True, timeout=15)
        output = result.stdout + result.stderr
        (exe.parent / (mode + '.log')).write_text(output, encoding='utf-8')
        match = re.fullmatch(r'PASS ' + mode + r' ([1-9][0-9]*) checks\n', output)
        if result.returncode != 0 or not match:
            raise RuntimeError((mode, result.returncode, output))
        count += int(match.group(1))
        summary.append(output.strip())
        print(summary[-1])
    summary.append('PASS vehicle occupancy: ' + str(len(MODES)) + ' strict x86 cases / ' + str(count) + ' checks')
    (exe.parent / 'runtime.log').write_text('\n'.join(summary) + '\n', encoding='utf-8')
    print(summary[-1])


if __name__ == '__main__':
    if len(sys.argv) != 3 or sys.argv[1] not in ('record', 'run'):
        raise SystemExit('Usage: vehicle_occupancy_runtime.py record OUTPUT | run EXE')
    target = Path(sys.argv[2]).resolve()
    record(target) if sys.argv[1] == 'record' else run(target)
