# SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
# Erzeugt die Illustrationen von ViviaBreathe (Haltungen, TriBall-Anleitung) als SVG-Strichzeichnungen.
# Platzhalter: #COLOR (Strich, an das Theme angepasst), #ACCENT (Orange), #BLUE (Blau), #GREEN (Grün).
# Aufruf: python3 tools/make_illustrations.py .
import os
import sys

root = sys.argv[1] if len(sys.argv) > 1 else '.'
out = os.path.join(root, 'resources', 'illustrations')
os.makedirs(out, exist_ok=True)

W, H = 240, 180
L = 'fill="none" stroke="#COLOR" stroke-width="5" stroke-linecap="round" stroke-linejoin="round"'
T = 'fill="none" stroke="#COLOR" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"'
A = 'fill="none" stroke="#ACCENT" stroke-width="4" stroke-linecap="round" stroke-linejoin="round"'
AD = 'fill="none" stroke="#ACCENT" stroke-width="3" stroke-linecap="round" stroke-dasharray="2 7"'
B = 'fill="none" stroke="#BLUE" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"'


def svg(name, body):
    with open(os.path.join(out, name + '.svg'), 'w') as f:
        f.write(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}">\n{body}\n</svg>\n')


def line(*pts, style=L):
    d = 'M' + ' L'.join(f'{x},{y}' for x, y in pts)
    return f'<path d="{d}" {style}/>'


def head(cx, cy, r=13):
    return f'<circle cx="{cx}" cy="{cy}" r="{r}" {L}/>'


def floor(y=164):
    return line((20, y), (220, y), style=T)


def chair(x0=56, x1=112, seat=122, back=True):
    s = line((x0, seat), (x1, seat), style=T) + line((x0 + 2, seat), (x0 + 2, 162), style=T) + line((x1 - 2, seat), (x1 - 2, 162), style=T)
    if back:
        s += line((x0, seat), (x0, 72), style=T)
    return s


def triball(x, y, s=1.0, up=(0, 0, 0), hose=True):
    """Atemtrainer: Sockel, drei Kammern mit Bällen; up[i] = 0 (unten) … 1 (oben)."""
    cw, ch, gap = 14 * s, 44 * s, 4 * s
    parts = [f'<rect x="{x - 3*s}" y="{y + ch}" width="{3*cw + 2*gap + 6*s}" height="{8*s}" rx="{3*s}" {B}/>']
    colors = ['#BLUE', '#GREEN', '#ACCENT']
    for i in range(3):
        cx = x + i * (cw + gap)
        parts.append(f'<rect x="{cx}" y="{y}" width="{cw}" height="{ch}" rx="{cw/2}" {B}/>')
        r = cw / 2 - 2.5 * s
        by = y + ch - r - 2 * s - up[i] * (ch - 2 * r - 4 * s)
        parts.append(f'<circle cx="{cx + cw/2}" cy="{by}" r="{r}" fill="{colors[i]}"/>')
    if hose:
        hx = x + 3 * cw + 2 * gap + 3 * s
        parts.append(f'<path d="M{hx},{y + ch + 2*s} C{hx + 14*s},{y + ch} {hx + 14*s},{y + 10*s} {hx + 22*s},{y + 6*s}" '
                     f'fill="none" stroke="#BLUE" stroke-width="{4*s}" stroke-linecap="round"/>')
        parts.append(f'<rect x="{hx + 20*s}" y="{y + 1*s}" width="{12*s}" height="{8*s}" rx="{2*s}" {B}/>')
    return '\n'.join(parts)


