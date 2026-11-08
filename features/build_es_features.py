#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
# This file is part of EmulationStation-RE.
# Copyright (c) 2026 The EmulationStation-RE contributors.
# The feature definitions come from Batocera (https://batocera.org), Copyright (c) 2026 The Batocera Team,
# distributed under the GNU Lesser General Public License, version 3 or later.
#
# Builds es_features.cfg, the file that lists the options of each emulator in the
# "per system advanced configuration" and game options menus.
#
# It reads the *.emulator.yml and *.core.yml files of a batocera.linux checkout, as Batocera's own
# build does (python-src/batocera-es-system), then keeps only:
#   - the emulators that es-launcher has a generator for,
#   - the options that es-launcher reads: an option whose key does not appear in the sources of
#     es-launcher would be shown in the menu and do nothing.
# The emulators Batocera does not have (Eden) are described in the *.emulator.yml files of this folder.
#
# Usage: features/build_es_features.py <batocera.linux checkout>
# Requires ruamel.yaml (YAML 1.2, like Batocera: "On" and "yes" stay strings).

from __future__ import annotations

import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path

import ruamel.yaml

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
LAUNCHER_SOURCES = REPO / 'es-launcher' / 'src'
OUTPUT = HERE / 'es_features.cfg'
PO_OUTPUT = REPO / 'locale' / 'features'

# es-launcher generators (ES_LAUNCHER_REGISTER_GENERATOR) -> names in es_features.cfg.
# "retroarch" is an alias of the libretro generator.
EMULATORS = {
    'libretro': 'libretro, retroarch',
    'azahar': 'azahar',
    'cemu': 'cemu',
    'dolphin': 'dolphin',
    'duckstation': 'duckstation',
    'eden': 'eden',
    'melonds': 'melonds',
    'mupen64plus': 'mupen64plus',
    'supermodel': 'supermodel',
    'stella': 'stella',
    'dosbox_staging': 'dosbox_staging',
    'scummvm': 'scummvm',
    'ymir': 'ymir',
    'pcsx2': 'pcsx2',
    'ppsspp': 'ppsspp',
    'rpcs3': 'rpcs3',
    'xemu': 'xemu',
    'xenia-edge': 'xenia-edge',
}

# Other names es-launcher gives a core (es-launcher/src/retroarch/Cores.cpp): the core name of
# es_systems.cfg is also the name of its library, so systems may use these instead of Batocera's.
CORE_ALIASES = {
    'libretro': {
        'mednafen_psx': ['mednafen_psx_hw'],
        'vba-m': ['vbam'],
        'vb': ['mednafen_vb'],
        'genesisplusgx': ['genesis_plus_gx'],
        'genesisplusgx-expanded': ['genesis_plus_gx_expanded'],
        'beetle-saturn': ['mednafen_saturn'],
        'pce': ['mednafen_pce'],
        'pce_fast': ['mednafen_pce_fast'],
        'mame078plus': ['mame2003_plus'],
    },
}

# Built-in features of ES (features="..." attribute) and the setting each one writes
BUILTIN_FEATURE_KEYS = {
    'ratio': 'ratio',
    'smooth': 'smooth',
    'rewind': 'rewind',
    'autosave': 'autosave',
    'shaders': 'shaderset',
    'videofilters': 'videofilters',
    'pixel_perfect': 'integerscale',
    'decoration': 'bezel',
    'latency_reduction': 'runahead',
    'colorization': 'gb_colorization',
    'fullboot': 'fullboot',
    'emulated_wiimotes': 'emulatedwiimotes',
    'screen_layout': 'layout_option',
    'internal_resolution': 'internal_resolution',
    'videomode': 'videomode',
    'autocontrollers': 'disableautocontrollers',
    'game_translation': 'ai_service_enabled',
    'cheevos': 'retroachievements',
    'netplay': 'netplay.nickname',
}
# Handled by ES itself, whatever the generator
ES_FEATURES = {'padtokeyboard', 'joystick2pad'}

