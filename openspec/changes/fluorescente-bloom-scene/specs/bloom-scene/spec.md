# Delta for FluorescenteBloomScene

## Purpose

Nueva escena visual con bloom units orgánicos que heredan la física de ondas de PinturaFluorescenteScene pero con renderizado de formas suaves, glow/halo, y movimiento elástico/breathing.

## ADDED Requirements

### Requirement: BloomUnitPool de tamaño configurable

El sistema DEBE crear un pool pre-allocado de BloomUnits con capacidad configurable entre 300 y 900 unidades.

#### Scenario: Pool se inicializa con valor por defecto

- GIVEN El sistema inicia FluorescenteBloomScene
- WHEN Se llama setup() con numBloomUnits = 500 (default)
- THEN Se crea un pool con 500 BloomUnits inactivas
- AND La capacidad máxima es 500

#### Scenario: Pool soporta valores fuera del rango

- GIVEN El usuario configura numBloomUnits = 1500
- WHEN Se llama setup()
- THEN El sistema limita automáticamente a 900 (max)
- AND El pool se crea con capacidad 900

### Requirement: BloomUnit con campos de renderizado orgánico

Cada BloomUnit DEBE tener:
- pos: posición en pantalla (glm::vec2)
- vel: velocidad (glm::vec2)
- color: color fluorescente (ofFloatColor)
- baseSize: tamaño base antes de respiración (float)
- currentSize: tamaño actual con animación (float)
- glow: intensidad del halo (float, 0-1)
- breathingPhase: fase de oscilación (float)
- noiseOffset: variación orgánica por noise (glm::vec2)
- phase: hereda de física de ondas (float)
- active: estado activo (bool)

#### Scenario: BloomUnit se inicializa correctamente

- GIVEN Un BloomUnit inactivo
- WHEN Se llama init(pos, vel, color, size)
- THENTodos los campos se configuran
- AND baseSize = size, currentSize = size, glow = 0.5, breathingPhase = random

### Requirement: Renderizado orgánico con glow/halo

El sistema DEBE renderizar cada BloomUnit como una forma suave con halo de glow.

#### Scenario: BloomUnit se renderiza con glow

- GIVEN Un BloomUnit activo con color cyan, size=40, glow=0.6
- WHEN Se llama draw()
- THEN Se dibuja un círculo central de tamaño (currentSize * 0.6)
- AND CÍRCULOS SUPERPUESTOS con alpha decreciente para crear efecto halo
- ANDColor usa additive blending para glow

### Requirement: Animación de breathing

El sistema DEBE aplicar oscilación de tamaño basada en breathingPhase para crear efecto de "respiración".

#### Scenario: Tamaño oscila suavemente

- GIVEN Un BloomUnit con breathingPhase = 1.5, baseSize = 50
- WHEN El tiempo avanza y se llama update()
- THEN currentSize = baseSize * (1.0 + 0.2 * sin(time * breathingPhase))
- AND La variación es suave (±20% del tamaño base)

### Requirement: Herencia de física de ondas

El sistema DEBE aplicar las mismas fuerzas físicas que PinturaFluorescenteScene:
- F_center: atracción al centro (parámetro k)
- F_wave: onda sinusoidal (parámetros A, w, q)
- F_swirl: rotación/tangente (parámetros S, ws, qs)
- F_damping: frenado (parámetro beta)

#### Scenario: Física de ondas se aplica

- GIVEN Un BloomUnit activo lejos del centro
- WHEN Se llama update() con física habilitada
- THEN F_center jala hacia el centro
- AND F_wave crea patrón de onda
- AND F_swirl agrega rotación
- AND F_damping frena movimiento

### Requirement: Parámetros de bloom

El sistema DEBE registrar los siguientes parámetros específicos de bloom:
- numBloomUnits: cantidad de unidades (300-900, default 500)
- minSize: tamaño mínimo (10-50, default 20)
- maxSize: tamaño máximo (50-150, default 80)
- noiseAmount: variación orgánica (0-1, default 0.3)
- mouseRadius: radio de influencia del mouse (50-300, default 150)
- mouseForce: fuerza del mouse (0-1, default 0.5)
- breathingSpeed: velocidad de respiración (0.5-3, default 1.5)
- breathingAmplitude: amplitud de respiración (0.1-0.5, default 0.2)

#### Scenario: Parámetros se registran en registry

- GIVEN FluorescenteBloomScene se activa
- WHEN Se llama registerParameters()
- THEN Los 9 parámetros de bloom aparecen en ParameterRegistry
- AND Cada parámetro tiene min, max, default, step válidos

### Requirement: Interacción de mouse con radio y falloff

El sistema DEBE aplicar fuerza repulsiva/atractiva a BloomUnits cerca del mouse basada en mouseRadius y mouseForce.

#### Scenario: Mouse empuja bloom units

- GIVEN Varios BloomUnits activos
- WHEN El mouse se mueve a posición (x, y) con mouseRadius=100, mouseForce=0.5
- THEN Cada BloomUnit a distancia < 100 recibe fuerza proporcional
- AND La fuerza disminuye con distancia (falloff lineal o cuadrático)

## MODIFIED Requirements

### Requirement: Herencia de SceneParams

(Previos: PinturaFluorescenteScene usa struct SceneParams para k, A, w, q, beta, S, ws, qs, flockSpeed, etc.)

Los parámetros físicos de SceneParams DEBEN ser heredados y utilizados. Los valores por defecto PUEDEN ser ajustados para comportamiento más elástico (beta más alto = más amortiguación).

#### Scenario: Parámetros físicos se aplican

- GIVEN SceneParams con k=0.3, beta=0.98
- WHEN Se actualiza un BloomUnit
- THEN La atracción al centro usa k=0.3
- AND El damping usa beta=0.98 (más elástico que default 0.95)

## REMOVED Requirements

### Requirement: Sistema de Trail Particles

(Reason: El efecto de glow/halo interno del BloomUnit elimina la necesidad de partículas trail separadas)

El sistema NO DEBE crear partículas trail separadas de los BloomUnits.

### Requirement: Flock Mode

(Reason: Priorizado fuera del scope inicial)

El sistema NO DEBE implementar comportamiento de bandada (flock behavior) en la primera iteración.

## Success Criteria

- [ ] Pool de 300-900 BloomUnits compila y funciona
- [ ] Cada BloomUnit tiene todos los campos requeridos
- [ ] Renderizado muestra formas suaves con halo
- [ ] Breathing animation es visible
- [ ] Física de ondas aplica correctamente
- [ ] 9 parámetros de bloom registrados
- [ ] Interacción de mouse funciona con radio/falloff