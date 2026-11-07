# OpenRGB Screen Sync

Plugin per OpenRGB 1.1 (API plugin v5, Qt6) che **spegne i LED quando il sistema operativo spegne lo schermo** e li riaccende quando lo schermo si riaccende.
Vale per **tutti** i dispositivi OpenRGB (tastiera, mouse, ventole, scheda madre, ...).

> **Funziona solo su Linux** (x86_64). Non esiste una versione per Windows o macOS.
>
> **Testato su:** CachyOS (base Arch Linux), KDE Plasma su Wayland, OpenRGB 1.1. Non l'ho provato su altre distro o altri desktop.

Non l'ho trovato già fatto: esistono solo script esterni che richiamano `openrgb` a riga di comando (lenti, ~3 s a ogni chiamata).

## Cosa fa
- Rileva lo standby dello schermo con il metodo giusto per ogni desktop:

| Desktop | Metodo |
|---|---|
| KDE Plasma (Wayland) | protocollo `org_kde_kwin_dpms` (provato) |
| sway, Hyprland, river, labwc (Wayland) | protocollo `wlr-output-power-management` |
| GNOME (Wayland e X11) | D-Bus `org.gnome.Mutter.DisplayConfig` (`PowerSaveMode`) |
| i3, XFCE, KDE X11, ... (X11) | estensione DPMS di X |

  Provato davvero solo su KDE Plasma/Wayland; gli altri backend sono scritti secondo i protocolli ufficiali ma non verificati su quei desktop.
- **Spegnimento schermo** -> i LED sfumano fino a spegnersi (oppure di colpo).
- **Accensione schermo** -> i LED tornano esattamente com'erano (colori, modo, luminosita') con dissolvenza o di colpo.
- Se lo schermo si riaccende a metà dissolvenza, la sfumatura inverte da dove e' arrivata.
- Spegnimento/accensione del **PC** (sospensione, shutdown) NON sono gestiti: conta solo lo schermo.
- Se un monitor e' acceso, i LED restano accesi: si spengono quando **tutti** i monitor sono in standby.

## Scheda "Schermo" in OpenRGB
- Casella per attivare/disattivare tutto.
- Spegnimento e accensione, ognuno con: *dissolvenza graduale* (durata da 0,1 a 30 secondi) oppure *di colpo*.
- Dispositivi: tutti, oppure solo quelli scelti. Il comportamento e' lo stesso per tutti.
- Pulsante **Prova** per spegnere e riaccendere subito, senza aspettare lo schermo.
- Le impostazioni si salvano da sole nelle impostazioni di OpenRGB (chiave `ScreenSyncPlugin`).
- Predefiniti: spegnimento 2 s, accensione 1 s.

## Come sfuma ogni dispositivo
1. Modo con colori per LED (Direct/Custom): scala i colori verso il nero.
2. Modo con luminosita': scala la luminosita' (gli effetti hardware restano intatti). Se la luminosita' minima non e' zero, alla fine passa a un modo Direct nero.
3. Modo senza nessuna delle due: al termine passa a un modo Direct nero (se esiste) e al ritorno ripristina il modo.

Dispositivi con colori "vuoti": se OpenRGB conosce per un dispositivo solo colori tutti neri (ad esempio perche' il profilo e' stato caricato prima che il dispositivo fosse registrato), il plugin lo salta, perche' non sa com'e' acceso davvero. Carica di nuovo il profilo dalla scheda Profili di OpenRGB e il dispositivo torna a sfumare.

Limite noto: se un altro plugin/effetto (es. HardwareSync) scrive i colori nello stesso momento, il fade puo' litigare con esso.

## Installazione
```
./installa.sh
```
Pacchetto pronto: scarica lo ZIP dalla pagina [Releases](https://github.com/zimu96/OpenRGB-ScreenSync/releases), estrailo e lancia `bash install.sh` (compilato su Ubuntu 22.04, gira su tutte le distro con glibc >= 2.35). Compilando da sorgenti, il risultato finisce in `~/.config/OpenRGB/plugins/` (quella che OpenRGB carica).

Richiede: cmake, qt6-base (con DBus), wayland, libx11 + libxext (opzionali, per X11), OpenRGB 1.1 (Qt6). Riavvia OpenRGB dopo l'installazione.

## File
- `src/DpmsWatcher.*` rilevamento stato schermo (Wayland KDE/wlroots, GNOME D-Bus, X11)
- `src/FadeEngine.*` dissolvenza e ripristino
- `src/SettingsWidget.*` scheda in OpenRGB
- `src/ScreenSyncPlugin.*` entry point del plugin
- `protocol/*.xml` protocolli Wayland KDE DPMS e wlr-output-power (riscritti a mano)
- `packaging/` installer e istruzioni inclusi negli ZIP delle release
- `.github/workflows/release-builds.yml` build automatica (Linux) e pubblicazione della release al push di un tag `v*`
- `sdk/` header SDK plugin OpenRGB (copiati dai sorgenti di OpenRGB)

## Licenza
GPL-2.0-or-later (come OpenRGB, da cui provengono gli header in `sdk/`).