# Built-in features Batocera no longer declares in its definitions, but that ES still shows only
# for the emulators that declare them: "autocontrollers" is the AUTOCONFIGURE CONTROLLERS option
# (global and per system), honoured by the RetroArch generator ("disableautocontrollers").
EXTRA_FEATURES = {
    'libretro': ['autocontrollers'],
}

yaml = ruamel.yaml.YAML(typ='safe', pure=True)


def load_yaml(path: Path) -> dict:
    with path.open(encoding='utf-8') as f:
        return yaml.load(f) or {}


# --------------------------------------------------------------------------------------------------
# Keys read by es-launcher


def launcher_keys() -> tuple[set[str], list[str]]:
    literals: set[str] = set()
    for source in LAUNCHER_SOURCES.rglob('*'):
        if source.suffix in ('.cpp', '.h'):
            literals.update(re.findall(r'"([A-Za-z0-9_.\-\[\]]+)"', source.read_text(encoding='utf-8')))

    # "retroarch." passes any key to retroarch.cfg, "deadzone_" is followed by the player number...
    prefixes = sorted(x for x in literals if len(x) > 3 and x[-1] in '._')
    return literals, prefixes


class KeyFilter:
    def __init__(self) -> None:
        self.literals, self.prefixes = launcher_keys()

    def __call__(self, key: str) -> bool:
        return key in self.literals or any(key.startswith(p) for p in self.prefixes)


# --------------------------------------------------------------------------------------------------
# Registry, as batocera_es_system.registry builds it


def new_info() -> dict:
    return {'features': None, 'shared_features': None, 'custom_features': None, 'systems': {}, 'cores': {}}


def extend_custom_features(base: dict | None, extend: dict | None) -> dict | None:
    if extend is None:
        return base
    if base is None:
        return dict(extend)

    before: dict[str, list] = defaultdict(list)
    append = []
    for key, value in extend.items():
        (before[value['before']] if 'before' in value else append).append((key, value))

    result = {}
    for key, value in base.items():
        result.update(before.get(key, []))
        result[key] = value
    result.update(append)
    return result


def extend_info(info: dict, data: dict) -> None:
    if 'features' in data:
        info['features'] = (info['features'] or []) + list(data['features'])
    if 'shared_features' in data:
        info['shared_features'] = (info['shared_features'] or []) + list(data['shared_features'])
    if 'custom_features' in data:
        info['custom_features'] = extend_custom_features(info['custom_features'], data['custom_features'])

    for system in data.get('systems', []) or []:
        name = system if isinstance(system, str) else system['name']
        if name not in info['systems']:
            info['systems'][name] = new_info()
        if not isinstance(system, str):
            extend_info(info['systems'][name], system)

    for core_name, core in (data.get('cores') or {}).items():
        if core_name not in info['cores']:
            info['cores'][core_name] = new_info()
        extend_info(info['cores'][core_name], core or {})


def file_info(path: Path) -> tuple[str, str, str, bool]:
    """kind, name, emulator, is_extension"""
    parts = path.stem.rsplit('.', 3)
    parts.reverse()
    kind, emulator, rest = parts[0], parts[1], parts[2:]
    if kind == 'emulator':
        return kind, emulator, emulator, bool(rest)
    return kind, rest[0], emulator, len(rest) > 1


def load_registry(files: list[Path]) -> dict[str, dict]:
    emulators: dict[str, dict] = {}
    cores: dict[str, dict[str, dict]] = defaultdict(dict)
    extensions: dict[str, list[Path]] = defaultdict(list)
    core_extensions: dict[tuple[str, str], list[Path]] = defaultdict(list)

    for path in files:
        kind, name, emulator, is_extension = file_info(path)
        if is_extension:
            (extensions[name] if kind == 'emulator' else core_extensions[(emulator, name)]).append(path)
            continue

        info = new_info()
        extend_info(info, load_yaml(path))
        if kind == 'emulator':
            emulators[name] = info
        else:
            cores[emulator][name] = info

    for name, info in emulators.items():
        for path in extensions.get(name, []):
            extend_info(info, load_yaml(path))

    for emulator, emulator_cores in cores.items():
        for name, core in emulator_cores.items():
            for path in core_extensions.get((emulator, name), []):
                extend_info(core, load_yaml(path))
            if emulator in emulators:
                emulators[emulator]['cores'][name] = core

    return emulators


