# Proposal: Refactor Particle System

## Intent

Eliminar los principales anti-patterns y problemas de calidad en el código actual:
- Variables globales sin encapsulamiento
- Memory leaks potenciales por uso de `new` sin ownership
- Inefficient particle removal (O(n) en cada muerte)
- Magic numbers dispersos sin configuración

## Scope

### In Scope
- [ ] Encapsular variables globales en la clase ofApp
- [ ] Implementar particle pool para evitar `new`/`delete`
- [ ] Agregar límite máximo de partículas
- [ ] Implementar removal eficiente (swap-and-pop)
- [ ] Extraer magic numbers a constantes con nombres claros
- [ ] Limpiar métodos vacíos no usados

### Out of Scope
- UI del panel de parámetros (mantener como está)
- Agregar sistema de testing (no hay infraestructura en OF)
- Refactor de la física de partículas
- Agregar más features

## Approach

Refactor incremental, archivo por archivo:

1. **Fase 1**: Mover variables globales a ofApp class
2. **Fase 2**: Implementar ParticlePool con pre-allocated particles
3. **Fase 3**: Reemplazar erase() por swap-and-pop
4. **Fase 4**: Definir constantes para todos los magic numbers
5. **Fase 5**: Limpiar métodos vacíos

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `src/ofApp.h` | Modified | Agregar ParticlePool, constantes, mover variables globales |
| `src/ofApp.cpp` | Modified | Refactorizar lógica de creación/destrucción de partículas |
| `src/main.cpp` | None | Sin cambios |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| Introducir bugs en física | Low | Build exitoso + test visual manual |
| Romper compilation | Low | Verificar con `make` después de cada cambio |

## Rollback Plan

```bash
git revert HEAD
make
```

Cada commit será atómico, permitiendo revert selectivo si hay problemas.

## Dependencies

- Ninguna dependencia externa
- OpenFrameworks 0.12.1 ya está configurado

## Success Criteria

- [ ] `make` compila sin errores
- [ ] App funciona visualmente igual que antes
- [ ] No más variables globales en namespace global
- [ ] Particle limit implementado (~5000 max)
- [ ] Memoria bajo control (no growth infinito)
