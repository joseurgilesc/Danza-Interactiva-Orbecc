# Exploration: enjambre-organico — Single Cohesive Swarm Flowing Through Kinect Joints

## Request

Build a NEW scene from scratch, inspired by PinturaFluorescenteScene's logic, with three key differences:
1. Particles MUCH SLOWER.
2. Organic, flowing movement between Kinect joints.
3. A SINGLE cohesive particle group (swarm) that travels smoothly through the body, visiting joint after joint — NOT one particle group per interaction point.

## Current State

### Scene architecture (contract)
- Scenes implement `IBaseScene` (`src/scenes/BaseScene.h`): lifecycle (`setup/update/draw`), activation (`activate/deactivate/pause/resume`), parameter management (`setParameter` overloads for `float`/`vec2`/`vec3`), metadata, beat/bar hooks, input handlers.
- Kinect/MediaPipe joints arrive via `setParameter(paramId, glm::vec2)` with **normalized 0–1 coordinates**; the scene stores them in a `std::map<std::string, InteractionPoint>`. `InteractionPoint::setTarget()` converts to pixels, computes velocity, and `update(dt)` applies confidence decay + inertia when no fresh data arrives (`src/scenes/InteractionPoint.h`).
- Scenes register float params via `ParameterRegistry::getGlobal()` with scene-scoped IDs (`"<sceneId>.<key>"`), registered on `activate()`, unregistered on `deactivate()`.
- Registration in `src/ofApp.cpp::setup()` (lines ~46–64): factory function → `sceneEngine.loadScene(id, scene)`. OF Makefile auto-globs `src/`, so a new scene needs **no build file changes**.

### PinturaFluorescenteScene (src/scenes/PinturaFluorescenteScene.h/.cpp)
- **Particle pool pattern**: pre-allocated `ParticlePool` (raw array, `acquire/release`, resize-able via registered `maxParticles` param, saturation warning). Dual pools: main + trail.
- **Rendering**: 5 fluorescent palettes (magenta/cyan/green/orange/yellow, 5 hex tones each + random variation), alpha blending (`ofEnableAlphaBlending`), dark background (10), `clearEachFrame_` toggle (clean vs. accumulate mode), filled circles sized by `size * (life/maxLife)`, trail particles at 0.7 alpha / 0.5 size.
- **Physics**: spring to center (`-k*r`) + radial wave (`A*sin`) + tangential swirl (`S*cos`) + damping. Per-particle `pointId` binds each particle to ONE interaction point (this is exactly the per-joint behavior the new scene must NOT have).
- **Flock mode**: full O(n²) boids (separation/alignment/cohesion, FLOCK_RADIUS=40) with log-blend transition — proof the codebase already paid the O(n²) price once; at 5000 capacity it relies on actual active counts staying low.
- Global palette read from `global.currentPalette` on activate. Debug markers toggle. Reads saved registry values on activate.

### Archived FlujoArticularScene (recovered via `git show 6152d83^:src/scenes/FlujoArticularScene.{h,cpp}`)
The closest prior art: particles flowing through a canonical 18-joint anatomical chain (`CANONICAL_JOINTS`: head → neck → left arm → right arm → spine → left leg → right leg). Key mechanics:
- `buildJointSequence()` each frame: canonical joints first (if active), then non-canonical extras (e.g. "mouse").
- Each particle independently targets a joint index; on arriving within `flowRadius` it advances to the next joint and emits a ripple.
- Steering: `velocity += dir * flowSpeed * approachIntensity * dt` with 0.98 damping; screen-center weak spring fallback when no joints.
- Final visual was **ripples only** — particles became invisible carriers.
- `activate()` deliberately does NOT clear `interactionPoints_` (joints accumulate while scene is loaded-but-inactive); `deactivate()` does clear them.
- Decay rate lowered to 0.08 (~12.5 s) to tolerate tracking dropouts.