# --------------------------------------------------------------------------------------------------
# Filtering


def filter_features(features: list[str] | None, supported) -> list[str] | None:
    if features is None:
        return None
    kept = [f for f in features if f in ES_FEATURES or (f in BUILTIN_FEATURE_KEYS and supported(BUILTIN_FEATURE_KEYS[f]))]
    return kept


def filter_info(info: dict, supported, shared: set[str]) -> dict:
    result = new_info()
    result['features'] = filter_features(info['features'], supported)
    if info['shared_features']:
        result['shared_features'] = [f for f in info['shared_features'] if f in shared]
    if info['custom_features']:
        result['custom_features'] = {k: v for k, v in info['custom_features'].items() if supported(k)}
    result['systems'] = {name: filter_info(s, supported, shared) for name, s in info['systems'].items()}
    result['cores'] = {name: filter_info(c, supported, shared) for name, c in info['cores'].items()}
    return result


def is_empty(info: dict) -> bool:
    return (
        info['features'] is None
        and not info['shared_features']
        and not info['custom_features']
        and all(is_empty(s) for s in info['systems'].values())
        and all(is_empty(c) for c in info['cores'].values())
    )


# --------------------------------------------------------------------------------------------------
# XML, as batocera_es_system.es_features writes it


def protect(value: object) -> str:
    if isinstance(value, bool):
        value = 'true' if value else 'false'
    value = str(value)
    for a, b in (('&', '&amp;'), ('<', '&lt;'), ('>', '&gt;'), ('"', '&quot;'), ('\n', '&#x0a;')):
        value = value.replace(a, b)
    return value


def attribute(name: str, value: object) -> str:
    return f' {name}="{protect(value)}"' if value else ''


def features_attribute(info: dict) -> str:
    return protect('' if info['features'] is None else ', '.join(info['features']))


def shared_to_xml(info: dict, indent: str):
    for shared in info['shared_features'] or []:
        yield f'{indent}<sharedFeature value="{protect(shared)}" />'


def custom_to_xml(features: dict | None, indent: str):
    for key, feature in (features or {}).items():
        line = f'{indent}<feature name="{protect(feature["prompt"])}"'
        line += attribute('submenu', feature.get('submenu'))
        line += attribute('group', feature.get('group'))
        line += attribute('order', feature.get('order'))
        line += attribute('requires', feature.get('requires'))
        line += f' value="{protect(key)}" description="{protect(feature.get("description", "") or "")}"'
        if 'preset' in feature:
            line += attribute('preset', feature['preset'])
            line += attribute('preset-parameters', feature.get('preset_parameters'))
        yield line + '>'
        for choice, value in (feature.get('choices') or {}).items():
            yield f'{indent}  <choice name="{protect(choice)}" value="{protect(value)}" />'
        yield f'{indent}</feature>'


def wrap(tag: str, lines: list[str], indent: str, attributes: str = '', self_closing: bool = False):
    start = f'{indent}<{tag}{" " + attributes if attributes else ""}'
    if not lines:
        if self_closing:
            yield f'{start} />'
        return
    yield f'{start}>'
    yield from lines
    yield f'{indent}</{tag}>'


def systems_to_xml(info: dict, indent: str):
    lines = []
    for name, system in info['systems'].items():
        if not system['shared_features'] and not system['custom_features']:
            continue
        lines.append(f'{indent}  <system name="{protect(name)}" features="{features_attribute(system)}" >')
        lines += shared_to_xml(system, indent + '    ')
        lines += custom_to_xml(system['custom_features'], indent + '    ')
        lines.append(f'{indent}  </system>')
    yield from wrap('systems', lines, indent)


