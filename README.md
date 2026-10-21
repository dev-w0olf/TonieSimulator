# Tonie-Simulator by W0olf (Flipper Zero App)

Ein nativer **Toniebox NFC-Simulator** (`.fap` – Flipper Application Package) für den **Flipper Zero**.  
Die App liest `.nfc`-Dumps (ISO/IEC 15693 / NXP ICODE SLIX & SLIX2) von der SD-Karte ein, organisiert sie übersichtlich nach **Sprache (`DE` / `EN`)**, **Serie** und **Favoriten (`FAV`)** und emuliert sie direkt für die Toniebox.

---

## Features

- **Animierter Splash-Screen mit integriertem Hauptmenü**:
  - Pixel-Art-Startbildschirm mit Löwen-Tonie auf einer isometrischen 3D-Toniebox und animierten NFC-Funkwellen.
  - Direkt in der unteren schwarzen Leiste wählbares 3-Punkte-Hauptmenü (`< 1. Simulation >`, `< 2. Tonie scannen >`, `< 3. Einstellungen >`).
- **Tonies per NFC einlesen & speichern (`2. Tonie scannen`)**:
  - Liest echte Tonie-Figuren (NXP ICODE SLIX / SLIX2 & ISO15693) direkt über die Rückseite des Flipper Zero ein.
  - Auswahl der Ziel-Sprache (`DE` / `EN`), Auswahl einer **bestehenden Serie** aus der Liste oder **Anlegen einer neuen Serie** per Texteingabe (`[+ Neue Serie anlegen]`) sowie Eingabe des **Tonie-Namens** zum direkten Speichern als `.nfc`-Datei auf der SD-Karte.
- **In-App Einstellungen (`3. Einstellungen`)**:
  - Konfiguriere **Start-Tab** (`DE` / `EN` / `FAV`), **Scroll-Tempo** (`Schnell` / `Normal` / `Langsam`), **Vibration** (`Ein` / `Aus`) und **Status-LED** (`Ein` / `Aus`) direkt am Flipper mit automatischer Speicherung in `tonie_simulator.cfg`.
- **2-stufige Serien-Navigation (`1. Simulation`)**:
  - **Ebene 1**: Auswahl der Hörspiel-Serie (unterstützt bis zu **350 Einträge** pro Ansicht inkl. verschachtelter Unterordner).
  - **Ebene 2**: Auswahl der einzelnen `.nfc`-Figuren innerhalb der gewählten Serie.
- **Interaktive Tabs (`[DE | EN | FAV]`)**: Nahtloser Wechsel per Links-/Rechts-Taste zwischen deutschen Tonies, englischen Tonies und deiner persönlichen Favoritenliste.
- **Header-Zähler & Marquee-Scrolling**:
  - Anzeige der aktuellen Position `[X/Y]` (z. B. `[244/268]`) direkt links oben in der Headerleiste für maximale Zeilenbreite in der Liste.
  - Automatischer horizontaler Lauftext (Marquee) für lange Serien- und Episodentitel.
- **Favoriten-Management & Config-Datei**:
  - Langes Drücken von **`OK`** (`[Hold OK]`) markiert oder entfernt einen Tonie als Favoriten (`★`) mit haptischem Feedback.
  - Persistente Speicherung in `/ext/nfc/flipper-zero-tonies/tonie_simulator.cfg`.
- **Live-Emulationsansicht**:
  - Offizielles Tonie-Kopf-Logo mit animierten Funkwellen (`SIG: [~ ~ ~]`), Lauftext für Name & Serie, 8-Byte-Hex-UID sowie Status-LED (Cyan/Blau) und Vibrations-Feedback.

---

## Ordnerstruktur auf der Flipper SD-Karte

Lege deine `.nfc`-Dumps in folgender Struktur auf der SD-Karte des Flipper Zero ab (fehlende Basisordner sowie die `tonie_simulator.cfg` werden beim ersten App-Start automatisch angelegt):

```text
/ext/nfc/flipper-zero-tonies/
├── tonie_simulator.cfg                  # Automatisch erstellte Konfigurations- & Favoritendatei
└── tonies/
    ├── German/                          # Standard-Tab (DE)
    │   ├── Benjamin Bluemchen/
    │   │   ├── 01_Wetterelefant.nfc
    │   │   └── 02_Rettet_den_Zoo.nfc
    │   ├── Bibi Blocksberg/
    │   │   └── Hexen_gibt_es_doch.nfc
    │   └── ...
    └── English/                         # Englischer Tab (EN)
        ├── Peppa Pig/
        │   └── Peppa_Bedtime_Stories.nfc
        └── ...
```