### Lessons from FlujoArticular's 18-commit fix history (the pain map)
| Commit | Lesson |
|--------|--------|
| `e5f1875` | Non-canonical interaction points (mouse) must join the sequence — don't hard-filter to canonical joints. |
| `e3df953` | Do NOT clear interaction points in `activate()` — joint data accumulates while the scene is inactive. |
| `7a086cf` | Visual clutter: bubbles + trails + glow were all cut; one strong visual beats three weak ones. |
| `010632b` → `b827bae` | Kill+respawn causes visible "pop"; in-place reset also failed → fixed-duration lifecycle with fade-in/out curve was the fix. |
| `b66f568` → `bb89d42` | Added noise wander, then REVERTED it — bolting wander onto per-particle steering broke the design; framerate-independent steering (`* dt`) is mandatory. |
| `ffb2e9b`, `6482729` | **Self-bounce jitter**: a particle near its target with nothing to advance to oscillates. Fixes: advance cooldown + only advance when 2+ joints active. |
| `1da1efb`, `e5fc432` | Pacing was too fast for the dance aesthetic → slower rhythm, and timing params must be EXPOSED in the panel (rippleInterval, particleMaxLife, fadeInTime). |

**Root-cause synthesis**: almost every bug came from PER-PARTICLE target management — thousands of independent agents each deciding when to advance produce jitter, self-bounce, and uncontrollable pacing. That is precisely what the user's "single cohesive swarm" requirement eliminates if we centralize the decision.

### Existing ParticleSwarmScene (src/scenes/ParticleSwarmScene.h)
Different concept (mutual-spring particle cloud repelled from joints, 600-particle cap because of O(n²) springs). Not a base for this scene, but: (a) confirms ~600 is the practical O(n²) ceiling at 60 fps in this codebase, (b) its name argues for naming the new scene distinctly (e.g. `EnjambreOrganicoScene`, id `enjambreOrganico`).

## Affected Areas

- `src/scenes/EnjambreOrganicoScene.h` / `.cpp` — NEW scene (only new files).
- `src/ofApp.cpp` — add factory call + `sceneEngine.loadScene(...)` (~3 lines, pattern at lines 46–64).
- No Makefile/addons changes (src auto-globbed). No SceneEngine changes (factory pattern already generic).
- Reusable as-is (copy-in, no shared-code refactor needed): fluorescent palettes, parameter registration boilerplate, `InteractionPoint` map handling, `CANONICAL_JOINTS` ordering, ParticlePool pattern.

## Approaches

### 1. Leader-follower: parametric swarm anchor + spring-damper particles + per-particle noise (RECOMMENDED)
A single invisible **swarm anchor** (one entity, not a particle) travels the joint sequence as a state machine: `DWELLING(joint i)` → ease-out departure → `TRAVELING` (smoothstep interpolation along the segment, duration in SECONDS, e.g. 2–5 s per segment) → ease-in arrival → dwell. Particles never pick targets; each one follows the anchor with a critically-damped spring (`acc = stiffness*(anchor + offset - pos) - damping*vel`) plus a per-particle `ofNoise(phase, t*slowRate)` orbital offset for organic wobble. Anchor velocity is clamped so joint teleports (tracking glitches) can't yank the swarm.

- **Pros**:
  - Slowness is a FIRST-CLASS parameter (segment duration + dwell time) — exactly the user's #1 ask; impossible for emergent dynamics to speed it up.
  - Structurally immune to FlujoArticular's whole bug family: one traveler → no self-bounce, no per-particle advance cooldowns, no pacing fights.
  - O(n) — comfortably 1000–3000 particles at 60 fps.
  - Cohesion is guaranteed by construction (all springs share one anchor).
  - Noise offsets evolve smoothly → no respawn pop if particles are persistent (fixed swarm population, no kill/respawn at all).
- **Cons**:
  - Less emergent than true boids; if noise amplitude/frequency is poorly tuned it can read as "blob on a rail".
  - Organic feel depends on layered noise tuning (2 octaves: large slow drift + small fast shimmer) — needs iteration time.
- **Effort**: Low–Medium.

