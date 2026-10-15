# Tasks: refactor-particle-system

## Phase 1: Foundation - Constants & Types

- [x] 1.1 Add static constexpr constants to `src/ofApp.h` (MAX_PARTICLES, PARTICLE_MAX_LIFE, SPAWN_OFFSET_RANGE, SPAWN_SIZE_MIN, SPAWN_SIZE_MAX, TIME_SCALE, MOUSE_SPEED_THRESHOLD, MIN/MAX_PARTICLES_PER_FRAME)
- [x] 1.2 Define ParticlePool class declaration in `src/ofApp.h`

## Phase 2: Core Implementation - ParticlePool

- [x] 2.1 Implement ParticlePool class in `src/ofApp.h` with:
  - Static array `Particle particles[MAX_PARTICLES]`
  - `bool active[MAX_PARTICLES]` flag array
  - `size_t activeCount` tracking
  - `acquire()` method
  - `release()` method
  - `getActiveCount()` method
  - `clear()` method
- [x] 2.2 Add `ParticlePool pool` member to ofApp class

## Phase 3: Core Implementation - State Migration

- [x] 3.1 Move global `palettes` to private `vector<vector<ofFloatColor>> palettes` in ofApp
- [x] 3.2 Move global `currentPalette` to private `int currentPalette` in ofApp
- [x] 3.3 Move global `clearEachFrame` to private member in ofApp
- [x] 3.4 Remove global variables from `src/ofApp.cpp` namespace
- [x] 3.5 Update `setupPalettes()` to be a method or lambda inside ofApp

## Phase 4: Implementation - Update Logic

- [x] 4.1 Refactor particle creation to use `pool.acquire()` instead of `new`
- [x] 4.2 Implement swap-and-pop removal in update loop (O(1) instead of O(n))
- [x] 4.3 Add particle limit check before acquiring
- [x] 4.4 Replace magic numbers with constants throughout

## Phase 5: Cleanup

- [x] 5.1 Remove empty callback declarations from `src/ofApp.h`:
  - keyReleased
  - mouseMoved
  - mouseDragged
  - mouseScrolled
  - mouseEntered
  - mouseExited
  - dragEvent
  - gotMessage
- [x] 5.2 Remove empty callback implementations from `src/ofApp.cpp`

## Phase 6: Verification

- [x] 6.1 Run `make` to verify compilation
- [x] 6.2 Run app and verify visual behavior unchanged
- [x] 6.3 Test particle limit by moving mouse rapidly
- [x] 6.4 Commit changes with conventional commit message

> **Phase 6 instrumentation note**: Press `D` (unused key in InputRouter/UI bindings) to toggle the on-screen debug overlay. Track `Main/Trail Pool` counts vs `MAX_PARTICLES`, the per-frame spawn counters, FPS, and the attractor state/coordinates while validating tasks 6.2 and 6.3.
