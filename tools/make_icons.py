# Erzeugt die SVG-Symbole von VitaBreathe (App-Symbol + Werkzeugleiste)
import os, sys
root = sys.argv[1]
ui = os.path.join(root, "resources/icons/ui"); app = os.path.join(root, "resources/icons/app")
BLUE, ORANGE = "#1474BC", "#E0620D"

APP = f'''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256" width="256" height="256">
  <defs>
    <linearGradient id="bg" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#1A82CC"/><stop offset="1" stop-color="#0F67AB"/>
    </linearGradient>
  </defs>
  <rect x="8" y="8" width="240" height="240" rx="48" fill="url(#bg)"/>
  <!-- Luftröhre und Bronchien -->
  <path d="M128 34 V100 M128 100 C128 112 118 118 106 124 M128 100 C128 112 138 118 150 124"
        fill="none" stroke="#fff" stroke-width="13" stroke-linecap="round"/>
  <!-- linker Lungenflügel -->
  <path d="M110 84 C92 64 62 76 50 110 C38 146 40 192 58 212 C72 226 100 222 108 204 C114 190 114 160 114 132 C114 112 114 96 110 84 Z" fill="#fff"/>
  <!-- rechter Lungenflügel -->
  <path d="M146 84 C164 64 194 76 206 110 C218 146 216 192 198 212 C184 226 156 222 148 204 C142 190 142 160 142 132 C142 112 142 96 146 84 Z" fill="#fff"/>
  <!-- Atemwelle -->
  <path d="M24 162 C44 162 52 134 74 134 S 104 190 128 190 S 160 134 182 134 S 212 162 232 162"
        fill="none" stroke="#0F67AB" stroke-width="24" stroke-linecap="round"/>
  <path d="M24 162 C44 162 52 134 74 134 S 104 190 128 190 S 160 134 182 134 S 212 162 232 162"
        fill="none" stroke="{ORANGE}" stroke-width="13" stroke-linecap="round"/>
</svg>
'''
os.makedirs(app, exist_ok=True); os.makedirs(ui, exist_ok=True)
open(os.path.join(app, "vitabreathe.svg"), "w").write(APP)

