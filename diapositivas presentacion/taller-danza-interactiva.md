# Danza Interactiva — Cuerpo, Imagen y Tiempo Real

---

## 1 · Punto de partida

No es danza con visuales de fondo.  
Es una escena donde el cuerpo **dialoga** con un sistema visual.

El movimiento no "acompaña" a la imagen.  
El movimiento **modifica, tensiona y expande** la imagen en tiempo real.

```
cuerpo ──→ cámara ──→ datos ──→ código ──→ imagen
   ↑                                          │
   └────────────── feedback ──────────────────┘
```

---

## 2 · Metalenguaje

La obra no solo muestra una danza.  
También muestra **cómo se construye su propio lenguaje**.

| Capa | Rol |
|------|-----|
| Cuerpo | Fuente de movimiento |
| Cámara / Kinect | Sensor de profundidad + esqueleto |
| Datos | 32 articulaciones, posición, velocidad |
| Código | Física de partículas, ondas, resortes |
| Imagen | Capa generativa en tiempo real |
| Escena | Composición final, proyectada |

La tecnología se vuelve **parte visible de la poética**.

---

## 3 · Interpretación en tiempo real

La obra ocurre entre **estructura e incertidumbre**.

Hay una programación previa,  
pero cada gesto produce variaciones.

```
Parámetros que el sistema lee del cuerpo:
┌──────────────┬─────────────────────────────┐
│ posición     │ ¿Dónde está cada articulación? │
│ velocidad    │ ¿Qué tan rápido se mueve?      │
│ distancia    │ ¿Manos juntas o separadas?     │
│ dirección    │ ¿Hacia dónde va el gesto?      │
│ pausa        │ ¿Cuánto tiempo sin moverse?    │
│ intensidad   │ ¿Fuerte o suave?              │
└──────────────┴─────────────────────────────┘
```

---

## 4 · El cuerpo como super-cuerpo

El cuerpo **no termina en la piel**.

```
         ┌─ cámara
         ├─ esqueleto digital (32 puntos)
cuerpo ──┼─ partículas (1500+)
         ├─ redes elásticas
         ├─ rastros de movimiento
         └─ espacio proyectado
```

El cuerpo se vuelve **interfaz viva**.

> *Escenas del proyecto:* `PinturaFluorescente`, `Furry`, `FluorescentNet`, `EnjambreOrganico`, `BodyContourPaint`

---

## 5 · Lo que el sistema NO hace

El foco está en la **relación** entre:

- Movimiento corporal
- Imagen generativa
- Visualidad en tiempo real
- Presencia escénica expandida

La música funciona como capa sonora de la obra,  
pero **también interactúa con los visuales** vía MIDI (ADSR, notas → parámetros).

---

## 6 · Herramientas de entrada

| Herramienta | Qué hace | Link |
|-------------|----------|------|
| **p5.js** | Visuales generativos en navegador | [p5js.org](https://p5js.org) | https://openprocessing.org/
| **MediaPipe** | Detección de cuerpo, manos y rostro | [mediapipe.dev](https://developers.google.com/edge/mediapipe) |
| **ml5.js** | Machine learning accesible para artistas | [ml5js.org](https://ml5js.org) |

---

## 7 · Herramientas avanzadas

### openFrameworks
Creative coding en C++ para sistemas visuales complejos  
[openframeworks.cc](https://openframeworks.cc)

```
Permite trabajar con:
├── cámara y sensores
├── Kinect (profundidad + esqueleto)
├── partículas (1500+ en tiempo real)
├── escenas modulares intercambiables
├── renderizado OpenGL con blending aditivo
└── performance en vivo
```

> *Este proyecto — ProyectoDanza2026 — tiene 5 escenas activas construidas en openFrameworks.*

---

## 8 · Música y escena

### Ableton Live
Composición, loops, texturas y organización sonora  
[ableton.com](https://www.ableton.com)

Ableton puede funcionar como soporte musical,  
pero también puede **enviar MIDI al sistema de partículas**.

```
Ableton ──MIDI──→ ProyectoDanza2026 ──→ parámetros visuales
                         │
                    ┌────┴────┐
                    │  ADSR   │
                    │ A=50ms  │
                    │ D=100ms │
                    │ S=0.8   │
                    │ R=200ms │
                    └─────────┘
```

---

## 9 · IA como asistente de código

La IA **no reemplaza** la creación artística.  
Puede ayudar a traducir **ideas poéticas en estructuras técnicas**.

```
Sirve para:
├── prototipar rápido
├── corregir errores
├── explicar código
├── ordenar escenas
├── documentar procesos
└── crear variaciones
```

> *En este proyecto, `BodyContourPaint` pasó por 7 iteraciones de diseño en una sola sesión asistida.*

---

## 10 · Gentle AI + SDD

**[Gentle AI](https://github.com/Gentleman-Programming/gentle-ai)**  
Orquestador de agentes con Spec-Driven Development.

```
SDD: Spec-Driven Development
─────────────────────────────
1. proposal  → ¿Qué queremos construir?
2. specs     → ¿Cómo debe comportarse?
3. design    → ¿Qué arquitectura?
4. tasks     → ¿En qué orden implementar?
5. apply     → Implementar
6. verify    → Validar contra specs
7. archive   → Guardar estado final
```

No se pide "haz una visual interactiva".  
Se define **comportamiento, entradas, salidas y reglas**.  
Luego se escribe el código.

---

## 11 · Ejemplo de especificación

### ❌ No pedir así:
> "Haz una visual interactiva."

### ✅ Pedir así:
> "Crear una skecth donde la mano derecha atraiga partículas,  
> la mano izquierda las disperse,  
> y la distancia entre ambas manos module la densidad visual.
> hazlo para p5js con la libreria de mediapipe.
"

```
Especificación resultante:
┌────────────────────────────────────────┐
│ Entrada:  hand_left.position           │
│           hand_right.position          │
│                                        │
│ Comportamiento:                        │
│   dist = |left - right|                │
│   densidad = map(dist, 0, 800, 0, 1)  │
│   mano derecha → F_attract = +k*r     │
│   mano izquierda → F_disperse = -k*r  │
│                                        │
│ Salida:  particleSystem.density        │
│          particleSystem.forceField     │
└────────────────────────────────────────┘
```

---

## 12 · Interacción práctica

Preguntas para el grupo:

| Pregunta | Escena de ejemplo |
|----------|-------------------|
| ¿Qué parte del cuerpo activa la imagen? | `Furry` — 32 joints forman convex hull |
| ¿Qué dato se extrae del movimiento? | `EnjambreOrganico` — velocidad entre joints |
| ¿Qué cambia visualmente? | `BodyContourPaint` — anillo crece con más joints |
| ¿Qué sensación produce? | `PinturaFluorescente` — física orbital hipnótica |
| ¿Qué relación cuerpo-sistema? | El cuerpo no se "ve", se siente en las partículas |

---

## 13 · Cierre

El código también puede ser **coreográfico**.

La imagen **no es decoración**.  
Es una **materia sensible**.

El cuerpo no solo se mueve.  
También **escribe, activa, deforma y expande** la escena.

```
cuerpo → datos → física → partículas → luz → proyección → audiencia
                                                          ↑
                        emoción ← percepción ←────────────┘
```

---

*ProyectoDanza2026 · openFrameworks · Kinect · partículas · MIDI · 2026*
