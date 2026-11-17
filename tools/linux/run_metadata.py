"""Read-only, opt-in run evidence for the issue #36 Linux client launcher."""
from datetime import datetime, timezone
import hashlib
import os
from pathlib import Path
import re
import stat


_QUALITY = r"[+-]?[0-9]{1,10}"
_VIDEO = {
    **{f"VideoSettings.set{name}Quality": _QUALITY for name in (
        "Terrain", "Geometry", "Lighting", "DynamicLighting", "DynamicShadows",
        "Effects", "Texture", "TextureFiltering")},
    "VideoSettings.setResolution": r"[0-9]{1,5}x[0-9]{1,5}@[0-9]{1,4}Hz",
    "VideoSettings.setAntialiasing": r"(?:Off|[0-9]{1,2}Samples)",
    "VideoSettings.setViewDistanceScale": r"[+-]?(?:[0-9]+(?:\.[0-9]*)?|\.[0-9]+)(?:[eE][+-]?[0-9]+)?",
    "VideoSettings.setVideoOptionScheme": _QUALITY,
}
_VIDEO_NAMES = {name.casefold(): name for name in _VIDEO}


def _stamp():
    return datetime.now(timezone.utc).isoformat()


def _error(path, error):
    return {"path": str(path), "error": type(error).__name__, "errno": error.errno}


def _small_file(path):
    if not stat.S_ISREG(path.stat().st_mode):
        raise OSError("metadata source is not a regular file")
    with path.open("rb") as source:
        data = source.read(1024 * 1024 + 1)
    if len(data) > 1024 * 1024:
        raise OSError("metadata file exceeds 1 MiB")
    return data


def video_snapshot(state):
    """Record only saved video commands; these do not establish the applied client state."""
    root = state / "OpenBF2/Profiles"
    result = {"utc": _stamp(), "meaning": "saved settings, not verified applied settings",
              "profiles": {}, "errors": []}
    if not root.exists():
        result["status"] = "profiles absent"
        return result
    try:
        profiles = sorted(root.iterdir())
    except OSError as error:
        result["errors"].append(_error(root, error))
        return result
    for profile in profiles:
        if profile.name != "Default" and not re.fullmatch(r"[0-9]+", profile.name):
            continue
        path = profile / "Video.con"
        if profile.is_symlink() or path.is_symlink():
            result["errors"].append({"path": f"{profile.name}/Video.con", "error": "symlink skipped"})
            continue
        if not path.exists():
            continue
        try:
            data = _small_file(path)
            commands = []
            for number, line in enumerate(data.decode("utf-8", errors="replace").splitlines(), 1):
                fields = line.split(maxsplit=1)
                name = _VIDEO_NAMES.get(fields[0].casefold()) if fields else None
                if name and len(fields) == 2:
                    value = fields[1].strip()
                    if value.startswith('"') and value.endswith('"'):
                        value = value[1:-1]
                    if len(value) <= 64 and re.fullmatch(_VIDEO[name], value, re.I):
                        commands.append({"line": number, "command": name, "value": value})
            result["profiles"][profile.name] = {"file": f"{profile.name}/Video.con",
                "sha256": hashlib.sha256(data).hexdigest(), "commands": commands}
        except OSError as error:
            result["errors"].append(_error(path, error))
    return result


def power_snapshot(sysfs=Path("/sys")):
    """Read available system power settings; missing fields remain explicit unknowns."""
    result = {"utc": _stamp(), "meaning": "instantaneous settings, not an energy or thermal measurement",
              "fields": [], "errors": [], "unknown": []}

    def read(path, unit="text", optional=False):
        try:
            value = path.read_text().strip()
            if len(value) > 128 or "\n" in value:
                raise OSError("invalid sysfs field")
            if unit != "text":
                value = int(value)
            result["fields"].append({"path": str(path.relative_to(sysfs)), "value": value, "unit": unit})
            return value
        except (OSError, ValueError, UnicodeError) as error:
            if not optional or not isinstance(error, FileNotFoundError):
                result["errors"].append({"path": str(path.relative_to(sysfs)),
                    "error": type(error).__name__, "errno": getattr(error, "errno", None)})
            return None

    supplies = sysfs / "class/power_supply"
    for device in sorted(supplies.glob("*")):
        try:
            kind = (device / "type").read_text().strip()
            scope_file = device / "scope"
            scope = scope_file.read_text().strip() if scope_file.exists() else None
        except OSError as error:
            result["errors"].append(_error(device / "type", error))
            continue
        # Peripheral batteries can expose device identifiers in their sysfs names.
        if scope == "Device" or kind == "Battery" and scope != "System" and not re.fullmatch(r"BAT[0-9]+", device.name):
            continue
        if kind in ("Mains", "USB", "USB_C", "USB_PD"):
            read(device / "online", "boolean")
        elif kind == "Battery":
            read(device / "status")
            read(device / "capacity", "percent")
    for path in sorted(sysfs.glob("devices/system/cpu/cpu[0-9]*/cpufreq/scaling_governor")):
        read(path)
    for path in sorted(sysfs.glob("class/drm/card[0-9]*/device/power_dpm_force_performance_level")):
        read(path)
    for device in sorted(sysfs.glob("class/hwmon/hwmon*")):
        caps = sorted(device.glob("power*_cap")) + sorted(device.glob("power*_cap_max"))
        if caps:
            read(device / "name", optional=True)
            for path in caps:
                read(path, "microwatts")
    paths = [field["path"] for field in result["fields"]]
    for label, needle in (("AC/battery status", "power_supply/"), ("CPU governor", "scaling_governor"),
                          ("GPU power policy", "power_dpm_force_performance_level"), ("power cap", "_cap")):
        if not any(needle in path for path in paths):
            result["unknown"].append(label)
    return result