def core_to_xml(name: str, core: dict, indent: str):
    systems = list(systems_to_xml(core, indent + '  '))
    if not core['shared_features'] and not core['custom_features'] and not systems:
        return
    lines = list(shared_to_xml(core, indent + '  '))
    lines += custom_to_xml(core['custom_features'], indent + '  ')
    lines += systems
    yield from wrap('core', lines, indent, f'name="{protect(name)}" features="{features_attribute(core)}"',
                    self_closing=core['features'] is not None)


def emulator_to_xml(tag: str, attributes: str, info: dict, indent: str):
    if is_empty(info):
        return
    cores = []
    for name, core in info['cores'].items():
        cores += core_to_xml(name, core, indent + '    ')
    lines = list(wrap('cores', cores, indent + '  '))
    lines += systems_to_xml(info, indent + '  ')
    lines += shared_to_xml(info, indent + '  ')
    lines += custom_to_xml(info['custom_features'], indent + '  ')
    yield from wrap(tag, lines, indent, attributes, self_closing=info['features'] is not None)


# --------------------------------------------------------------------------------------------------
# Translations: Batocera's batocera-es-system.po files, reduced to the strings of es_features.cfg


def po_entries(path: Path, obsolete: bool = False) -> dict[str, str]:
    """msgid -> msgstr of a .po file, untranslated ones included (plural forms: the first msgstr).
    With obsolete: the messages msgfmt would see defined twice next to ours, that is the ones
    without a msgctxt, including those commented out with #~ (msgfmt still counts them)."""
    entries: dict[str, str] = {}
    msgid = msgstr = None
    current = None
    has_context = False

    def unquote(s: str) -> str:
        return s.strip()[1:-1].encode('utf-8').decode('unicode_escape').encode('latin-1').decode('utf-8')

    def flush():
        if msgid is not None and msgstr is not None and not (obsolete and has_context):
            entries.setdefault(msgid, msgstr)

    for line in path.read_text(encoding='utf-8').splitlines():
        line = line.strip()
        if obsolete and line.startswith('#~'):
            line = line[2:].strip()
        if line.startswith('msgctxt '):
            flush()
            msgid = msgstr = current = None
            has_context = True
        elif line.startswith('msgid '):
            if msgstr is not None:
                flush()
                has_context = False
            msgid, msgstr, current = unquote(line[6:]), None, 'id'
        elif line.startswith('msgstr ') or line.startswith('msgstr[0] '):
            msgstr, current = unquote(line.split(' ', 1)[1]), 'str'
        elif line.startswith(('msgid_plural ', 'msgstr[')):
            current = None
        elif line.startswith('"') and current == 'id':
            msgid += unquote(line)
        elif line.startswith('"') and current == 'str':
            msgstr += unquote(line)
        elif not line:
            flush()
            msgid = msgstr = current = None
            has_context = False
    flush()
    return entries


def po_quote(s: str) -> str:
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n').replace('\t', '\\t') + '"'


def feature_strings(blocks: list[dict]) -> set[str]:
    strings: set[str] = set()

    def visit(info: dict):
        for feature in (info['custom_features'] or {}).values():
            for key in ('prompt', 'description', 'submenu', 'group'):
                if feature.get(key):
                    strings.add(str(feature[key]))
            strings.update(str(c) for c in (feature.get('choices') or {}))
        for sub in list(info['systems'].values()) + list(info['cores'].values()):
            visit(sub)

    for block in blocks:
        visit(block)
    return strings


