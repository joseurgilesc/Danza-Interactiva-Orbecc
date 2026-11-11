#!/usr/bin/env bash
# Arranca el detector de pose (MediaPipe) + la app oF de Danza Interactiva Orbecc.
# Uso:  bash run.sh start | stop | status
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
DET="$ROOT/pose_detector.py"
APP="$ROOT/bin/DanzaInteractiva2026-27"

# Python con mediapipe (Python 3.12 — mediapipe no tiene wheel para 3.13/3.14).
# Se reutiliza el .venv ya existente de ofx-mascara; cámbialo si creas uno propio.
PYTHON="$HOME/openFrameworks/apps/myApps/ofx-mascara/detector/.venv/bin/python"
if [ ! -x "$PYTHON" ]; then
  echo "⚠️  Python con mediapipe no encontrado en: $PYTHON"
  echo "   Crea un venv con Python 3.12 e instala: pip install mediapipe"
  PYTHON="python3"
fi

detector_pid() { pgrep -f "pose_detector.py" | head -1; }
app_pid() { pgrep -f "bin/DanzaInteractiva2026-27" | head -1; }
csound_pid() { pgrep -f "csound.*ambient.csd" | head -1; }

start() {
  # Sincronizar config del repo → bin/data/config (data dir de oF en Linux)
  mkdir -p "$ROOT/bin/data/config"
  cp -f "$ROOT/config/kinect-mappings.json" "$ROOT/bin/data/config/" 2>/dev/null || true
  cp -rf "$ROOT/config/presets" "$ROOT/bin/data/config/" 2>/dev/null || true
  cp -f "$ROOT/config/midi.json" "$ROOT/bin/data/config/" 2>/dev/null || true
  cp -f "$ROOT/config/sequence.json" "$ROOT/bin/data/config/" 2>/dev/null || true

  # Reiniciar el detector SIEMPRE (para que use el código Python más reciente).
  if [ -n "$(detector_pid)" ]; then
    kill "$(detector_pid)" 2>/dev/null
    sleep 1
  fi
  echo "▶ arrancando pose_detector.py..."
  setsid -f "$PYTHON" "$DET" > "$ROOT/detector.log" 2>&1
  sleep 2

  # Arrancar Csound (música ambiente) si está instalado.
  if command -v csound >/dev/null 2>&1; then
    if [ -z "$(csound_pid)" ]; then
      echo "▶ arrancando csound (ambient.csd)..."
      setsid -f csound -odac "$ROOT/csound/ambient.csd" > "$ROOT/csound.log" 2>&1
      sleep 1
    fi
  else
    echo "ℹ️  Csound no instalado — sin música ambiente (sudo apt install csound)"
  fi

  if [ -z "$(app_pid)" ]; then
    echo "▶ arrancando app oF (DISPLAY=:0)..."
    DISPLAY=:0 setsid -f "$APP" > "$ROOT/app.log" 2>&1
    sleep 1
  fi
  echo "detector pid: $(detector_pid)"
  echo "app pid:      $(app_pid)"
}

stop() {
  [ -n "$(app_pid)" ] && kill "$(app_pid)" 2>/dev/null && echo "app detenida" || true
  [ -n "$(detector_pid)" ] && kill "$(detector_pid)" 2>/dev/null && echo "detector detenido" || true
  [ -n "$(csound_pid)" ] && kill "$(csound_pid)" 2>/dev/null && echo "csound detenido" || true
}

status() {
  echo "detector: $(detector_pid 2>/dev/null || echo 'no')"
  echo "app:      $(app_pid 2>/dev/null || echo 'no')"
  echo "csound:   $(csound_pid 2>/dev/null || echo 'no')"
}

case "${1:-start}" in
  start) start ;;
  stop) stop ;;
  status) status ;;
  *) echo "uso: $0 start|stop|status" ;;
esac
