#!/usr/bin/env python3
# Trägt die englischen Übersetzungen aus tools/translations_en.py in translations/vitabreathe_en.ts ein.
# Ablauf nach Textänderungen:  lupdate VitaBreathe.pro  →  python3 tools/fill_ts.py  →  bauen
# Fehlende Übersetzungen werden gemeldet und bleiben „unfinished“ (dann zeigt das Programm den deutschen Text).
import os
import sys
import xml.etree.ElementTree as ET

here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, here)
from translations_en import EN, EN_CONTEXT  # noqa: E402

ts = os.path.join(here, '..', 'translations', 'vitabreathe_en.ts')
tree = ET.parse(ts)
missing = []
done = 0
for ctx in tree.getroot().findall('context'):
    for msg in ctx.findall('message'):
        src = msg.find('source').text or ''
        tr = msg.find('translation')
        if tr is None:
            tr = ET.SubElement(msg, 'translation')
        en = EN_CONTEXT.get((ctx.find('name').text, src), EN.get(src))
        if en is None:
            missing.append((ctx.find('name').text, src))
            continue
        for child in list(tr):
            tr.remove(child)
        tr.text = None
        if msg.get('numerus') == 'yes':
            forms = en if isinstance(en, list) else [en, en]
            for f in forms:
                nf = ET.SubElement(tr, 'numerusform')
                nf.text = f
        else:
            tr.text = en[0] if isinstance(en, list) else en
        tr.attrib.pop('type', None)
        done += 1

tree.write(ts, encoding='utf-8', xml_declaration=True)
# lupdate-kompatibler Kopf
with open(ts, encoding='utf-8') as f:
    data = f.read()
data = data.replace("<?xml version='1.0' encoding='utf-8'?>", '<?xml version="1.0" encoding="utf-8"?>\n<!DOCTYPE TS>', 1)
with open(ts, 'w', encoding='utf-8') as f:
    f.write(data)
print(f'{done} übersetzt, {len(missing)} fehlen')
for c, s in missing:
    print(f'  FEHLT [{c}]: {s!r}')
sys.exit(1 if missing else 0)