def write_translations(batocera: Path, strings: set[str]) -> int:
    locales = batocera / 'package/batocera/emulationstation/batocera-es-system/locales'
    es_lang = REPO / 'locale' / 'lang'
    count = 0

    for po in sorted(locales.glob('*/batocera-es-system.po')):
        lang = po.parent.name
        es_po = es_lang / lang / 'LC_MESSAGES' / 'emulationstation2.po'
        if not es_po.exists():
            continue

        # msgfmt refuses a msgid defined twice, even an obsolete one: leave out the strings ES
        # already has
        known = set(po_entries(es_po, obsolete=True)) | {''}
        entries = {k: v for k, v in sorted(po_entries(po).items()) if k in strings and k not in known and v}
        target = PO_OUTPUT / f'{lang}.po'
        if not entries:
            target.unlink(missing_ok=True)
            continue

        PO_OUTPUT.mkdir(parents=True, exist_ok=True)
        lines = [
            '# Translations of the options of es_features.cfg, from Batocera\'s batocera-es-system.po.',
            '# Generated by features/build_es_features.py: do not edit.',
            '# Compiled together with lang/<lang>/LC_MESSAGES/emulationstation2.po, whose header (UTF-8) applies:',
            '# this file has none, msgfmt refuses a header defined twice.',
            '',
        ]
        for msgid, msgstr in entries.items():
            lines += [f'msgid {po_quote(msgid)}', f'msgstr {po_quote(msgstr)}', '']
        target.write_text('\n'.join(lines), encoding='utf-8')
        count += 1

    return count


# --------------------------------------------------------------------------------------------------


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('batocera', type=Path, help='batocera.linux checkout')
    args = parser.parse_args()

    package = args.batocera / 'package' / 'batocera'
    if not package.is_dir():
        sys.exit(f'{args.batocera} is not a batocera.linux checkout')

    files = sorted(p for p in package.rglob('*.yml') if p.name.endswith(('.emulator.yml', '.core.yml')))
    files += sorted(HERE.glob('*.emulator.yml'))
    registry = load_registry(files)

    supported = KeyFilter()
    shared_defs = registry['_shared']
    shared_keys = {k for k in (shared_defs['custom_features'] or {}) if supported(k)}

    blocks: list[tuple[str, str, dict]] = []
    for emulator, names in EMULATORS.items():
        if emulator not in registry:
            print(f'warning: no definition for {emulator}', file=sys.stderr)
            continue
        info = filter_info(registry[emulator], supported, shared_keys)
        cores = {}
        for name, core in info['cores'].items():
            cores[name] = core
            for alias in CORE_ALIASES.get(emulator, {}).get(name, []):
                cores.setdefault(alias, core)
        info['cores'] = cores
        extra = filter_features(EXTRA_FEATURES.get(emulator), supported) or []
        if extra:
            info['features'] = (info['features'] or []) + [f for f in extra if f not in (info['features'] or [])]
        blocks.append(('emulator', f'name="{protect(names)}" features="{{features}}"', info))

    global_defs = filter_info(registry['_global'], supported, shared_keys)

    # only the shared definitions that something uses
    used: set[str] = set()

    def collect(info: dict):
        used.update(info['shared_features'] or [])
        for sub in list(info['systems'].values()) + list(info['cores'].values()):
            collect(sub)

    for _, _, info in blocks:
        collect(info)
    collect(global_defs)
    shared = new_info()
    shared['custom_features'] = {k: v for k, v in (shared_defs['custom_features'] or {}).items() if k in used}

    lines = [
        '<?xml version="1.0" encoding="UTF-8" ?>',
        '<!-- Options of the emulators of es-launcher, from Batocera\'s es_features (LGPL-3.0-or-later,',
        '     Copyright (c) 2026 The Batocera Team). Generated by features/build_es_features.py: do not edit. -->',
        '<features>',
    ]
    lines += emulator_to_xml('sharedFeatures', '', shared, '  ')
    lines += emulator_to_xml('globalFeatures', '', global_defs, '  ')
    for tag, attributes, info in blocks:
        lines += emulator_to_xml(tag, attributes.replace('{features}', features_attribute(info)), info, '  ')
    lines.append('</features>')
    OUTPUT.write_text('\n'.join(lines) + '\n', encoding='utf-8')

    strings = feature_strings([shared, global_defs] + [info for _, _, info in blocks])
    languages = write_translations(args.batocera, strings)

    print(f'{OUTPUT.relative_to(REPO)}: {len(blocks)} emulators, {len(shared["custom_features"])} shared options; '
          f'{languages} translations in {PO_OUTPUT.relative_to(REPO)}')


if __name__ == '__main__':
    main()
