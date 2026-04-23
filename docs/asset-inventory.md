# RPSopwith Asset Inventory

Reference for all graphical assets needed, their dimensions, frame counts, and
planned layer/sprite-sheet assignment. Update this file before allocating new
XRAM regions.

Source reference: SDL Sopwith uses `SYM_WDTH 16` / `SYM_HGHT 16` as the base
symbol size (see `sw.h`). All plane sprites are 16×16. Ground targets are also
drawn at 16×16 or composed from multiple 16×16 cells.

---

## Layer Architecture

| Layer | VGA Plane | Fill Mode        | Fill BPP | Fill Notes                        | Sprite Mode | Sprite Size | Sprite BPP |
|-------|-----------|------------------|----------|-----------------------------------|-------------|-------------|------------|
| 0     | 0         | Mode-2 tile      | 1 or 2   | Sky, clouds, horizon              | Mode-5      | 8×8         | 4          |
| 1     | 1         | Mode-2 tile      | 4        | Terrain, runway, baked buildings  | Mode-5      | 16×16       | 4          |
| 2     | 2         | Mode-1 character | built-in | HUD score/supplies (built-in font)| Mode-5      | 16×16       | 4          |

Layer 0 renders behind Layer 1 which renders behind Layer 2.

**Note on enemy planes:** Mode-5 sprites each carry their own `palette_ptr` in
their config entry. Player and enemy planes share the same Layer 2 sprite strip
and layer; each sprite independently selects its 16-color palette. No separate
layer needed for enemies.

---

## Sprite Assets

### Layer 2 — Planes (16×16, 4bpp)

| Asset               | Frames | Notes                                              |
|---------------------|--------|----------------------------------------------------|
| Player plane        | 32     | 16 angles × 2 orientations (right/left)            |
| Enemy plane 1       | 32     | Same strip as player, different `palette_ptr`      |
| Enemy plane 2       | 32     | Same strip as player, different `palette_ptr`      |
| Plane hit/falling   | 8      | `symbol_plane_hit[4]` — spinning falling plane     |
| Plane win           | 8      | `symbol_plane_win[4]` — victory fly-off animation  |

Total unique frame data on Layer 2: ~40 frames × 128 bytes = ~5 KB.
Enemy planes reuse player frame data (same XRAM ptr, different palette ptr).

### Layer 1 — Ground entities (16×16, 4bpp)

All targets in SDL Sopwith are **stationary**. `movetarg()` / `moveox()` never
update `ob_x`/`ob_y`. Candidates for tilemap baking (see Tilemap section).

| Asset             | Standing | Destroyed | Notes                                    |
|-------------------|----------|-----------|------------------------------------------|
| Cow / ox          | 1        | 1         | `symbol_ox[2]` — standing + dead        |
| Powerup: ammo     | 1        | 1         | `symbol_powerups` + collected variant   |
| Powerup: bomb     | 1        | 1         | Same as above                            |
| Powerup: fuel     | 1        | 1         | Same as above                            |
| Balloon           | 3×2      | —         | Floats, 3 drift orientations × 2 frames |

Powerups appear/disappear dynamically — sprites preferred over tiles.
Balloon drifts in air — sprite needed.

### Layer 0 — Small flying objects (8×8, 4bpp)

| Asset             | Frames | Notes                                                 |
|-------------------|--------|-------------------------------------------------------|
| Individual bird   | 2      | `symbol_bird[2]` — wing-up, wing-down                |
| Flock             | 2      | `symbol_flock[2]` — flock wing-up, wing-down         |
| Bird splat        | 1      | `symbol_birdsplat` — bird hit by plane                |
| Bullet / shot     | 1      | `symbol_pixel` — single lit pixel; 8×8 with 1 opaque |
| Smoke             | 1      | `symbol_pixel` — same single pixel in grey/black     |
| Bomb              | 4      | `symbol_bomb[2]` × 2 transform angles — tumbling     |
| Missile           | 8      | `symbol_missile[4]` × 2 — 8 rotations               |
| Starburst         | 2      | `symbol_burst[2]` — flare expanding/contracting      |
| Explosion debris  | 8      | `symbol_debris[8]` — 8 debris object types          |

Bombs and missiles fly through the air but land on terrain — **Layer 0 renders
behind Layer 1 terrain tiles**, so a bomb skimming low could clip behind a hill
before detonating. Acceptable visual tradeoff; collision uses world coords not
screen layers. If this looks wrong in practice, move to Layer 1 or 2 sprites.

---

## Tilemap Assets — Layer 1 (terrain + baked buildings)

Since all targets are stationary, buildings can be baked into the Layer 1
world tilemap at startup. On destruction, the game rewrites the tilemap cells
for that building (replacing standing tiles with destroyed tile IDs).

### Terrain tiles (current)

