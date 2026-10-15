# Proposal: Add Click Attractor Point

## Intent

Agregar un segundo punto de atracción controlado por click del mouse. Al hacer click, se crea un punto de atracción temporal que jala suavemente las partículas hacia esa posición. Esto agrega una nueva forma de interacción controlada por el usuario.

## Scope

### In Scope
- [ ] Agregar `attractorPoint` y `attractorActive` en ofApp
- [ ] Modificar mousePressed para setear attractor
- [ ] Modificar mouseReleased para limpiar attractor
- [ ] Agregar fuerza de atracción en Particle::update()

### Out of Scope
- Múltiples puntos de atracción simultáneos
- Guardar/recuperar posiciones de attractor
- UI visual del punto de atracción

## Approach

1. Agregar miembros privados en ofApp: `glm::vec2 attractorPoint`, `bool attractorActive`
2. En mousePressed: setear attractorPoint = mousePos, attractorActive = true
3. En mouseReleased: attractorActive = false
4. En Particle::update(): agregar fuerza adicional hacia attractor si active

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/ofApp.h` | Modified | Agregar attractor state |
| `src/ofApp.cpp` | Modified | Handler de mouse y physics |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Partículas se agrupan muy cerca del punto | Low | Usar fuerza suave (k pequeño) |
| Conflicto con click actual (limpia pantalla) | Low | Mantener ambos comportamientos |

## Rollback Plan

```bash
git revert HEAD
make && make RunRelease
```

## Dependencies

- Ninguna dependencia externa

## Success Criteria

- [ ] Al hacer click, partículas se dirigen hacia el mouse
- [ ] Al soltar click, partículas vuelven al comportamiento normal
- [ ] `make` compila sin errores
- [ ] Comportamiento original preservado (click limpa pantalla)