---

## Steuerung

| Ansicht | Taste | Aktion |
| :--- | :--- | :--- |
| **Splash-Screen (Hauptmenü)** | `Links` / `Rechts` (oder `Hoch`/`Runter`) | Zwischen `1. Simulation`, `2. Tonie scannen` und `3. Einstellungen` wechseln |
| **Splash-Screen (Hauptmenü)** | `OK` | Gewählten Menüpunkt öffnen |
| **Splash-Screen (Hauptmenü)** | `Back` (Kurz) | App beenden |
| **Tonie scannen** | `Links` / `Rechts` | Ziel-Sprache (`DE` / `EN`) umschalten |
| **Tonie scannen (Erkannt)** | `OK` | Serie auswählen / neu anlegen & Tonie-Namen eingeben |
| **Tonie scannen (Erkannt)** | `Hoch` / `Runter` | Neuen Scan starten |
| **Browser (Serien / Dateien)** | `Hoch` / `Runter` | Durch Liste scrollen (mit Wrap-Around) |
| **Browser (Serien / Dateien)** | `Links` / `Rechts` | Tab wechseln zwischen **`DE`**, **`EN`** und **`FAV`** |
| **Browser (Serien-Ebene)** | `OK` (Kurz) | Serien-Ordner öffnen |
| **Browser (Datei-Ebene / FAV)** | `OK` (Kurz) | NFC-Emulation für gewählten Tonie starten |
| **Browser (Datei-Ebene / FAV)** | `OK` (Gedrückt halten) | Tonie als Favorit (`★`) hinzufügen / entfernen |
| **Browser (Datei-Ebene)** | `Back` (Kurz) | Zurück zur Serien-Übersicht (merkt sich die Position) |
| **Browser (Serien-Ebene)** | `Back` (Kurz) | Zurück zum Hauptmenü (Splash-Screen) |
| **Live-Emulation** | `OK` oder `Back` | NFC-Emulation stoppen und zurück zur Liste |

---

## Konfiguration (`tonie_simulator.cfg`)

Die Datei `/ext/nfc/flipper-zero-tonies/tonie_simulator.cfg` kann direkt über die App (Favoriten per `[Hold OK]`) oder am PC im Texteditor angepasst werden:

```ini
Filetype: TonieSimulator Config
Version: 1
# DefaultTab: German, English, or Favorites
DefaultTab: German
# ScrollSpeedMs: Marquee speed in ms (100..1000)
ScrollSpeedMs: 250
# Vibro: true or false
Vibro: true
# Led: true or false
Led: true
# Favorites: full SD paths to .nfc files (also toggleable via Long-OK in app)
Favorite: /ext/nfc/flipper-zero-tonies/tonies/German/Bibi Blocksberg/Hexen_gibt_es_doch.nfc
```

---

## Installation & Build mit `uFBT`

### Voraussetzungen
1. Installiere das offizielle Flipper Build Tool (`ufbt`):
   ```bash
   python3 -m pip install --upgrade ufbt
   ```
2. Verbinde deinen Flipper Zero per USB-Kabel mit dem Mac/PC und **schließe qFlipper**, damit der serielle Port frei ist.

### SDK passend zu deiner Firmware laden
Damit beim Start kein `API Version Mismatch`-Fehler auftritt, wähle einmalig das SDK passend zur Firmware auf deinem Flipper:

- **Momentum Firmware (Release)**:
  ```bash
  ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json --channel=release
  ```
- **Official Flipper Firmware (Release)**:
  ```bash
  ufbt update --hw-target f7 --channel=release
  ```
- **Unleashed Firmware (Release)**:
  ```bash
  ufbt update --index-url=https://up.unleashedflip.com/directory.json --channel=release
  ```

### Kompilieren & direkt auf dem Flipper starten
Im Projektverzeichnis ausführen:

```bash
ufbt launch
```

Die kompilierte App wird dabei automatisch nach `/ext/apps/NFC/tonie_simulator.fap` auf deine Flipper-SD-Karte übertragen und gestartet. Später findest du sie jederzeit auf dem Flipper unter **`Apps` → `NFC` → `Tonie-Simulator by W0olf`**.