def cache_snapshot(cache):
    """Count regular files without reading cache contents or following directory symlinks."""
    result = {"utc": _stamp(), "files": 0, "bytes": 0, "skipped_symlinks": 0, "errors": [],
              "meaning": "counts and sizes do not establish cache validity or warmth"}
    def error(error):
        result["errors"].append(_error(error.filename, error))
    for directory, dirs, files in os.walk(cache, followlinks=False, onerror=error):
        linked = [name for name in dirs if (Path(directory) / name).is_symlink()]
        result["skipped_symlinks"] += len(linked)
        dirs[:] = [name for name in dirs if name not in linked]
        for name in files:
            path = Path(directory) / name
            try:
                info = path.lstat()
                if stat.S_ISLNK(info.st_mode):
                    result["skipped_symlinks"] += 1
                elif stat.S_ISREG(info.st_mode):
                    result["files"] += 1
                    result["bytes"] += info.st_size
            except OSError as error:
                result["errors"].append(_error(path, error))
    return result


def graphics_overrides(env, content):
    keys = ("DXVK_CONFIG_FILE", "DXVK_FILTER_DEVICE_NAME", "DXVK_FILTER_DEVICE_UUID",
            "DXVK_LOG_LEVEL", "DXVK_NUM_COMPILER_THREADS", "MESA_SHADER_CACHE_DISABLE",
            "MESA_SHADER_CACHE_MAX_SIZE", "MESA_SHADER_CACHE_SHOW_STATS", "MESA_SHADER_CACHE_SINGLE_FILE",
            "MESA_SHADER_CACHE_MULTI_FILE", "DRI_PRIME", "MESA_VK_DEVICE_SELECT")
    result = {"environment": {key: env.get(key) for key in keys}}
    inline = env.get("DXVK_CONFIG")
    result["DXVK_CONFIG"] = {"present": inline is not None,
        "sha256": hashlib.sha256(inline.encode()).hexdigest() if inline is not None else None}
    path = Path(env.get("DXVK_CONFIG_FILE") or "dxvk.conf")
    if not path.is_absolute():
        path = content / path
    record = {"path": str(path), "selection": "environment" if env.get("DXVK_CONFIG_FILE") else "default candidate"}
    try:
        record["sha256"] = hashlib.sha256(_small_file(path)).hexdigest()
    except FileNotFoundError:
        record["status"] = "absent"
    except OSError as error:
        record["error"] = _error(path, error)
    result["config_file"] = record
    return result


def graphics_evidence(run):
    """Retain selected actual device/presenter log lines, including their source locations."""
    result = {"records": [], "errors": []}
    for path in sorted(run.glob("*_d3d9.log")) + [run / "client.log"]:
        if not path.is_file():
            continue
        try:
            with path.open(errors="replace") as source:
                selected_device, presenter = False, False
                for number, raw in enumerate(source, 1):
                    text = raw.rstrip("\r\n")
                    message = re.sub(r"^info:\s*", "", text)
                    kind = None
                    if "copyDisplayModeFrom:" in message:
                        kind = "requested_display_mode"
                        text = message[message.index("copyDisplayModeFrom:"):]
                    elif selected_device:
                        kind = "selected_device"
                        selected_device = False
                    elif message == "Creating device:":
                        selected_device = True
                    elif message == "Presenter: Actual swapchain properties:":
                        kind, presenter = "actual_presenter", True
                    elif presenter and re.match(r"^(Format|Color space|Present mode|Buffer size|Image count):", message):
                        kind = "actual_presenter"
                    else:
                        presenter = False
                        if message.startswith(("DXVK:", "Build:", "Found device:")) or re.match(r"Driver\s*:", message):
                            kind = "device_driver"
                    if kind:
                        result["records"].append({"file": path.name, "line": number, "kind": kind, "text": text})
        except OSError as error:
            result["errors"].append(_error(path, error))
    result["status"] = "reported log evidence" if result["records"] else "unknown; no matching device/presenter log evidence"
    return result
