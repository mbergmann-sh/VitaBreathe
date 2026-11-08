# Erzeugt die Signaltöne von VitaBreathe (16 Bit, 44,1 kHz, mono) – eigene Klänge, frei verwendbar
import math, wave, struct, sys, os
out = sys.argv[1]; R = 44100
def write(name, samples):
    with wave.open(os.path.join(out, name), "w") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(R)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1, min(1, s)) * 32000)) for s in samples))
def bell(freq, dur, amp=0.5, partials=((1,1),(2.0,0.35),(3.0,0.12)), decay=4.0, attack=0.006):
    n = int(R*dur); res = []
    for i in range(n):
        t = i/R; env = min(1, t/attack) * math.exp(-decay*t)
        res.append(amp*env*sum(a*math.sin(2*math.pi*freq*m*t) for m,a in partials))
    return res
def glide(f0, f1, dur, amp=0.4):
    n = int(R*dur); res = []; ph = 0
    for i in range(n):
        t = i/R; f = f0 + (f1-f0)*t/dur; ph += 2*math.pi*f/R
        env = math.sin(math.pi*t/dur)**1.5
        res.append(amp*env*(math.sin(ph)+0.2*math.sin(2*ph)))
    return res
def mix(*parts):
    n = max(len(p) for _,p in parts); res = [0.0]*n
    for off,p in parts:
        o = int(off*R)
        for i,s in enumerate(p):
            if o+i < n + 0: 
                if o+i >= len(res): res.extend([0.0]*(o+i-len(res)+1))
                res[o+i] += s
    return res
write("phase.wav",   bell(880, 0.45, 0.45, decay=7))            # neutraler Phasenwechsel
write("inhale.wav",  glide(520, 780, 0.35))                      # Einatmen: steigend
write("hold.wav",    bell(660, 0.4, 0.4, decay=8))               # Halten
write("exhale.wav",  glide(700, 440, 0.4))                       # Ausatmen: fallend
write("done.wav",    mix((0, bell(523.25, 2.2, 0.35, decay=1.6)), (0.25, bell(659.25, 2.0, 0.3, decay=1.6)),
                         (0.5, bell(783.99, 2.2, 0.3, decay=1.4))))  # Ende der Übung (Dreiklang)
write("reminder.wav", mix((0, bell(784, 1.0, 0.4, decay=3)), (0.35, bell(988, 1.2, 0.4, decay=3))))
alarm = []
for k in range(3):
    alarm += bell(1046.5, 0.18, 0.45, decay=10) + bell(1318.5, 0.25, 0.45, decay=10) + [0.0]*int(R*0.25)
write("alarm.wav", alarm)                                         # Wecker ohne Musik (in Schleife)
print("ok")
