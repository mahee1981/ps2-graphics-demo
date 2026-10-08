# Texture Streaming and Packet Organisation

Two problems, treated separately because they have separate solutions:

1. How a texture gets from main RAM into VRAM, and how we know when that VRAM can
   be reused.
2. How to order and group rendering packets so they only ever reference textures
   that are resident at that moment.

The allocator choice falls out of the answer to the second question, so it comes
last.

## 0. Terminology

**Pass.** A maximal group of draws whose textures are all resident in one half of
the texture area at the same time. A frame is an ordered sequence of passes. Each
pass consists of a texture set, the draws that reference it, and ends with a fence
and a swap of halves.

This is not a "pass" in the shading sense — there is no shadow pass or lighting
pass here. It is purely a VRAM residency window: the unit of work during which the
resident texture set does not change. A frame whose textures all fit at once is a
single pass.

**Half.** One of the two equal regions the texture area is divided into, so that
uploads into one can overlap drawing from the other. 94208 words, or 368 KB, each.

**Resident set.** The textures present in the half that the current pass draws
from.

**Upload.** One host-to-local transfer carrying one texture into VRAM.

**Fence.** A wait proving the GS has finished reading a region, implemented with
the GS FINISH signal. Distinct from waiting on a DMA channel, which only proves a
transfer was delivered.

## 1. Budget

VRAM is 4 MB: 1048576 words, where one word is 32 bits, or one PSM_32 pixel. At
640x448 double buffered with a 24-bit Z-buffer:

| buffer | words | KB |
|---|---|---|
| framebuffer 0, page aligned | 286720 | 1120 |
| framebuffer 1 | 286720 | 1120 |
| Z-buffer | 286720 | 1120 |
| **left for textures** | **188416** | **736** |

| texture | words | KB | fit in 736 KB | fit in a 368 KB half |
|---|---|---|---|---|
| 256x256, 32 or 24 bit | 65536 | 256 | 2 | 1 |
| 256x256, 16 bit | 32768 | 128 | 5 | 2 |
| 256x256, 8 bit + palette | 16640 | 65 | 11 | 5 |
| 128x128, 32 or 24 bit | 16384 | 64 | 11 | 5 |
| 128x128, 16 bit | 8192 | 32 | 23 | 11 |
| 128x128, 8 bit + palette | 4352 | 17 | 43 | 21 |
| 64x64, 32 bit | 4096 | 16 | 46 | 23 |

Palette figures include a 16x16 32-bit CLUT at 256 words, allocated alongside the
texture so the two share a lifetime.

The last column matters because the texture area gets split in two halves so that
uploads and drawing can overlap (section 2.3). Each half is 94208 words, or
368 KB.

At 32-bit, a 256x256 texture nearly fills a half on its own, which forces one pass
per texture and the maximum number of fences. Either halving the dimensions or
going paletted fixes that, and the table shows they are interchangeable as far as
VRAM is concerned: 128x128 at 32-bit and 256x256 at 8-bit both give five per half.
The choice between them is about which artifact is preferable — lower spatial
resolution at full colour, or full resolution capped at 256 colours. With nearest
filtering on magnification, a 128x128 texture stretched over a large on-screen
object will look blocky, which argues for paletted when geometry is close to the
camera.

Combining both — 128x128 paletted at 17 KB — puts 21 textures in a half, which for
most scenes means a single pass and no streaming at all.

Main RAM is 32 MB, so all of a scene's textures fit there comfortably. VRAM is the
scarce resource and the GIF bus is not, which is why streaming is the normal path
rather than a fallback.

Three sizing rules that the rest of the document depends on:

- Buffer width is rounded up to 64 pixels, 128 for 8- and 4-bit formats. A 32x32
  texture costs the same as 64x32.
- 24-bit textures occupy one word per pixel, exactly like 32-bit. The saving is on
  the bus (3 bytes per pixel instead of 4), not in VRAM.
- Texture dimensions are effectively powers of two, because TEX0 stores log2 of
  width and height. Footprints therefore come in a small number of discrete sizes
  — 4096, 16384, 65536 words and so on — which matters when choosing an allocator.
  Paletted textures break the neatness slightly, since a CLUT adds 256 words on
  top: 4352 and 16640 rather than 4096 and 16384.

