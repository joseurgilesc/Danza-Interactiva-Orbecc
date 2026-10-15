# Génesis Volumétrica — 8 escenas 3D con profundidad Orbbec

- **Objetivo**: 8 escenas volumétricas 3D ("Génesis Volumétrica") que usan la profundidad real de la Orbbec para materializar el cuerpo en 3D, en 4 fases narrativas (Origen → Estructura → Disolución → Expansión). Coexisten con las 10 escenas 2D existentes.
- **Rama**: `main`.
- **Entrega**: `ask-on-risk` (local, sin PR hasta que el usuario lo pida).
- **Test runner**: compilación + ejecución visual con usuario (cámara + DISPLAY=:0).

## Specs

- **S1** — 8 escenas 3D: DepthSliceScan, VolumetricLattice, ElasticDepthMesh, DepthKineticVortex, VoxelFluidDisruption, GravitationalPointCloud, SilhouetteDepthTunnel, AcousticDepthEchoes.
- **S2** — Paleta global: `#1D2B53` (cNoche), `#7E2553` (cMagenta), `#FF004D` (cNeon), `#FAEF5D` (cCore).
- **S3** — Mapeo Z: `worldZ = ofMap(z, 0, 1, 400, -800)`. Cámara 3D **FIJA** (no orbitable; el ratón queda para interacción).
- **S4** — Core motif: núcleo palpitante en el pecho (`SPINE_MID`) dibujado al final de `draw()` de las 8 escenas.
- **S5** — Infraestructura 3D compartida: `InteractionPoint::worldPos()`, `GenesisCommon.h` (paleta + `computeConvexHull` + `pointInPolygon` + `toWorld` + cámara + core motif).

## Tasks

- T0 — Fase 0: infraestructura 3D (`GenesisCommon.h`, `worldPos()`, refactor Furry al convex hull compartido). [S2,S3,S5] — inline — HECHO
- T1 — `DepthSliceScan` (planos Z + círculos concéntricos aditivos). [S1,S3] — inline — HECHO (compila + registrada)
- T2 — `VolumetricLattice` (VBO de líneas conectando joints por distancia). [S1,S3] — inline — HECHO
- T3 — `ElasticDepthMesh` (rejilla N×M con resortes deformada en Z). [S1,S3] — inline — HECHO
- T4 — `DepthKineticVortex` (partículas 3D por aceleración Z). [S1,S3] — inline — HECHO
- T5 — `VoxelFluidDisruption` (vóxeles 3D + Perlin + flotabilidad). [S1,S3] — inline — HECHO
- T6 — `GravitationalPointCloud` (nube de puntos deformada por el pecho en Z). [S1,S3] — inline — HECHO
- T7 — `SilhouetteDepthTunnel` (hulls históricos extruidos en Z). [S1,S3] — inline — HECHO
- T8 — `AcousticDepthEchoes` (esferas wireframe por desaceleración Z). [S1,S3] — inline — HECHO
- T9 — Integración: registro de las 8 escenas + verificación visual. [S1,S4] — inline — HECHO (8 registradas; verificación visual pendiente del usuario)

## Log

- L1 — (verbo) documento del usuario "DOCUMENTO DE ARQUITECTURA TÉCNICA Y NARRATIVA ... Génesis Volumétrica - El Viaje de la Materia Digital (Integración Orbbec Gemini 2)" con paleta `#1D2B53/#7E2553/#FF004D/#FAEF5D`, 4 fases (Origen/Estructura/Disolución/Expansión), 8 escenas, `worldZ = ofMap(pt.z, 0, 1, 400, -800)`, y core motif `ofDrawSphere(chest, 10 + 5*sin(t*3))`.
- L2 — decisiones usuario: (1) salto a 3D sí; (2) cámara fija; (3) core motif solo en las 8 nuevas; (4) empezar por Fase 0 (infraestructura).
- L3 — evaluación: las 8 escenas pertinentes y factibles. Correcciones: `InteractionPoint::worldPos()` (puente 2D→3D), cámara fija en (0,0,1400) mirando al origen, chestPoint = `SPINE_MID`, convex hull extraído del Furry a `GenesisCommon.h`.
- L4 — 03/11: Fase 0 + T1 HECHOS. `GenesisCommon.h` (paleta cNoche/cMagenta/cNeon/cCore, `toWorld`, `beginGenesisCamera`, `computeConvexHull`+`pointInPolygon`, `drawCoreMotif`), `InteractionPoint::worldPos()`, refactor Furry al convex hull compartido, y escena `DepthSliceScan` (6 planos Z + anillos concéntricos aditivos + core motif) registrada en ofApp. Compila + app viva.
- L5 — 03/11: T2–T9 HECHOS. Las 8 escenas 3D implementadas, registradas en ofApp (18 escenas totales) y compilando. VolumetricLattice (VBO líneas por distancia, color por proximidad), ElasticDepthMesh (rejilla 32×24 resortes + empuje Z), DepthKineticVortex (partículas por aceleración Z), VoxelFluidDisruption (ofDrawBox + Perlin + flotabilidad), GravitationalPointCloud (ofVboMesh points + glPointSize + gravedad del pecho), SilhouetteDepthTunnel (convex hull histórico + túnel), AcousticDepthEchoes (esferas por frenado Z). Verificación visual pendiente del usuario.
