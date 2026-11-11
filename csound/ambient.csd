<CsoundSynthesizer>
<CsOptions>
-odac -m0
</CsOptions>
<CsInstruments>

sr = 44100
ksmps = 32
nchnls = 2
0dbfs = 1

; ============================================================
; Música ambiente + OSC hacia la app oF.
; Envía a 127.0.0.1:9001 (la app escucha en ese puerto).
; ============================================================

; --- Tabla 1: onda seno suave para drones ---
giSine ftgen 1, 0, 4096, 10, 1

; ============================================================
; Capa 1 — drone grave (pad lento). Frecuencia y amplitud respiran.
; ============================================================
instr 1
  ; LFOs lentos de respiración
  kbreath = 0.5 + 0.5 * sin(2 * 3.14159 * (p4 / 60) + p5)
  kfreq = p6 * (1.0 + 0.04 * kbreath)
  kenv = p7 * (0.6 + 0.4 * sin(2 * 3.14159 * (p8 / 90) + p9))

  a1 oscili kenv, kfreq, giSine
  a2 oscili kenv * 0.12, kfreq * 1.5, giSine
  aout = (a1 + a2) * 0.6

  ; Enviar energía (amplitud) y frecuencia por OSC cada 0.1 s
  ktrig metro 10
  if (ktrig == 1) then
    OSCsend ktrig, "localhost", 9001, "/csound/energy", "f", kenv
    OSCsend ktrig, "localhost", 9001, "/csound/freq", "f", kfreq
  endif

  outs aout, aout
endin

; ============================================================
; Capa 2 — shimmer agudo (brillo tenue)
; ============================================================
instr 2
  kfreq = p4 + 12 * sin(2 * 3.14159 * (p5 / 40))
  kenv = p6 * (0.5 + 0.5 * sin(2 * 3.14159 * (p7 / 55)))

  aout oscili kenv, kfreq, giSine

  ktrig metro 10
  if (ktrig == 1) then
    OSCsend ktrig, "localhost", 9001, "/csound/shimmer", "f", kenv
  endif

  outs aout * 0.25, aout * 0.25
endin

</CsInstruments>
<CsScore>
; Capa 1: drones graves (largos, respiración lenta)
i 1 0 3600 0.012 0.0   55.0   0.22 0.011 0.0
i 1 0 3600 0.009 1.7   82.4   0.18 0.008 1.3
i 1 0 3600 0.007 3.1  110.0   0.15 0.006 2.2

; Capa 2: shimmer
i 2 0 3600 440.0 0.02 0.09 0.013
i 2 0 3600 660.0 0.03 0.06 0.017
i 2 0 3600 880.0 0.04 0.04 0.021
</CsScore>
</CsoundSynthesizer>
