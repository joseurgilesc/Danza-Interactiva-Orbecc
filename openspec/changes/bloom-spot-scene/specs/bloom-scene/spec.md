# Delta for BloomSpotScene

## Purpose

Variación de FluorescenteBloomScene donde BloomUnits spawnan solo dentro de un área circular configurable ("spot"), con fondo negro fuera del círculo.

## ADDED Requirements

### Requirement: Parámetros de Spot

El sistema DEBE registrar tres nuevos parámetros para configurar el área circular:

| ID | Descripción | Rango | Default |
|---|-------------|-------|---------|
| `spotCenterX` | Centro X del spot (0-1 = pantalla completa) | 0.0-1.0 | 0.5 |
| `spotCenterY` | Centro Y del spot (0-1 = pantalla completa) | 0.0-1.0 | 0.5 |
| `spotRadius` | Radio del spot (0-1 = pantalla completa) | 0.05-0.5 | 0.25 |

#### Scenario: Parámetros se registran correctamente

- GIVEN BloomSpotScene se activa
- WHEN Se llama registerParameters()
- THEN Los 3 parámetros de spot aparecen en ParameterRegistry
- AND Cada parámetro tiene min, max, default, step válidos

### Requirement: Campo de Spot

El sistema DEBE calcular un campo circular con:

- **Centro del campo**: `(spotCenterX * screenWidth, spotCenterY * screenHeight)`
- **Radio del campo**: `spotRadius * min(screenWidth, screenHeight)`

#### Scenario: Campo se recalcula al cambiar tamaño de ventana

- GIVEN spotCenterX=0.5, spotCenterY=0.5, spotRadius=0.25
- WHEN La ventana cambia a 800x600
- THEN centro = (400, 300), radio = 150 (0.25 * 600)
- AND Cuando la ventana cambia a 1200x800
- THEN centro = (600, 400), radio = 200 (0.25 * 800)

### Requirement: Spawning limitado al Spot

El sistema DEBE generar nuevas BloomUnits solo dentro del círculo del spot.

#### Scenario: Nueva unit spawna dentro del spot

- GIVEN El sistema necesita spawnear una nueva BloomUnit
- WHEN La posición generated es `dist(pos, center) < fieldRadius`
- THEN La BloomUnit se inicializa correctamente
- AND Queda disponible en el pool

#### Scenario: Nueva unit NO spawna fuera del spot

- GIVEN El sistema necesita spawnear una nueva BloomUnit
- WHEN La posición generada tiene `dist(pos, center) >= fieldRadius`
- THEN Se DESCARTA esa posición
- AND Se genera una nueva候选 posición
- AND Este proceso se repite hasta encontrar posición válido o max Attempts

### Requirement: Campo de fuerza usa Spot como centro

El sistema DEBE usar spotCenter en lugar de screenCenter para las fuerzas físicas:

- F_center: atracción hacia spotCenter (no screen center)
- F_wave: onda desde spotCenter (no desde screen center)
- F_swirl: rotación alrededor de spotCenter

#### Scenario: Units se mueven hacia el spot

- GIVEN Una BloomUnit en posición (100, 100) con spotCenter (400, 300)
- WHEN Se aplica update() con física
- THEN F_center jala hacia (400, 300), no hacia screen center
- AND La unit eventualmente orbita cerca del spot

### Requirement: Registro de escena

El sistema DEBE registrar BloomSpotScene con ID "5-BloomSpot" en el sistema de escenas.

#### Scenario: Escena disponible en UI

- GIVEN ofApp está ejecutándose
- WHEN El usuario selecciona "5-BloomSpot"
- THEN BloomSpotScene se activa
- AND El área circular es visible con bloom units dentro

### Requirement: Herencia de parámetros parent

El sistema DEBE mantener todos los parámetros de FluorescenteBloomScene:

| Categoría | Parámetros |
|-----------|-------------|
| Física | k, A, w, q, beta, S, ws, qs |
| Bloom | numBloomUnits, minSize, maxSize, noiseAmount |
| Interacción | mouseRadius, mouseForce |
| Animación | breathingSpeed, breathingAmplitude |

#### Scenario: Parámetros parent funcionan sin cambios

- GIVEN BloomSpotScene está activa
- WHEN El usuario modifica `k` o `breathingSpeed`
- THEN El comportamiento cambia exactamente como en FluorescenteBloomScene
- AND Los parámetros de spot coexistentes funcionan correctamente

## MODIFIED Requirements

### Requirement: Fondo negro fuera del spot

(Previos: FluorescenteBloomScene usa el fondo definido en BaseScene)

El sistema DEBE mantener fondo negro (no draw background) tanto dentro como fuera del spot. El área fuera del spot permanece negra.

#### Scenario: Fondo fuera del spot es negro

- GIVEN spotRadius = 0.25, spotCenterX/Y = 0.5, screen 800x600
- WHEN Se renderiza la escena
- THEN Área fuera del círculo de radio 150 centrado en (400,300) es negra
- AND Solo las bloom units dentro del círculo son visibles

## REMOVED Requirements

### Requisito: Spawning en pantalla completa

(Reason: Reemplazado por spawning limitado al spot)

El sistema NO DEBE generar BloomUnits fuera del área circular del spot.

## Success Criteria

- [ ] 3 nuevos parámetros de spot registrados
- [ ] Campo circular se calcula correctamente
- [ ] Units spawnan solo dentro del spot
- [ ] Fuerza física usa spot como centro
- [ ] Registro "5-BloomSpot" funciona
- [ ] Parámetros parent funcionan sin cambios
- [ ] Fondo fuera del spot es negro