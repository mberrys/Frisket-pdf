# Regression corpus

Pinned inputs that reproduce previously-fixed fuzz findings. `fuzz.yml` and
`scripts/fuzz-bughunt.sh` pass this directory to every harness so a fix can't
silently regress.

## MIC-326 / R-003 (JBIG2, `fuzz_images`)

| File | Finding | Root cause | Fix |
|------|---------|------------|-----|
| `jbig2_composition_timeout_c8a61daa.bin` | libFuzzer timeout (>1200s), later re-confirmed at >60s locally | `PDFJBIG2Decoder::processSymbolDictionary`'s height-class loop (`pdfjbig2decoder.cpp`) makes zero progress when a height class's first delta-width decodes as out-of-band: `NSYMSDECODED` stays unchanged, the outer `while` condition is still true, and the loop spins forever on the arithmetic decoder without ever allocating a bitmap. Invisible to the pre-existing allocation budget, which only charges on allocation. | Added `PDFJBIG2Decoder::accountDecodeWork(items, pixels)`, charged once per symbol-dictionary height class (and per decoded bitmap, symbol instance, halftone grid cell) against a tight, dedicated item budget (`JBIG2_MAX_TOTAL_DECODE_WORK_ITEMS`), independent of pixel count. |
| `jbig2_symbol_dict_zero_progress_timeout_7cf9387b.bin` | libFuzzer timeout (>30s), found by local fuzzing while verifying the fix above | `PDFJBIG2Decoder::readBitmap` decodes a legal-size generic-region bitmap (bounded by the allocation budget) but arithmetic-decoding it pixel-by-pixel under ASan/UBSan takes far longer than allocating it — allocation was bounded, wall-clock time was not. | Same `accountDecodeWork` call now also charges decoded/composited *pixels* against a second, independent budget (`JBIG2_MAX_TOTAL_DECODE_WORK_PIXELS`) sized generously (4x the allocation cap) so it never rejects a legitimately large scan, while the item budget above is what actually bounds degenerate, near-zero-pixel loops. |

Both seeds now execute in well under 100ms (was: 1200s+ and 30s+
respectively). See `docs/V1_RELEASE_READINESS.md` §R-003 for the full
investigation writeup, including the corrected diagnosis after the first
committed fix (`8f174230`, which charged only composited pixels with a
1-pixel floor) turned out not to reach either of these code paths.

## MIC-326 / R-003 (Huffman, `fuzz_images`)

`jbig2_huffman_signed_overflow.bin` — UBSan signed-integer overflow in
`PDFJBIG2HuffmanDecoder::readSignedInteger`, fixed by `8f174230`
(range-checked 64-bit arithmetic via `checkHuffmanRange`). Not present in
this corpus directory; the original CI run only uploaded the timeout
artifact. Re-add it here if/when the original seed is recovered.
