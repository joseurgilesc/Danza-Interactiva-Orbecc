# Multi-Scene Parameter Persistence Specification

## Purpose

Sistema para guardar y cargar TODOS los parámetros de TODAS las escenas de forma eficiente sin conflictos de nombres.

## Requirements

### Requirement: Save All Scenes Parameters

El sistema DEBE guardarTODOS los parámetros REGISTRADOS de TODAS las escenas cargadas a un archivo JSON cuando el usuario presiona el botón "Save Parameters".

#### Scenario: Save all parameters from all scenes

- GIVEN hay 4 escenas cargadas: "pinturaFluorescente", "furry", "anemone", "bloomsSpot"
- AND cada escena tiene parámetros propios (ej: "pinturaFluorescente.k", "furry.speed", etc)
- WHEN el usuario presiona el botón "Save Parameters"
- THEN el sistema DEBE escribir un archivo JSON en `config/parameters.json`
- AND cada parámetro DEBE tener un ID con formato `"sceneId.paramId"` (ej: `"pinturaFluorescente.k"`)
- AND NO DEBE haber conflictos de nombres entre escenas

#### Scenario: Save handles missing scene gracefully

- GIVEN una escena está en el registry pero no tiene parámetros
- WHEN se presiona el botón Save
- THEN el sistema DEBE omitir esa escena sin error
- AND continuar con las siguientes escenas

### Requirement: Manual Load Button

El sistema DEBE tener un botón "Load Parameters" en ParamPanel que carga los parámetros guardados cuando el usuario lo presiona.

#### Scenario: Manual load updates all scenes

- GIVEN existe el archivo `config/parameters.json` con parámetros guardados
- AND las escenas están previamente cargadas
- WHEN el usuario presiona el botón "Load Parameters"
- THEN el sistema DEBE cargar el archivo JSON
- AND actualizar CADA parámetro en su escena correspondiente usando el scene ID del JSON
- AND el sistema NO DEBE usar lógica de matching por substring

#### Scenario: Load with no file shows error

- GIVEN NO existe el archivo `config/parameters.json`
- WHEN el usuario presiona "Load Parameters"
- THEN el sistema DEBE loguear un warning indicando archivo no encontrado
- Y NO DEBE crashear

### Requirement: Auto-load on Startup

El sistema DEBE cargar automáticamente los parámetros guardados al iniciar la aplicación.

#### Scenario: Auto-load restores saved parameters

- GIVEN existe `config/parameters.json` con parámetros guardados
- AND hay 4 escenas cargadas
- WHEN la aplicación inicia (setup())
- THEN el sistema DEBE ejecutar el mismo flujo de load que el botón manual
- AND restaurar los valores guardados en cada escena

#### Scenario: No saved file on first run

- GIVEN es la primera ejecución (no existe parameters.json)
- WHEN la aplicación inicia
- THEN el sistema DEBE continuar normalmente sin error

### Requirement: No Name Conflicts

El sistema DEBE garantizar que los IDs de parámetros sean únicos usando el scene ID como prefijo.

#### Scenario: Same param name in different scenes

- GIVEN escena "anemone" tiene parámetro "speed"
- AND escena "furry" tiene parámetro "speed"
- WHEN se guardan ambos
- THEN el archivo JSON DEBE tener "anemone.speed" y "furry.speed" como claves separadas
- AND al cargar, "anemone.speed" VA solo a la escena "anemone", no a "furry"

## MODIFIED Requirements

### Requirement: Removed - Fragile String Matching

(Reason: La lógica actual de matching por substring ("prefix contains sceneName") es frágil y propensa a errores)

- GIVEN la lógica anterior: `if (sceneId.find(prefix) != npos)`
- WHEN se guarda/carga
- THEN el sistema DEBE usar lookup directo por scene ID, NO substring matching

## REMOVED Requirements

### Requirement: Removed - Scene ID Extraction from String

(Reason: Se usaba para parsear IDs tipo "3-Anemone" y extraer "Anemone" - esto ya no es necesario con namespacing determinista)

- GIVEN la lógica anterior: `size_t dashPos = sceneId.find('-');`
- WHEN se procesa un sceneId
- THEN el sistema DEBE usar el sceneId exacto sin transformación