## 2. Streaming a texture to VRAM

### 2.1 What one transfer consists of

A host-to-local transfer is four register writes followed by the pixel data in
GIF image mode:

| register | contents |
|---|---|
| `BITBLTBUF` | destination address in VRAM, destination buffer width, destination PSM |
| `TRXPOS` | source and destination x/y, transfer direction within the buffer |
| `TRXREG` | width and height in pixels |
| `TRXDIR` | 0 for host to local, which starts the transfer |

The pixel payload follows as image-mode GIF data. A single GIF tag carries at most
0x7FFF qwords, so anything larger is split across several tags. A 256x256 24-bit
texture is 12288 qwords and fits in one; a 1024x1024 32-bit texture would need
nine.

After the payload, write `TEXFLUSH`. The GS caches texels, and without the flush a
draw can sample stale data from the region that was just overwritten.

The payload size follows bits-per-pixel, not the VRAM footprint. 24-bit is three
bytes per pixel on the bus and four in VRAM; getting this wrong reads past the end
of the source buffer.

### 2.2 The RAM side

Textures are decoded once at load into the exact format the GS will sample and
kept in RAM for the lifetime of the scene. Nothing is converted mid-frame.

Requirements:

- **Final PSM layout.** The bytes handed to the DMA are already what the GS
  expects.
- **Alignment.** DMA references source data in 16-byte units; align to 64 so a
  transfer never shares a cache line with unrelated data.
- **Cache flush before transfer.** The DMA controller reads physical memory and
  does not see the EE data cache. Either send with the flush flag set, or flush the
  pixel range explicitly.
- **One arena, not one allocation per texture.** A load-time bump allocator over
  a single block keeps the heap intact and makes alignment structural rather than
  per-allocation. Nothing is individually freed in a static scene.

Because the RAM copy never moves, a texture is a stable descriptor: pixel pointer,
dimensions, PSM, VRAM footprint, payload size. Its VRAM address is the only field
that changes, and only within a frame.

### 2.3 Path assignment: PATH3 for textures, PATH1 for geometry

Textures go to the GS on PATH3, the GIF DMA channel. Geometry goes on PATH1, via
VU1 and XGKICK. The reason is throughput: the two paths are scheduled
independently, so a texture transfer can be in flight while VU1 is transforming
and kicking primitives. Serialising them — upload, wait, draw, wait — leaves one
engine idle while the other works, and on this machine the bus is the resource we
have in surplus. Using both at once is the whole point.

This rules out the alternative of putting uploads into the same ordered stream as
the drawing, which is what gsKit does in its inline mode. gsKit can do that
because it pushes primitives through PATH3 as well, so its uploads and draws are
already one chain. Here, matching that would mean routing texture data through VU1:
into its 16 KB of data memory and back out via XGKICK, chopping a 256 KB texture
into dozens of fragments, through the same unit that should be processing
vertices. It would also collapse the parallelism we just chose.

Two consequences follow from running both paths concurrently.

**An upload must never target VRAM that is currently being sampled.** This is not
something a fence can fix after the fact — it is a layout requirement. The texture
area is therefore split in two halves: draws for the current pass sample half A
while uploads for the next pass fill half B. The two never overlap, so no
synchronisation is needed *during* a pass at all.

**A fence is still needed at the swap**, to confirm the GS has finished with the
half about to be refilled. But by then those draws are a full pass old, so the
FINISH has usually already been signalled and the wait returns immediately. The
stall exists, it just stops being on the critical path.

Note what this means for ordering within a pass: there is none to rely on. VU1 runs
asynchronously — its VIF1 transfer completing means the data and start command
were delivered, not that the microprogram has finished emitting primitives. The
design does not depend on ordering because the two halves are disjoint.

Also worth knowing: PATH1 has priority over PATH3 at GIF arbitration, and PATH3
is interruptible between packets. So uploads progress in whatever slack VU1
leaves rather than competing with it. That is the desired behaviour — geometry
never waits on a texture transfer — but it does mean upload throughput is whatever
bandwidth the geometry path leaves spare, not the full bus.

