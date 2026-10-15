# Danza Interactiva Orbecc — partículas + cuerpo con profundidad Orbbec

- **Objetivo**: portar `particulasOF` (app oF con sistema de escenas de partículas) al repo `Danza-Interactiva-Orbecc` y sustituir la fuente de pose 2D (webcam/Kinect) por la cámara **Orbbec Gemini 2**, dando a las partículas y a TODAS las escenas una relación real con el cuerpo vía **profundidad (depth)**.
- **Rama**: `main` (repo nuevo, vacío) · trabajo en el repo clonado.
- **Entrega**: `ask-on-risk` (local, sin PR hasta que el usuario lo pida).
- **Test runner**: compilación oF (`make`) + ejecución visual con `DISPLAY=:0`; verificación visual con el usuario (no hay test headless fiable para GL).

## Specs

- **S1** — (verbo) "lo que quiero es que las particulas y todas las escenas ahora tenga una mejor relacion con el cuerpo debido a las posiblildiades orbeec con progundidad y demas"
- **S2** — (verbo) "quiero hacer una version de ese protecto sobre todo el uso de la camara, pero con la camara orbecc con lo que hemos logrado aca /home/jose-urgiles/Escritorio/Orbec"
- **S3** — Portar todas las escenas de `particulasOF` intactas (ParticleSwarm, OrganicNet, EnjambreOrganico, RibbonDance, Furry, GridDissolve, FluorescentNet, TrappedBodyNet, BodyContourPaint, PinturaFluorescente) con su sistema de mapeo, presets y UI.
- **S4** — Cámara Orbbec Gemini 2 nativa (OrbbecSDK C++) como fuente color + depth, reutilizando la integración de `ofx-mascara` (ob::Pipeline, D2C). [assumption]
- **S5** — Pose con profundidad real: joints 3D con `z` muestreado del depth en cada joint (hoy `z=0`, 2D). [assumption derivada de "profundidad"]
- **S6** — Silueta/contorno del cuerpo desde el depth, expuesta a las escenas para efectos de relación corporal. [assumption]

## Tasks

- T1 — Fase 0: copiar fuente de `particulasOF` al repo + `config.make` Orbbec + compilar en Linux (addons ofxOsc/ofxImGui/ofxMidi). [S3] — inline — HECHO (compila → `bin/DanzaInteractiva2026-27`)
- T2 — Fase 1: `OrbbecAdapter` (IKinectAdapter) nativo con color+depth y pose MediaPipe → 18 joints con `z` real. [S4,S5] — inline — HECHO (compila + enlaza libOrbbecSDK; verificación visual pendiente del usuario)
- T3 — Fase 2: exponer depth a las escenas: `InteractionPoint.z` + `VEC3_XYZ` en el mapping engine/UI + mappings por defecto (`config/kinect-mappings.json`). [S6] — inline — HECHO
- T4 — Fase 3: mejorar cada escena para usar profundidad / `z` real. [S1,S6] — inline — HECHO (las 10 escenas escalan tamaño/brillo/ancho con la proximidad del cuerpo)
- T5 — Fase 4: verificación con usuario de todas las escenas + pulido. [S1] — inline — PENDIENTE

## Log