### 2. Boids-lite seeking a moving leader
Same anchor path-following, but particles also run separation/alignment/cohesion among themselves (code pattern already exists in PinturaFluorescente's flock mode) plus a seek-leader force.

- **Pros**: genuinely emergent swarm texture; alignment produces beautiful flow lines; most "alive" look.
- **Cons**: O(n²) → hard cap ~400–600 particles without a spatial hash grid (extra complexity); boid weight tuning is notoriously fiddly; seek+boids can orbit/oscillate around the anchor — the same class of instability FlujoArticular fought for 18 commits, now in 2D force space.
- **Effort**: Medium–High (spatial grid required for headroom).

### 3. Spline path + flow-field advection
Catmull-Rom spline through active joints; particles advected along a slowly-moving progress window with curl-noise perpendicular displacement.

- **Pros**: smoothest possible trajectories (curves between joints, not straight segments); gorgeous ribbon/trail potential.
- **Cons**: joints move every frame → continuous spline rebuild + particle parameter reprojection (jitter risk when joints drop in/out reshapes the whole curve); cohesive "group" requires an extra progress-window constraint; hardest to reason about and debug; most code.
- **Effort**: High.

## Recommendation

**Approach 1 (leader-follower)**, with an optional pinch of Approach 2: a cheap separation-only pass using a coarse spatial grid IF the swarm reads as a blob during tuning (defer; likely unnecessary because noise offsets de-overlap particles naturally).

Concrete shape of the scene:
- **Anchor state machine** with exposed params: `travelDuration` (s/segment, default ~3.0), `dwellDuration` (s, default ~1.5), `swarmRadius`, `noiseAmount`, `noiseSpeed`, `springStiffness`, `particleCount`. All registered like FlujoArticular did (lesson `e5fc432`: expose timing in the panel).
- **Joint path**: reuse `CANONICAL_JOINTS` ordering + non-canonical append (lessons `e5f1875`). Anchor targets next ACTIVE joint in cyclic canonical order; if the target joint drops mid-travel, retarget smoothly (clamped anchor velocity absorbs the jump).
- **Edge cases handled by design**: 1 active joint → anchor dwells there, swarm breathes in place via noise (no advance, no bounce). 0 active joints → anchor eases to screen center (FlujoArticular fallback). Fixed persistent particle population → zero respawn pops.
- **Rendering**: PinturaFluorescente's fluorescent palettes + dark background; particles as soft filled circles with size/alpha modulated by noise phase; optional short trails via per-particle ring buffer (FlujoArticular's `MAX_TRAIL_POINTS` pattern) or accumulate mode — pick ONE strong visual (lesson `7a086cf`).
- **Slow by construction**: anchor takes seconds per segment; particle springs are low-stiffness/high-damping; all speeds scale with `dt` (framerate-independent, lesson `b66f568`).
- **Activation semantics**: copy FlujoArticular's `activate()` (preserve interaction points) and decay rate 0.08.

## Risks

- **Organic-feel tuning is subjective**: spring + noise constants will need live iteration with the dancer/projector; mitigate by exposing all motion params in the registry panel from day one.
- **Joint teleports** (MediaPipe tracking glitches) could still cause visible anchor lurch — mitigated by anchor max-speed clamp; must be verified with real Kinect input.
- **Single-joint and zero-joint states** must be explicitly tested (FlujoArticular's worst bugs lived there).
- **Name collision risk**: `ParticleSwarmScene` already exists; new scene must use a distinct id (`enjambreOrganico`) to avoid registry/param collisions.
- **No automated tests in project** (`strict_tdd: false`, verification = `make` build) — correctness of motion is visual; plan for a debug-markers mode like PinturaFluorescente's.

## Ready for Proposal

Yes. The architecture is fully mapped, the recommended approach (leader-follower swarm anchor) directly neutralizes the documented failure modes of the prior attempt, and the scope is contained (2 new files + 3 lines in ofApp.cpp). The orchestrator can proceed to `sdd-propose` for change `enjambre-organico`.
