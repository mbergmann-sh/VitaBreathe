# ViviaBreathe – Abgleich-Server

Mit diesem Ordner gleicht ViviaBreathe seine Datenbank zwischen mehreren Geräten ab (z. B. Desktop
im Büro und Laptop zu Hause, später Smartphone/Tablet). Aufbau und Sicherheitskonzept stammen
aus VitaParms (`api/sync.php`, `api/lib.php`).

## So sind die Daten geschützt

- Auf dem Server liegt **nur die verschlüsselte Datenbank** (`sync/db.vbdb`,
  SQLite3 Multiple Ciphers, ChaCha20-Poly1305). Verschlüsselt wird auf dem Gerät mit dem
  **Master-Passwort** – der Server kennt es nicht und kann nichts entschlüsseln.
- Unverschlüsselte Datenbanken lehnt der Server ab.
- Zugreifen darf nur, wer den **Zugangsschlüssel** kennt (getrennt vom Master-Passwort). Der
  erste Upload legt ihn fest; gespeichert wird davon nur ein Hash (`data/.uploadkey.php`).
- `sync/` und `data/` sind per `.htaccess` gesperrt; abrufbar ist nur `api/sync.php`.
- Zusammengeführt wird **auf den Geräten** (zeilenweise, die jüngere Änderung gewinnt,
  Löschungen werden übertragen). Der Server zählt nur eine Revision hoch und weist veraltete
  Uploads ab (HTTP 409 → das Gerät arbeitet den neuen Stand ein und versucht es erneut).

## Voraussetzungen

- Webspace mit **Apache** (`.htaccess` wird ausgewertet) und **PHP ≥ 7.4**
- **HTTPS** (Let's Encrypt genügt) – ViviaBreathe verbindet sich außer mit `localhost` nur über https
- keine Datenbank, keine weiteren PHP-Module

## Installation

1. Subdomain anlegen, z. B. `atem.example.de`, auf einen leeren Ordner zeigen lassen, HTTPS aktivieren.
2. Den **Inhalt** dieses Ordners hochladen (versteckte `.htaccess`-Dateien mitnehmen):
   ```
   .htaccess  index.html  robots.txt
   api/.htaccess  api/lib.php  api/sync.php
   data/.htaccess  data/index.html
   sync/.htaccess  sync/index.html
   ```
3. `data/` und `sync/` beschreibbar machen (Rechte `755`, bei manchen Anbietern `775`).
4. **Erstes Gerät:** ViviaBreathe → *Extras → Einstellungen → Abgleich*: *Abgleich einschalten*,
   Adresse eintragen, **Erzeugen** (Zugangsschlüssel), **Verbindung testen**, **Jetzt abgleichen**.
   Den Zugangsschlüssel sicher notieren.
5. **Weitere Geräte:** Beim ersten Start im Anmeldefenster **Vom Server laden …** wählen,
   Adresse und Zugangsschlüssel eingeben, danach mit dem **gemeinsamen Master-Passwort** anmelden.

Danach gleicht ViviaBreathe automatisch ab: beim Start, etwa eine Minute nach Änderungen und beim
Beenden – von Hand über *Datei → Datenbank abgleichen* (`Strg+Umschalt+A`).

## Wichtig

- Alle Geräte verwenden **dasselbe Master-Passwort**. Wird es auf einem Gerät geändert, muss es
  auf allen anderen ebenfalls geändert werden, sonst melden diese beim Abgleich einen Fehler.
- Gerätebezogen bleiben die Abgleich-Einstellungen selbst und der zuletzt gewählte Benutzer.
- Wiedergabelisten (MP3-Pfade) werden mit abgeglichen; Dateien, die es auf einem Gerät nicht
  gibt, werden dort rot markiert.

## Schnittstelle

| Aufruf | Ergebnis |
|---|---|
| `GET api/sync.php?state=1` | `{ok, claimed, rev, size, updated, schema}` |
| `GET api/sync.php?get=db` | verschlüsselte Datenbank |
| `POST api/sync.php` (`base`, `schema`, Datei `db`) | `200 {rev}`, `409` veraltet, `426` ältere Programmversion |

Alle Aufrufe brauchen den Kopf `X-ViviaBreathe-Key` (der Kopf `X-VitaBreathe-Key` der Versionen
bis 0.4.0 wird weiterhin angenommen). Lokal testen:
`php -S 127.0.0.1:8099 -t website` und in ViviaBreathe `http://127.0.0.1:8099/` eintragen
(siehe auch `tests/synctest`).