S = 'fill="none" stroke="#COLOR" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"'
A = 'fill="none" stroke="#ACCENT" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"'
icons = {
 "user": f'<circle cx="12" cy="8" r="4" {S}/><path d="M4 21c0-4.4 3.6-7 8-7s8 2.6 8 7" {S}/>',
 "user-add": f'<circle cx="10" cy="8" r="4" {S}/><path d="M2 21c0-4.4 3.6-7 8-7 1.6 0 3 .3 4.2.9" {S}/><path d="M19 14v7M15.5 17.5h7" {A}/>',
 "users": f'<circle cx="9" cy="8" r="3.5" {S}/><path d="M2 20c0-3.9 3.1-6 7-6s7 2.1 7 6" {S}/><circle cx="17" cy="7" r="2.8" {S}/><path d="M17 12.5c2.9 0 5 1.8 5 5" {S}/>',
 "play": f'<circle cx="12" cy="12" r="10" {S}/><path d="M10 8l6 4-6 4z" fill="#ACCENT" stroke="#ACCENT" stroke-width="1.5" stroke-linejoin="round"/>',
 "pause": '<path d="M8 5v14M16 5v14" fill="none" stroke="#COLOR" stroke-width="3" stroke-linecap="round"/>',
 "stop": f'<rect x="6" y="6" width="12" height="12" rx="1.5" {S}/>',
 "box": f'<rect x="4" y="4" width="16" height="16" rx="1" {S}/><circle cx="4" cy="4" r="2.6" fill="#ACCENT"/>',
 "circle": f'<circle cx="12" cy="12" r="9.5" {S}/><circle cx="12" cy="12" r="5" fill="#ACCENT" stroke="none"/>',
 "wave": f'<path d="M2 12c2.5 0 3-6 6-6s3.5 12 7 12 3.5-6 7-6" {A}/><path d="M2 21h20" {S}/>',
 "history": f'<path d="M3 12a9 9 0 1 0 3-6.7" {S}/><path d="M3 4v5h5" {S}/><path d="M12 7v5l3 2" {S}/>',
 "chart": f'<path d="M3 3v18h18" {S}/><path d="M6 16l4-5 3 3 6-7" {A}/>',
 "bell": f'<path d="M6 16V11a6 6 0 0 1 12 0v5l2 2H4z" {S}/><path d="M10 20a2 2 0 0 0 4 0" {S}/>',
 "alarm": f'<circle cx="12" cy="13" r="7.5" {S}/><path d="M12 9v4l2.5 2" {A}/><path d="M3 6l3-3M21 6l-3-3M7 20l-1.5 2M17 20l1.5 2" {S}/>',
 "moon": f'<path d="M20 14.5A8.5 8.5 0 0 1 9.5 4a8.5 8.5 0 1 0 10.5 10.5z" {S}/><path d="M17 3v3M15.5 4.5h3" {A}/>',
 "music": f'<path d="M9 18V5l11-2v13" {S}/><circle cx="6.5" cy="18" r="2.5" {S}/><circle cx="17.5" cy="16" r="2.5" {S}/>',
 "print": f'<path d="M6 9V3h12v6" {S}/><rect x="3" y="9" width="18" height="8" rx="1.5" {S}/><path d="M6 14h12v7H6z" {S}/>',
 "pdf": f'<path d="M14 3H6v18h12V7z" {S}/><path d="M14 3v4h4" {S}/><path d="M9 13h6M9 17h4" {A}/>',
 "settings": f'<path d="M4 6h10M18 6h2M4 12h4M12 12h8M4 18h12M20 18h0" {S}/><circle cx="16" cy="6" r="2" {S}/><circle cx="10" cy="12" r="2" {S}/><circle cx="18" cy="18" r="2" {S}/>',
 "sync": f'<path d="M20 11a8 8 0 0 0-14.3-4.9L4 8" {S}/><path d="M4 3v5h5" {S}/><path d="M4 13a8 8 0 0 0 14.3 4.9L20 16" {S}/><path d="M20 21v-5h-5" {S}/>',
 "lock": f'<rect x="5" y="11" width="14" height="10" rx="1.5" {S}/><path d="M8 11V7a4 4 0 0 1 8 0v4" {S}/>',
 "help": f'<circle cx="12" cy="12" r="10" {S}/><path d="M9.5 9a2.5 2.5 0 1 1 3.5 2.3c-.7.3-1 .9-1 1.7v.5" {S}/><circle cx="12" cy="17" r="1.1" fill="#COLOR"/>',
 "reset": f'<path d="M4 12a8 8 0 1 0 2.3-5.7L4 8.5" {S}/><path d="M4 3.5v5h5" {A}/>',
 "add": f'<path d="M12 5v14M5 12h14" {S}/>',
 "edit": f'<path d="M4 20h4L19 9l-4-4L4 16z" {S}/><path d="M13.5 6.5l4 4" {S}/>',
 "delete": f'<path d="M4 7h16M10 11v6M14 11v6M6 7l1 14h10l1-14M9 7V4h6v3" {S}/>',
 "volume": f'<path d="M4 9h4l5-4v14l-5-4H4z" {S}/><path d="M16.5 8.5a5 5 0 0 1 0 7M19 6a8.5 8.5 0 0 1 0 12" {A}/>',
 "event": f'<rect x="3" y="5" width="18" height="16" rx="1.5" {S}/><path d="M3 10h18M8 3v4M16 3v4" {S}/><path d="M8 15l2.5 2.5L16 13" {A}/>',
 "info": f'<circle cx="12" cy="12" r="10" {S}/><path d="M12 11v6" {S}/><circle cx="12" cy="7.5" r="1.1" fill="#COLOR"/>',
 "measure": f'<path d="M12 21s-7-4.4-7-10a4 4 0 0 1 7-2.6A4 4 0 0 1 19 11c0 5.6-7 10-7 10z" {S}/><path d="M7 12h3l1.5-3 2 5 1.5-2H17" {A}/>',
 "fullscreen": f'<path d="M4 9V4h5M20 9V4h-5M4 15v5h5M20 15v5h-5" {S}/>',
 "quit": f'<path d="M15 4h4v16h-4" {S}/><path d="M10 8l-4 4 4 4M6 12h10" {S}/>',
 "database": f'<ellipse cx="12" cy="5.5" rx="8" ry="3" {S}/><path d="M4 5.5v13c0 1.7 3.6 3 8 3s8-1.3 8-3v-13M4 12c0 1.7 3.6 3 8 3s8-1.3 8-3" {S}/>',
 "theme": f'<circle cx="12" cy="12" r="9" {S}/><path d="M12 3a9 9 0 0 0 0 18z" fill="#COLOR"/>',
 "lungs": f'<path d="M12 3v7M12 10c-1 2-3 2.5-4 3M12 10c1 2 3 2.5 4 3" {A}/><path d="M9 7.5C6 6 3.5 9 3 13c-.5 4 .5 7 3 7.5 2.5.5 3.5-1 3.5-3.5V9.5c0-1-.2-1.6-.5-2zM15 7.5c3-1.5 5.5 1.5 6 5.5.5 4-.5 7-3 7.5-2.5.5-3.5-1-3.5-3.5V9.5c0-1 .2-1.6.5-2z" {S}/>',
 "lips": f'<path d="M3 12c2-3 4-4 6-3 1 .5 1.5.5 2 0 2-1 4 0 6 3-2 3-4 4-6 4s-4-1-8-4z" {S}/><path d="M3 12h14" {S}/><path d="M19 9.5h2.5M19.5 12h3M19 14.5h2.5" {A}/>',
 "belly": f'<path d="M7 3v4c-2 2-3 4.5-3 7.5C4 19 7 21 12 21s8-2 8-6.5c0-3-1-5.5-3-7.5V3" {S}/><circle cx="12" cy="14" r="3" {A}/><path d="M12 9.5V8M12 20v-1.5" {A}/>',
 "nose": f'<path d="M10 3c0 4-3 8-4 11 0 2 1.5 3 3.5 3 1 0 1.5.5 2 1" {S}/><path d="M15 9h6M16 12.5h6M15 16h6" {A}/>',
 "acbt": f'<path d="M20 12a8 8 0 1 1-2.3-5.7" {S}/><path d="M18 2.5v4h-4" {S}/><circle cx="12" cy="12" r="1.3" fill="#ACCENT"/><circle cx="8" cy="12" r="1.3" fill="#ACCENT"/><circle cx="16" cy="12" r="1.3" fill="#ACCENT"/>',
}
for name, body in icons.items():
    open(os.path.join(ui, name + ".svg"), "w").write(
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24">{body}</svg>\n')
print(len(icons), "icons")
