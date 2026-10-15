# Design: refactor-particle-system

## Technical Approach

Refactor incremental del sistema de partículas manteniendo la misma física y comportamiento visual. Se introduce ParticlePool pre-allocated, se encapsula el estado en ofApp, y se limpian los métodos vacíos.

## Architecture Decisions

### Decision: Particle Pool Implementation

**Choice**: Pre-allocated array with active flag instead of vector of pointers
**Alternatives considered**: 
- std::vector<Particle*> with new/delete (current - rejected: memory leaks)
- std::vector<Particle> (rejected: no recycling, large moves)
- std::unique_ptr (rejected: overhead, still allocates)

**Rationale**: Array pre-allocated is fastest for iteration. Active flag allows O(1) toggle between active/inactive without allocation/deallocation.

### Decision: Efficient Removal Strategy

**Choice**: Swap-and-pop with active count tracking
**Alternatives considered**:
- erase() in loop (current - rejected: O(n) per removal)
- std::list (rejected: bad cache locality, overhead)
- Mark-and-sweep (rejected: complex, fragmentation)

**Rationale**: Swap last element with dead element, then pop. O(1) and maintains cache locality for iteration.

### Decision: Constants Organization

**Choice**: Static const members in ofApp class
**Alternatives considered**:
- Enum class (rejected: no, needs float values)
- #define macros (rejected: no type safety)
- Global const (rejected: same problem as globals)

**Rationale**: Grouped with the class that uses them, provides type safety, clear naming.

### Decision: Global Variables Migration

**Choice**: Move all global state to private members of ofApp
**Alternatives considered**:
- Singleton (rejected: unnecessary complexity)
- Static members (rejected: no advantage here)

**Rationale**: Standard OOP encapsulation. ofApp is the application state container.

## Data Flow

```
Mouse Input → ofApp::update() → ParticlePool::acquire() → Particle initialized
                                           ↓
                                    Particle updated (per-frame)
                                           ↓
                                    Dead check → ParticlePool::release()
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `src/ofApp.h` | Modify | Add ParticlePool class, constants, move globals to private |
| `src/ofApp.cpp` | Modify | Implement pool usage, efficient removal, remove empty callbacks |
| `src/main.cpp` | None | No changes |

## Interfaces / Contracts

### ParticlePool Class (new)

```cpp
class ParticlePool {
    static constexpr size_t MAX_PARTICLES = 5000;
    
    Particle* acquire(glm::vec2 pos, glm::vec2 vel, ofFloatColor col, float size);
    void release(Particle* p);
    Particle* getParticles();  // Raw array access
    size_t getActiveCount() const;
    void clear();              // Release all
};
```

### Constants (new in ofApp)

```cpp
class ofApp {
    // Spawn configuration
    static constexpr float SPAWN_OFFSET_RANGE = 15.0f;
    static constexpr float SPAWN_SIZE_MIN = 2.0f;
    static constexpr float SPAWN_SIZE_MAX = 8.0f;
    static constexpr int MIN_PARTICLES_PER_FRAME = 2;
    static constexpr int MAX_PARTICLES_PER_FRAME = 6;
    
    // Physics configuration
    static constexpr float PARTICLE_MAX_LIFE = 400.0f;
    static constexpr float TIME_SCALE = 30.0f;
    static constexpr float MOUSE_SPEED_THRESHOLD = 50.0f;
    
    // Limits
    static constexpr size_t MAX_PARTICLES = 5000;
};
```

## Testing Strategy

| Layer | What to Test | Approach |
|-------|-------------|----------|
| Build | `make` compiles | Verify no errors |
| Visual | App runs correctly | Manual test - particles behave same |
| Memory | No leaks | Monitor in Activity Monitor |

**Note**: OpenFrameworks no tiene framework de testing. Testing es manual visual + build verification.

## Migration / Rollout

No migration required. Changes are internal refactor with same external behavior.

## Open Questions

- [ ] None - diseño basado en specs claras