def profile(dx=0, dy=0, mouth='closed'):
    """Kopf im Profil (Blick nach rechts); Mund bei etwa (142, 82)."""
    p = (f'<path transform="translate({dx},{dy})" d="M72,44 C70,24 90,14 110,16 C130,18 142,32 144,50 L154,66 '
         f'L144,71 C145,74 145,77 143,79" {L}/>')
    p += f'<path transform="translate({dx},{dy})" d="M143,87 C144,96 136,106 122,106 L120,128 M72,44 C68,66 76,88 94,98 L96,128" {L}/>'
    if mouth == 'closed':
        p += f'<path transform="translate({dx},{dy})" d="M143,80 L148,82 L143,86" {L}/>'
    elif mouth == 'pursed':
        p += f'<ellipse transform="translate({dx},{dy})" cx="148" cy="83" rx="5" ry="4" fill="none" stroke="#ACCENT" stroke-width="4"/>'
    elif mouth == 'open':
        p += f'<ellipse transform="translate({dx},{dy})" cx="146" cy="84" rx="5" ry="8" fill="none" stroke="#ACCENT" stroke-width="4"/>'
    return p


def arrow(x0, y0, x1, y1, style=A):
    import math
    a = math.atan2(y1 - y0, x1 - x0)
    h1 = (x1 - 9 * math.cos(a - 0.5), y1 - 9 * math.sin(a - 0.5))
    h2 = (x1 - 9 * math.cos(a + 0.5), y1 - 9 * math.sin(a + 0.5))
    return line((x0, y0), (x1, y1), style=style) + line(h1, (x1, y1), h2, style=style)


def sitting(lean=0.0, arms='lap'):
    """Sitzende Figur (Blick nach rechts). lean 0 = aufrecht, 1 = weit vorgebeugt."""
    hip = (84, 120)
    sh = (90 + 32 * lean, 66 + 6 * lean)
    hd = (sh[0] + 4 + 12 * lean, sh[1] - 20 + 4 * lean)
    s = head(*hd) + line(hip, sh)
    s += line(hip, (134, 120), (134, 160), (148, 160))
    if arms == 'lap':
        s += line((sh[0] + 2, sh[1] + 6), (sh[0] + 14 - 4 * lean, sh[1] + 30 + 14 * lean), (130 + 8 * lean, 116))
    elif arms == 'forearms':
        s += line((sh[0] + 2, sh[1] + 6), (sh[0] + 6, 112), (146, 112))
    return s, hd


# ---------------------------------------------------------------- Haltungen
fig, hd = sitting(0.0)
svg('posture_upright', chair() + floor() + fig +
    arrow(170, 140, 170, 40, style=B))

fig, hd = sitting(1.0, 'forearms')
svg('posture_coachman', chair(back=False) + floor() + fig +
    line((hd[0] + 16, hd[1] + 6), (hd[0] + 40, hd[1] + 14), style=AD) +
    line((hd[0] + 16, hd[1] + 12), (hd[0] + 36, hd[1] + 24), style=AD))

# Torwarthaltung: stehend vorgebeugt, Hände auf dem Tisch
gk = (head(136, 54) + line((84, 106), (124, 70)) + line((84, 106), (74, 162), (62, 162)) +
      line((84, 106), (96, 162), (108, 162)) + line((122, 74), (160, 98)) +
      line((150, 100), (216, 100), style=T) + line((156, 100), (156, 162), style=T) + line((210, 100), (210, 162), style=T) +
      line((150, 56), (174, 62), style=AD) + line((150, 62), (170, 74), style=AD))
svg('posture_goalkeeper', floor() + gk)

# Bauchatmung im Liegen, Beine angewinkelt, Hand auf dem Bauch
belly = (line((16, 152), (224, 152), style=T) + head(36, 134) + line((52, 138), (128, 138)) +
         line((128, 138), (160, 104), (190, 150)) + line((62, 136), (84, 120), (102, 124)) +
         f'<ellipse cx="108" cy="126" rx="8" ry="5" fill="#ACCENT"/>' + arrow(108, 112, 108, 88))
svg('posture_belly', belly)

