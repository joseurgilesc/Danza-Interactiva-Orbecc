# Silueta real del cuerpo desde el depth

- **Objetivo**: exponer la silueta exacta del cuerpo (píxel a píxel, desde el mapa de profundidad) a las escenas, superando la aproximación del convex hull de articulaciones.
- **Rama**: `main`.
- **Entrega**: `ask-on-risk`.
- **Test runner**: compilación + ejecución visual con usuario.

## Specs

- **S1** — `DepthStore` (singleton) con el depth alineado submuestreado (96×54) publicado por el `OrbbecAdapter` cada frame.
- **S2** — Consulta de silueta: `isBody(nx, ny, umbralMM)` → un píxel es cuerpo si `0 < depth < umbral`.
- **S3** — Una escena de demostración que rellena la silueta real con partículas (o usa el contorno real).

## Tasks

- T1 — `DepthStore` + cablear en `OrbbecAdapter`. [S1,S2] — inline — HECHO
- T2 — Escena de demostración (partículas dentro de la silueta real). [S3] — inline — HECHO (`BodySilhouetteScene`, registrada)
- T3 — Integrar el contorno real en escenas existentes (Furry, BodyContourPaint, SilhouetteDepthTunnel). [S3] — inline — HECHO

## Log

- L1 — (verbo) "lo que falta es el contorno exacto píxel a píxel sacado del mapa de profundidad: todo píxel a menos de X metros es cuerpo"
- L2 — decisión: `DepthStore` singleton (no pasar el adapter a las escenas). OrbbecAdapter publica el depth submuestreado; las escenas consultan `isBody`.
