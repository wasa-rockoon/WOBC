import ast
import sys
from collections import defaultdict
from pathlib import Path
import shutil

from rich.panel import Panel
from rich.text import Text
from wcpp import util

source = Path(r'C:\WOBC\src\library\wcpp\python\util.py')
tree = ast.parse(source.read_text(encoding='utf-8'))
node = next(n for n in tree.body if isinstance(n, ast.FunctionDef) and n.name == 'packet_view')
namespace = {'Panel': Panel, 'Text': Text}
exec(compile(ast.Module(body=[node], type_ignores=[]), str(source), 'exec'), namespace)
view = namespace['packet_view']
data = Path(r'F:\log_ 214.bin').read_bytes()
_, packets = util.parse_packet(data)
all_packets = defaultdict(lambda: [defaultdict(lambda: [defaultdict(lambda: [[], None, -1]), None]), None])
hidden = 0
for packet in packets:
    util.add_packet(all_packets, packet)
    selection = [packet.origin_unit_id, packet.component_id, packet.packet_id]
    panel = view(all_packets, selection)
    assert isinstance(panel, Panel)
    hidden += 'Payload hidden:' in panel.renderable.plain
assert hidden == 1006, hidden
assert len(packets) == 24421, len(packets)
for unit_id, (units, _) in all_packets.items():
    for component_id, (components, _) in units.items():
        for packet_id in components:
            selection = [unit_id, component_id, packet_id]
            for key in ('K', 'j', 'k', 'J', 'h', 'l'):
                util.on_input(key, all_packets, selection, None)
                assert isinstance(view(all_packets, selection), Panel)
print(f'PASS: {len(packets)} packets rendered; {hidden} invalid payloads hidden; navigation verified.')

if '--install' in sys.argv:
    target = Path(util.__file__)
    original = target.read_bytes()
    newline = '\r\n' if b'\r\n' in original else '\n'
    text = original.decode('utf-8')
    old = "            for entry in packet.entries:\n                txt += entry.__str__('  ')"
    new = """            try:
                entry_text = ''.join(entry.__str__('  ') for entry in packet.entries)
            except Exception as exc:
                # Keep navigation available even when one packet cannot be rendered.
                txt += f'Payload hidden: {type(exc).__name__}: {exc}\\n'
                txt += 'Use j/k for next/previous packet, h/l for another packet ID.\\n'
            else:
                txt += entry_text"""
    old = old.replace('\n', newline)
    new = new.replace('\n', newline)
    assert text.count(old) == 1, 'Expected exactly one display block; no changes made.'
    updated = text.replace(old, new)
    compile(updated, str(target), 'exec')
    backup = target.with_name('util.py.before-packet-display-fix.bak')
    assert not backup.exists(), f'Backup already exists: {backup}'
    shutil.copy2(target, backup)
    target.write_bytes(updated.encode('utf-8'))
    print(f'Installed: {target}\nBackup: {backup}')