### 2.4 Knowing when VRAM is safe to reuse

`dma_channel_wait` proves a transfer completed. It says nothing about whether the
GS has finished *drawing* with the data. Those are different moments, and
overwriting a region in between corrupts whatever is still being rasterised.

The fence is the GS FINISH signal:

1. Clear the FINISH bit in `CSR`.
2. Queue a FINISH register write after the last primitive to be waited on.
3. Spin until `CSR` reports FINISH.

Which path the FINISH goes out on matters. Sent through PATH3 it only orders
against other PATH3 traffic. Sent through PATH1 it queues behind everything VU1
has already kicked, which is what is actually needed when geometry goes through
VU1. tyra implements exactly this — a small VU1 program whose only job is to emit a
FINISH giftag — in `RendererCoreSync`, although nothing in the engine currently
calls it.

Cost is a full pipeline drain. That is why the strategies below are judged largely
on how few fences they need.

## 3. Organising packets around what is resident

### 3.1 The constraint

Every draw names a texture. A draw can only be submitted while its texture is in
VRAM. VRAM holds a subset of the frame's textures. So the renderer has to produce
an order, and a grouping of that order into passes within which the resident set
does not change.

Three properties of the workload decide which strategy fits:

- **Can draw order change?** Opaque geometry with a Z-buffer can be reordered
  freely. Transparent geometry cannot; it has to be drawn back to front.
- **Are texture sizes uniform?** Uniform sizes make slot-based schemes viable.
- **Does the whole working set fit?** If it does, most of this disappears.

### 3.2 Strategy A — texture-major batching

Sort draws by texture. Fill VRAM with as many textures as fit, emit every draw
that uses them, fence, reclaim everything, repeat until the frame is done.

- Uploads per frame: one per texture, which is the theoretical minimum, provided
  no texture straddles a pass boundary.
- Fences per frame: passes minus one.
- Requires reordering draws, so opaque only.
- Pass count is `ceil(total unique texture bytes / window size)` in the best case.

For a static scene the partition is frame-invariant — same draws, same textures,
same sizes — so it can be computed once at load and simply walked each frame.

Greedy construction is adequate:

```
sort draws by texture
pass = empty
for each draw:
    if draw.texture not resident in pass and does not fit:
        emit pass, start a new one
    add draw to pass
emit pass
```

Sorting matters more than the packing heuristic. Unsorted, a draw order that
alternates between two large textures degenerates to one pass per draw.

### 3.3 Strategy B — draw-order streaming with eviction

Keep the submission order the application asked for. Upload each texture when it
is first needed; when VRAM is full, evict something.

- Uploads per frame: unbounded. A texture used early and again late gets uploaded
  twice, or more.
- Fences: one per reclaim.
- Preserves draw order, so it handles transparency.
- Benefits from a smarter eviction choice than "drop everything", because keeping
  a small hot texture resident across an eviction is cheap and saves an upload.

This is what gsKit does, and roughly what tyra does — tyra reclaims the whole area
rather than choosing a victim.

### 3.4 Strategy C — fixed slots

Standardise texture dimensions so the texture area divides into interchangeable
slots. With 736 KB: eleven 64 KB slots, or a mix such as two 256 KB plus eight
64 KB.

- Uploads: same as whichever ordering strategy sits on top.
- Fences: potentially per slot rather than per pass. If each slot tracks which
  draws referenced it, a slot can be recycled as soon as those specific draws have
  completed, rather than draining the whole pipeline.
- Requires asset discipline: every texture is one of a few fixed sizes in a fixed
  PSM. This was normal practice on PS2.
- Removes fragmentation and variable-size fitting from the problem entirely.

The fine-grained recycling is the real advantage. Whole-area reclaim forces a full
drain; per-slot reclaim lets drawing continue while other slots are refilled.

### 3.5 Strategy D — split window

Not really an alternative strategy but a property of the base design, included
here for comparison. The texture area is halved so that PATH3 can fill one half
while PATH1 draws from the other, which is what makes concurrent use of the two
paths possible at all (section 2.3).