- L1 — (verbo) "1. primero en esta carpeta quiero que hagamos una veersion de este proyecto https://github.com/joseurgilesc/particulasOF"
- L2 — (verbo) "2. quiero hacer una version de ese protecto sobre todo el uso de la camara, pero con la camara orbecc con lo que hemos logrado aca /home/jose-urgiles/Escritorio/Orbec"
- L3 — (verbo) "lo que quiero es que las particulas y todas las escenas ahora tenga una mejor relacion con el cuerpo debido a las posiblildiades orbeec con progundidad y demas"
- L4 — decisión repo: `particulasOF` es privado; el usuario dio un PAT y el repo nuevo `Danza-Interactiva-Orbecc` (público). Fuente de referencia clonada en `/tmp/opencode/particulasOF`.
- L5 — hallazgo arquitectura: pose actual es 2D (webcam/Kinect → MediaPipe → OSC `/pose/*`, `z=0`). El adaptador de cámara es `IKinectAdapter` con `MediaPipePoseAdapter` y `MockKinectAdapter`.
- L6 — 02/11: Fase 0 compila en Linux (T1 HECHO). Fixes: addons ofxImGui/ofxMidi instalados; ofxImGui actualizado a imgui docking 1.93.1 + backend modernizado (KeyMap→AddKeyEvent, TextureId→GetTexID, IM_OFFSETOF→offsetof, MouseDown→AddMouseButtonEvent, etc.); cstdint en KinectPanel.h; renombres ImGuiCol Tab* en StudioTheme.cpp; ImHelpers.cpp deshabilitado (no usado).
- L7 — 02/11: T2 HECHO. `OrbbecAdapter` (src/adapters/kinect/OrbbecAdapter.h/.cpp) implementa IKinectAdapter nativo: OrbbecSDK color RGB 1280x720 + depth Y16 D2C (ALIGN_D2C_HW_MODE), pose MediaPipe vía `pose_detector.py` (TCP 127.0.0.1:5005, solo pose, modelo pose_landmarker_lite.task), mapeo 33→18 joints (head/neck/shoulder/elbow/wrist/hand/hip/knee/ankle/spine_base/spine_mid), y `z` real muestreado del depth en cada joint (normalizado 0..1, rango 400–4000mm). Cableado en ofApp.cpp (reemplaza MediaPipePoseAdapter). `run.sh` arranca detector + app (reusa .venv de ofx-mascara, Python 3.12 + mediapipe).
- L8 — 02/11: verificada la app en vivo (cámara Orbbec + pose fluyendo). Fixes runtime: (1) main.cpp necesitaba `glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11)` (crash Wayland "X11: Platform not initialized"); (2) KinectPanel `names[]` desalineado con enum ConnectionState (imprimía "Error" en vez de "Connected"); (3) los config/presets NO se encontraban porque oF en Linux resuelve rutas contra `bin/data/` → se sincroniza `config/` → `bin/data/config/` en run.sh; (4) `kinectMappingFromJson` crasheaba con gridDissolve.json porque usa `"transformType":"VEC2_XY"` (string) → ahora acepta int o string.
- L9 — 02/11: T3 HECHO. `InteractionPoint.z` (profundidad 0..1), `KinectTransformType::VEC3_XYZ` (dispatch `glm::vec3(x,y,z)`), UI en KinectPanel, y `config/kinect-mappings.json` con 18 mappings VEC3_XYZ → particleSwarm. T4 parcial: ParticleSwarmScene ya escala el tamaño de partícula con la proximidad (más cerca = más grandes).
- L10 — 03/11: solicitud extra usuario ("no se ven bien las letras de los paneles" + "shortcuts en pantalla pero limpio al salir de controles"). Fixes: (1) fuentes Roboto descargadas a `bin/data/fonts/` (antes faltaban → placeholder ilegible); (2) tamaño de fuente 16→18px; (3) overlay de atajos (`UIManager::drawShortcutsOverlay`) no interactivo, solo visible con la UI (U la oculta → escenario limpio); (4) segfault de font-atlas resuelto: las fuentes deben cargarse ANTES de `gui_.setup()` (que construye la textura de la atlas), y se eliminó el `ImGui::CreateContext()` redundante en `Gui::setup()` de ofxImGui que descartaba las fuentes.
- L11 — 03/11: T4 HECHO. `setParameter(vec3)` implementado en las 9 escenas restantes (guarda z en el InteractionPoint) + helper `averageProximity()` en InteractionPoint.h. Efecto por escena (proximidad = 1-z): particleSwarm/bodyContourPaint/pinturaFluorescente/enjambre → tamaño de partícula; organicNet/trappedBodyNet → ancho de línea + radio de nodo; fluorescentNet → tamaño de partícula libre + línea + nodo; ribbonDance → ancho de cinta; gridDissolve → brillo de celda; furry → alfa del contorno corporal.