Estimated ~40–60 unique profile tiles covering:
- Sky edge transitions (jagged, smooth, various slopes)
- Solid fill (underground)
- Runway surface
- Crater shapes (added dynamically to tilemap on bomb impact)

### Building tiles (to add)

Each building needs tiles for standing and destroyed states. Tiles can be
shared between buildings (e.g., a common "rubble" tile for all destroyed states,
common wall/roof texture tiles).

| Building        | Size (tiles) | Standing tiles | Destroyed tiles | Notes                  |
|-----------------|-------------|----------------|-----------------|------------------------|
| Hangar          | 4 × 3       | ~12            | ~8              | Largest structure      |
| Factory         | 3 × 2       | ~8             | ~6              |                        |
| Oil tank        | 2 × 2       | ~4             | ~4              | Round top              |
| Tank            | 2 × 1       | ~2             | ~2              | Stationary in all levels |
| Truck           | 2 × 1       | ~2             | ~2              |                        |
| Tanker truck    | 3 × 1       | ~3             | ~2              |                        |
| Flag            | 1 × 2       | ~2             | ~1              | Could be sprite        |
| Tent            | 2 × 1       | ~2             | ~1              |                        |
| Radio tower     | 1 × 4       | ~4             | ~3              | Tall, narrow           |
| Water tower     | 2 × 3       | ~6             | ~4              | On legs                |
| Cow / ox        | 2 × 1       | ~2             | ~2              | OR keep as sprite      |

**Tile budget estimate:**
- Terrain: ~50 tiles
- Buildings (standing): ~47 tiles
- Buildings (destroyed): ~35 tiles (many shared rubble tiles)
- Total: ~132 / 256 tiles

This leaves ~124 tiles of headroom. Sharing rubble/wall/roof tile IDs across
building types will help. Track actual usage once artwork is drawn.

### Layer 0 tileset (sky, 1–2bpp)

Very few tiles needed:
- Sky gradient bands: 2–4 tiles
- Cloud patch variants: 4–8 tiles
- Empty transparent tile: 1 tile (tile ID 0)

Total Layer 0 tile budget: ~16 tiles. The tileset itself at 2bpp costs only
256 bytes (16 tiles × 16 bytes each).

---

## Asset Gaps (things not in original SDL Sopwith to consider)

| Item              | Notes                                                          |
|-------------------|----------------------------------------------------------------|
| Runway markings   | Bake into Layer 1 tilemap as painted tile strip at startup    |
| Explosion flash   | A bright 8×8 tile frame before debris scatters                |
| Score digits      | Mode-1 built-in font handles this; no custom tiles needed     |

---

## Missing from initial asset list (vs SDL Sopwith source)

The SDL Sopwith source reveals these additional objects not in the original list:

| Asset           | Type       | Where defined      |
|-----------------|------------|--------------------|
| Bomb            | Projectile | `symbol_bomb[2]`   |
| Missile         | Projectile | `symbol_missile[4]`|
| Starburst/flare | Projectile | `symbol_burst[2]`  |
| Smoke trail     | Effect     | `symbol_pixel`     |
| Bird splat      | Effect     | `symbol_birdsplat` |
| Balloon         | Target     | `symbol_balloon[6]`|
| Powerups        | Pickup     | `symbol_powerups[NUM_POWERUP_TYPES]` |
| Plane win anim  | Plane      | `symbol_plane_win[4]` |
| Plane hit anim  | Plane      | `symbol_plane_hit[4]` |
| Medals/ribbons  | HUD        | `symbol_medal[3]`, `symbol_ribbon[6]` (end-screen only) |

---

## XRAM Budget (to be allocated in constants.h)

Current video XRAM end: `0x3B20`. OPL2 reserved at `0xFE00`. Available: ~49 KB.

| New region                      | Est. size |
|---------------------------------|-----------|
| Layer 0 Mode-2 config + tilemap | ~300 B    |
| Layer 0 tileset (2bpp, 16 tiles)| ~256 B    |
| Layer 0 palette (2bpp = 4 colors)| 8 B      |
| Layer 0 Mode-5 config (32 sprites × 8 B) | 256 B |
| Layer 0 sprite data (8×8, 4bpp, 32 frames) | ~1 KB |
| Layer 0 sprite palette          | 32 B      |
| Layer 1 Mode-5 config (32 sprites × 8 B) | 256 B |
| Layer 1 sprite data (16×16, 4bpp, 16 frames) | ~2 KB |
| Layer 1 sprite palette(s)       | ~128 B    |
| Layer 2 Mode-1 HUD config       | ~32 B     |
| Layer 2 Mode-5 config (16 sprites × 8 B) | 128 B |
| Layer 2 sprite data (16×16, 4bpp, 40 frames) | ~5 KB |
| Layer 2 sprite palettes (player + 2 enemy) | 96 B |
| **Total new**                   | **~10 KB** |

~39 KB remains for future content. All exact addresses must be added to
`constants.h` with `_Static_assert` overlap checks before any C code references
them.