- Halves effective capacity, so roughly doubles the number of passes.
- Each boundary costs much less, because the fence has a full pass of slack to be
  satisfied in and normally returns immediately.
- The trade is pass count against stall cost. It only pays off if the textures are
  small enough that a half still holds a useful working set — which, per the table
  in section 1, means 16-bit or paletted at 256x256.

The degenerate case is worth naming: if one texture fills a whole half, every draw
becomes its own pass and the fence is hit constantly. At that point the split is
costing more than it saves and a single undivided window with serialised phases is
better. Texture format drives which side of that line the scene sits on.

### 3.6 Strategy E — make everything fit

Budget the scene so all textures are resident permanently: fewer textures, smaller
dimensions, 16-bit or paletted formats, or packed into atlases.

- Uploads per frame: zero.
- Fences: none.
- Entirely a content constraint, with no runtime machinery at all.

Worth stating plainly because it is the cheapest option when it is available. At
8-bit with palettes, eleven 256x256 textures fit in 736 KB. A scene that stays
under that does not need any of the strategies above.

### 3.7 Comparison

| | uploads/frame | fences/frame | draw order | needs |
|---|---|---|---|---|
| A texture-major | unique textures | passes − 1 | must be free to reorder | static-ish scene |
| B draw-order | unbounded | one per reclaim | preserved | eviction policy |
| C fixed slots | depends on ordering | one per slot recycle | either | uniform texture sizes |
| D split window | as A | passes − 1, usually non-blocking | as A | a half big enough for a working set |
| E fit everything | 0 | 0 | preserved | content budget |

These compose rather than compete. A realistic renderer uses E for as much as
possible, A for the opaque remainder, and B for transparent geometry that cannot be
reordered. C is an implementation choice underneath any of them.

## 4. Allocators

Each allocator is judged on what the strategies above actually ask of it: how
reclaim happens, whether individual lifetimes exist, and whether sizes vary.

### 4.1 Bump

A cursor that only moves forward. Allocation is a bounds check and an add; there is
no individual free, only a wholesale reset.

- Cost: O(1) allocate, O(1) reset.
- Fragmentation: impossible.
- Needs: all contents to die at the same moment.
- Pairs with: A, D, and load-time allocation for E.

### 4.2 Ring

A bump cursor that wraps to the start when it runs out, overwriting the oldest
allocations.

- Cost: O(1).
- Fragmentation: impossible.
- Needs: proof that the oldest allocations are dead before wrapping, i.e. a fence.
- Pairs with: A and B.

Worth being precise about the difference from a plain bump arena: a ring only earns
its extra complexity if allocations are reclaimed in the same order they were made
and you want to wrap without a hard boundary. With pass-based reclaim, where
everything dies together, the wrap never happens mid-pass and the ring degenerates
into a bump arena with a reset. For strategy A, bump is the honest description.

### 4.3 Stack

A bump cursor plus saved markers; frees must happen in reverse order of
allocation.

- Cost: O(1).
- Fragmentation: impossible.
- Needs: strictly nested lifetimes.
- Pairs with: nothing here directly, but it describes what tyra's `free(address)`
  actually is — a rewind, which is only correct if called in reverse order, which is
  why tyra iterates its allocation list backwards.

### 4.4 Free-list

Blocks covering the region, split on allocation and merged on free.

- Cost: O(n) allocate and free, plus a node allocation per split.
- Fragmentation: yes. Total free can be large while no single run is big enough.
- Needs: nothing; it is the general case.
- Pairs with: B, where individual eviction is the point.

This is the right structure only when lifetimes are genuinely independent. Under
strategy A it is pure overhead, because per-texture free is never used.

### 4.5 Pool and size-classed slabs

Fixed-size slots with a free list of indices. Several pools, one per size class, if
sizes vary in a small number of steps.

- Cost: O(1) both ways.
- Fragmentation: none internally; some waste when a texture is smaller than its
  class.
- Needs: sizes drawn from a small fixed set.
- Pairs with: C.

