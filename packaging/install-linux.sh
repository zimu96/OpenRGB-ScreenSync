#!/usr/bin/env bash
#----------------------------------------------------------------------------#
#  Installatore del plugin Screen Sync per OpenRGB (Linux)                   #
#  Uso: bash install.sh                                                      #
#  Copia libScreenSyncPlugin.so in ~/.config/OpenRGB/plugins/                #
#----------------------------------------------------------------------------#
set -e

PLUGIN_DIR="$HOME/.config/OpenRGB/plugins"
SO="libScreenSyncPlugin.so"

if [ ! -f "$SO" ]; then
    echo "ERRORE: $SO non trovato accanto a questo script." >&2
    echo "Esegui questo script dalla cartella dove hai estratto il file." >&2
    exit 1
fi

mkdir -p "$PLUGIN_DIR"

# Backup dell'eventuale versione precedente
if [ -f "$PLUGIN_DIR/$SO" ]; then
    BAK="$PLUGIN_DIR/$SO.bak-$(date +%Y%m%d-%H%M%S)"
    cp "$PLUGIN_DIR/$SO" "$BAK"
    echo "Backup della versione precedente salvato in:"
    echo "  $BAK"
    # rimuovo il file prima di copiare: sovrascriverlo mentre OpenRGB e' aperto puo' farlo andare in crash
    rm -f "$PLUGIN_DIR/$SO"
fi

cp "$SO" "$PLUGIN_DIR/$SO"

echo
echo "Installato: $PLUGIN_DIR/$SO"
echo
echo "Prossimi passi:"
echo "  1. Chiudi e riavvia OpenRGB:  openrgb --startminimized"
echo "  2. Impostazioni -> Plugin -> spunta 'Attivato' su Screen Sync"
echo "  3. Apri la scheda 'Schermo' e scegli come devono sfumare i LED"
