# OpenRGB Screen Sync

Plugin per OpenRGB 1.1 (API plugin v5, Qt6) che **spegne i LED quando il sistema operativo spegne lo schermo** e li riaccende quando lo schermo si riaccende.
Vale per **tutti** i dispositivi OpenRGB (tastiera, mouse, ventole, scheda madre, ...), oppure solo per quelli che scegli.
Se dal PC esce dell'audio, puoi far restare accesi i LED anche con lo schermo spento (utile se sei in stanza ad ascoltare musica).

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
- Opzione "Spegni subito i LED quando spengo il PC": vale solo per lo **spegnimento** del computer, non per il riavvio né per la sospensione.
- Opzione "Audio": con lo schermo spento i LED (tutti o solo quelli scelti) restano accesi finche' dal PC esce audio. Vedi la sezione **Audio** qui sotto.
- Se un monitor e' acceso, i LED restano accesi: si spengono quando **tutti** i monitor sono in standby.

## Audio: LED accesi finche' si ascolta qualcosa
Caso d'uso: lo schermo va in standby, ma sei ancora in stanza (divano, musica, film dal telefono) e i LED accesi ti fanno comodo, mentre lo schermo acceso no.

- Con la casella attiva, a schermo spento i dispositivi scelti **restano accesi finche' c'e' un flusso audio** in uscita dal PC. Gli altri si spengono come sempre.
- Conta **qualunque uscita**: casse, uscita frontale o posteriore, HDMI, cuffie, **Bluetooth**, e anche l'audio che arriva dal **telefono** collegato via Bluetooth o KDE Connect e riprodotto dal PC. Non importa che il PC sia bloccato.
- Se i LED si erano gia' spenti e l'audio parte dopo (anche dopo ore), **si riaccendono con dissolvenza**.
- Quando l'audio finisce **non si spengono subito**: aspettano qualche secondo (regolabile da 1 a 120 s, predefinito 8), cosi' le pause tra un brano e l'altro (Spotify, ecc.) non li fanno lampeggiare.
- Se il **volume dell'uscita e' a zero o l'uscita e' muta**, quell'audio non conta: vuol dire che non vuoi ascoltare.
- Scelta dei dispositivi: *tutti quelli scelti per lo spegnimento* oppure *solo alcuni* (es. solo i LED del case, non la tastiera). L'elenco e' indipendente da quello dello spegnimento schermo.

Come rileva l'audio (valido su qualunque distro):
- Con **PipeWire o PulseAudio** (quasi tutte le distro attuali) usa `pactl`: legge stato, volume e muto di ogni uscita. Copre anche il Bluetooth.
- Senza `pactl` legge lo stato di ALSA dal kernel (`/proc/asound`): nessun programma richiesto. In questo caso il volume a zero non e' verificabile e il Bluetooth non e' visibile.
- Limite: se un'app tiene aperto un flusso audio silenzioso (es. un video in pausa che non rilascia l'audio), i LED possono restare accesi.

## Scheda "Schermo" in OpenRGB
- Casella per attivare/disattivare tutto.
- Spegnimento e accensione, ognuno con: *dissolvenza graduale* (durata da 0,1 a 30 secondi) oppure *di colpo*.
- Dispositivi: tutti, oppure solo quelli scelti. Il comportamento e' lo stesso per tutti.
- Riquadro **Audio**: casella, attesa dopo la fine dell'audio, dispositivi (tutti o solo alcuni).
- Pulsante **Prova** per spegnere e riaccendere subito, senza aspettare lo schermo.
- Le impostazioni si salvano da sole nelle impostazioni di OpenRGB (chiave `ScreenSyncPlugin`).
- Predefiniti: spegnimento 2 s, accensione 1 s.

## Come sfuma ogni dispositivo
1. Modo con colori per LED (Direct/Custom): scala i colori verso il nero.
2. Modo con luminosita': scala la luminosita' (gli effetti hardware restano intatti). Se la luminosita' minima non e' zero, alla fine passa a un modo Direct nero.
3. Modo senza nessuna delle due: al termine passa a un modo Direct nero (se esiste) e al ritorno ripristina il modo.

Dispositivi con colori "vuoti": se OpenRGB conosce per un dispositivo solo colori tutti neri (ad esempio perche' il profilo e' stato caricato prima che il dispositivo fosse registrato), il plugin lo salta, perche' non sa com'e' acceso davvero. Succede di solito con dispositivi registrati da altri plugin dopo che OpenRGB ha gia' caricato il profilo di apertura; la correzione spetta al plugin di quel dispositivo (riapplicare il profilo dopo la registrazione). Nel frattempo, carica di nuovo il profilo dalla scheda Profili di OpenRGB e il dispositivo torna a sfumare.

Limite noto: se un altro plugin/effetto (es. HardwareSync) scrive i colori nello stesso momento, il fade puo' litigare con esso.

## Installazione
```
./installa.sh
```
Pacchetto pronto: scarica lo ZIP dalla pagina [Releases](https://github.com/zimu96/OpenRGB-ScreenSync/releases), estrailo e lancia `bash install.sh` (compilato su Ubuntu 22.04, gira su tutte le distro con glibc >= 2.35). Compilando da sorgenti, il risultato finisce in `~/.config/OpenRGB/plugins/` (quella che OpenRGB carica).

Richiede: cmake, qt6-base (con DBus), wayland, libx11 + libxext (opzionali, per X11), OpenRGB 1.1 (Qt6). Riavvia OpenRGB dopo l'installazione.

## File
- `src/DpmsWatcher.*` rilevamento stato schermo (Wayland KDE/wlroots, GNOME D-Bus, X11)
- `src/FadeEngine.*` dissolvenza e ripristino (livello per ogni dispositivo)
- `src/AudioWatcher.*` rilevamento dell'audio in uscita (pactl, con ripiego su ALSA)
- `src/ShutdownWatcher.*` spegnimento del PC (systemd-logind)
- `src/SettingsWidget.*` scheda in OpenRGB
- `src/ScreenSyncPlugin.*` entry point del plugin
- `protocol/*.xml` protocolli Wayland KDE DPMS e wlr-output-power (riscritti a mano)
- `packaging/` installer e istruzioni inclusi negli ZIP delle release
- `.github/workflows/release-builds.yml` build automatica (Linux) e pubblicazione della release al push di un tag `v*`
- `sdk/` header SDK plugin OpenRGB (copiati dai sorgenti di OpenRGB)

## Licenza
GPL-2.0-or-later (come OpenRGB, da cui provengono gli header in `sdk/`).