# Lippenbremse: Profil mit gespitzten Lippen, lange Ausatemlinien
svg('pursed_lips', profile(0, 10, 'pursed') +
    line((162, 92), (222, 92), style=AD) + line((162, 84), (214, 76), style=AD) + line((162, 100), (214, 108), style=AD))

# Huffen: offener Mund, Brille anhauchen
huff = (profile(-10, 10, 'open') +
        '<circle cx="186" cy="70" r="14" fill="none" stroke="#BLUE" stroke-width="3"/>'
        '<circle cx="186" cy="110" r="14" fill="none" stroke="#BLUE" stroke-width="3"/>'
        + line((186, 84), (186, 96), style=B) +
        ''.join(f'<circle cx="{x}" cy="{y}" r="2.5" fill="#ACCENT"/>' for x, y in
                [(160, 88), (168, 82), (170, 96), (176, 90), (164, 100), (178, 74), (180, 106)]))
svg('huff', huff)

# ---------------------------------------------------------------- TriBall-Anleitung
# 1. Aufrecht sitzen, Gerät senkrecht halten
fig, hd = sitting(0.1, arms='none')
svg('triball_sit', chair() + floor() + fig +
    line((98, 74), (118, 96), (150, 90)) + triball(150, 50, 0.85, hose=False) +
    line((150, 70), (150, 100), style=T))

# 2. Normal ausatmen – nicht ins Gerät
fig, hd = sitting(0.0)
svg('triball_exhale', chair() + floor() + fig + triball(170, 100, 0.75, hose=False) +
    line((hd[0] + 16, hd[1] + 4), (hd[0] + 50, hd[1] - 2), style=AD) + line((hd[0] + 16, hd[1] + 10), (hd[0] + 46, hd[1] + 18), style=AD))

# 3. Mundstück mit den Lippen fest umschließen
svg('triball_seal', profile(-20, 10) +
    '<rect x="128" y="86" width="22" height="12" rx="3" fill="none" stroke="#BLUE" stroke-width="4"/>' +
    '<path d="M150,92 C176,92 180,120 186,140" fill="none" stroke="#BLUE" stroke-width="5" stroke-linecap="round"/>' +
    triball(178, 116, 0.6, hose=False) +
    f'<circle cx="126" cy="92" r="11" fill="none" stroke="#ACCENT" stroke-width="3"/>')

# 4. Langsam einatmen: erster Ball steigt
svg('triball_inhale', triball(40, 40, 2.0, up=(1, 0, 0)) + arrow(222, 40, 196, 44) +
    f'<text x="112" y="172" font-family="sans-serif" font-size="13" text-anchor="middle" fill="#ACCENT">600 ml/s</text>')

# 5. Ball oben halten (Uhr, 3 s)
svg('triball_hold', triball(20, 40, 2.0, up=(1, 0, 0), hose=False) +
    '<circle cx="186" cy="80" r="34" fill="none" stroke="#COLOR" stroke-width="5"/>' +
    line((186, 80), (186, 58), style=A) + line((186, 80), (200, 92), style=A) +
    '<text x="186" y="140" font-family="sans-serif" font-size="18" font-weight="bold" text-anchor="middle" fill="#ACCENT">≥ 3 s</text>')

# 6. Absetzen, ausruhen – später steigern: 1, 2, 3 Bälle
rest = (triball(14, 50, 0.95, up=(1, 0, 0), hose=False) + triball(90, 50, 0.95, up=(1, 1, 0), hose=False) +
        triball(166, 50, 0.95, up=(1, 1, 1), hose=False) +
        arrow(70, 76, 86, 76, style=T) + arrow(146, 76, 162, 76, style=T) +
        ''.join(f'<text x="{x}" y="150" font-family="sans-serif" font-size="15" font-weight="bold" text-anchor="middle" '
                f'fill="#COLOR">{n}</text>' for x, n in [(36, '1'), (112, '2'), (188, '3')]))
svg('triball_rest', rest)

print(len(os.listdir(out)), 'Illustrationen')