This fits the hardware better than it first appears. Texture dimensions are powers
of two, so footprints land on 4096, 16384, 65536 words and so on — natural size
classes. Three pools covering 64x64, 128x128 and 256x256 would handle most content
with O(1) allocation and no fitting logic whatsoever.

A pool tolerates the CLUT bump that a buddy allocator would not: a class can
simply be sized at 4352 or 16640 words to include the palette, since nothing here
requires the class size to be a power of two.

### 4.6 Buddy

A power-of-two tree: split a block in half repeatedly until it is the right size,
merge siblings on free.

- Cost: O(log n).
- Fragmentation: bounded, internal only, at most 2x on a bad fit.
- Needs: region size a power of two, ideally.
- Pairs with: C, or B with mixed sizes.

Attractive in principle because texture footprints are already power-of-two
multiples. The catch here is that 188416 words is not a power of two — the nearest
is 131072, which would waste 57344 words, or 224 KB of a 736 KB budget. Either
accept the waste, or run several trees over the region, at which point a
size-classed pool is simpler and does the same job.

### 4.7 Summary

| allocator | alloc | free | fragments | required lifetime pattern |
|---|---|---|---|---|
| bump | O(1) | all at once | no | uniform |
| ring | O(1) | implicit at wrap | no | FIFO, fenced |
| stack | O(1) | reverse order | no | nested |
| free-list | O(n) | any order | yes | independent |
| pool | O(1) | any order | no (some waste) | independent, fixed sizes |
| buddy | O(log n) | any order | bounded | independent, power-of-two sizes |

## 5. What to build here

The scene is static, opaque, and uses a handful of textures at 256x256 and below.
That points at strategy A — sort by texture, pass per fill — over two bump
regions, one per half of the texture area.

Concretely:

- Textures in RAM in final PSM layout, one load-time arena, 64-byte aligned.
- Texture area split in two halves, each a bump region, reset when it becomes the
  upload target.
- Draws sorted by texture and partitioned into passes once at load, since the
  scene is static. The partition must be known one pass ahead so uploads for pass
  N+1 can start during pass N.
- Per pass: issue uploads for the next pass into the idle half on PATH3, submit
  this pass's packets on PATH1, then at the boundary fence with FINISH through
  PATH1 and swap halves.
- Residency tracking kept across frames, so a frame whose working set already sits
  in the current half performs no uploads. An optimisation on top, not the
  mechanism.

Then, in order of expected value:

1. **Get textures into 8-bit with palettes, or at least 16-bit.** Per the table in
   section 1, a 368 KB half holds one 32-bit 256x256 texture but five paletted
   ones, or twenty-one at 128x128 paletted. This is the difference between one pass
   per draw and the whole frame fitting in a single pass, which makes it the
   highest-value change by a wide margin rather than a nice-to-have. Halving
   dimensions instead gets the same VRAM result, so the decision is a quality call
   rather than a budget one.
2. **Standardise texture dimensions and move to size-classed slots.** Once sizes
   come from a fixed set, individual slots can be recycled as their draws retire
   instead of reclaiming a whole half, which decouples reclaim granularity from
   the pass structure.
3. **Revisit the split** if measurement shows pass count dominating. If a half is
   too small to hold a useful working set, a single undivided window with
   serialised upload and draw phases costs fewer fences overall, at the price of
   giving up path concurrency.

What is deliberately not on the list: moving uploads into the geometry chain.
That would serialise both onto one path and give up the overlap that motivated the
PATH3/PATH1 split in the first place.

The free-list currently in the codebase has no role in any of this and should be
retired unless runtime asset loading appears.

## 6. What to measure

Numbers that decide whether any of the later steps are worth doing:

- Passes per frame. If it is 1, nothing after step 2 matters.
- Uploads per frame versus unique textures per frame. A gap means the sort is not
  doing its job.
- KB uploaded per frame, against available bus bandwidth.
- Time lost to fences, as a share of frame time. Near zero means the split is
  doing its job and uploads are hiding behind geometry. Significant means a half is
  too small for the working set, and the answer is smaller texture formats before
  anything structural.
- PATH3 idle time during a pass. If uploads finish long before the pass ends, the
  halves are larger than they need to be and the split could be uneven rather than
  50/50.
