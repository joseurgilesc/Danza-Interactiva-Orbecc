# Delta for Particle System

## ADDED Requirements

### Requirement: ParticlePool - Pre-allocated Memory

The system SHALL pre-allocate a pool of Particle objects to avoid runtime `new`/`delete` operations.

The pool SHALL support:
- Fixed maximum capacity (default: 5000 particles)
- `acquire()` method that returns an available particle
- `release(particle*)` method that marks particle as available again

#### Scenario: Acquire particle from pool

- GIVEN ParticlePool with available particles
- WHEN `acquire()` is called
- THEN return pointer to initialized particle
- AND particle is marked as active

#### Scenario: Pool exhausted

- GIVEN ParticlePool at maximum capacity
- WHEN `acquire()` is called
- THEN return nullptr (no allocation)
- AND do not create new particle

---

### Requirement: Particle Limit

The system MUST enforce a maximum limit on active particles to prevent memory exhaustion.

- GIVEN application running
- WHEN particle count would exceed 5000
- THEN do not create new particles
- AND existing particles continue to update normally

---

### Requirement: Efficient Particle Removal

The system SHALL use O(1) removal strategy (swap-and-pop) instead of O(n) erase.

- GIVEN particle marked as dead
- WHEN removal is processed
- THEN swap dead particle with last active particle
- AND pop last element (O(1) operation)

---

### Requirement: Constants Configuration

The system MUST define all magic numbers as named constants with clear semantics.

Constants to define:
| Constant | Value | Description |
|----------|-------|-------------|
| MAX_PARTICLES | 5000 | Maximum active particles |
| PARTICLE_MAX_LIFE | 400f | Frames before death |
| SPAWN_OFFSET_RANGE | 15f | Random spawn offset |
| SPAWN_SIZE_MIN | 2f | Minimum particle size |
| SPAWN_SIZE_MAX | 8f | Maximum particle size |
| TIME_SCALE | 30.0f | Delta time multiplier |
| MOUSE_SPEED_THRESHOLD | 50f | Speed for particle spawn |
| MIN_PARTICLES_PER_FRAME | 2 | At mouse movement |
| MAX_PARTICLES_PER_FRAME | 6 | At high speed |

---

## MODIFIED Requirements

### Requirement: Global State Encapsulation

The system MUST NOT expose application state in global variables.

(Previously: `clearEachFrame`, `currentPalette`, `palettes` were global)

- GIVEN application initialization
- WHEN `ofApp::setup()` runs
- THEN all state SHALL be initialized within ofApp class
- AND no global variables shall be modified during runtime

---

## REMOVED Requirements

### Requirement: Unused Callback Methods

The following callback methods SHALL be removed as they are empty:

- `keyReleased(int key)` - unused
- `mouseMoved(int x, int y)` - unused
- `mouseDragged(int x, int y, int button)` - unused
- `mouseScrolled(int x, int y, float scrollX, float scrollY)` - unused
- `mouseEntered(int x, int y)` - unused
- `mouseExited(int x, int y)` - unused
- `dragEvent(ofDragInfo dragInfo)` - unused
- `gotMessage(ofMessage msg)` - unused

(Reason: Code clutter with no functionality)

---

## Verification Scenarios

### Build Verification

- GIVEN modified source files
- WHEN `make` command executed
- THEN compilation completes without errors
- AND binary is produced in bin/

### Runtime Verification

- GIVEN application launched
- WHEN user moves mouse rapidly
- THEN particles spawn up to limit (5000)
- AND framerate remains stable (no O(n) stutters)

### State Verification

- GIVEN application running
- WHEN checking for global variables
- THEN no Particle-related globals exist
- AND state is encapsulated in ofApp
