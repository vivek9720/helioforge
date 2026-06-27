# Helioforge

Helioforge is an original C++17 monorepo for parser, decoder, and state-replay fuzzing. It contains four related subprojects that model compact storage and message formats used by embedded services. The code has no external dependencies and is intended for private review, expansion, sanitizer testing, and Fenrir submission preparation.

## Subprojects

- `binpack`: binary container parser with magic/version header, metadata, section table, length-prefixed payloads, nested records, optional index order, checksums, and cross-section references.
- `minidb`: embedded record-store decoder with pages, slots, typed fields, varints, string table references, page chains, freelist metadata, and journal-style replay.
- `cfgscript`: configuration/script parser with assignments, nested blocks, quoted strings, escapes, comments, include records, arrays, maps, expressions, and symbol references.
- `streamcodec`: streaming frame decoder with fragmented input, metadata-only compression flags, rolling decoder state, message IDs, continuation frames, and multi-frame reconstruction.

## Input Formats

### binpack

Files begin with `BPK1`, a 16-bit version, 16-bit flags, 16-bit section count, and metadata pairs. Each section table entry stores `id`, `kind`, payload offset, payload length, checksum, and a short name. Kind `1` payloads contain nested records. Kind `2` payloads contain cross-section references. Flag bit `0` enables an index list that is validated against parsed section IDs.

### minidb

Files begin with `MDB1`, page size, page count, string table count, and journal record count. The string table stores varint IDs and length-prefixed strings. Pages contain a page ID, next-page link, freelist slots, and length-prefixed encoded records. Records use varint IDs and typed fields: null, zigzag integer, string, bytes, and string-table reference. After page-chain reconstruction, journal records are replayed into the visible record map.

### cfgscript

Scripts support `include "name" { key: value }` records and top-level assignments. Values include strings, integers, booleans, arrays, maps, bare identifiers, and `$symbol` or `$call(...)` expressions. Line comments use `#` or `//`. Includes are data records only; no filesystem access is performed.

### streamcodec

Frames start with `SC`, then flags, type, message ID, sequence number, metadata length, payload length, and state byte. Metadata is a sequence of short key/value pairs. Flags record compression metadata and fragmentation/continuation state. The decoder accepts partial input and reconstructs completed messages across calls.

## Build

```sh
cmake -S . -B build
cmake --build build
```

## Test

```sh
ctest --test-dir build --output-on-failure
```

## Fuzzing

With Clang/libFuzzer available:

```sh
cmake -S . -B build-fuzz -DHELIOFORGE_BUILD_FUZZERS=ON -DCMAKE_CXX_COMPILER=clang++
cmake --build build-fuzz
./build-fuzz/binpack_fuzzer fuzz/corpus/binpack_fuzzer -runs=1000
./build-fuzz/minidb_fuzzer fuzz/corpus/minidb_fuzzer -runs=1000
./build-fuzz/cfgscript_fuzzer fuzz/corpus/cfgscript_fuzzer -runs=1000
./build-fuzz/streamcodec_fuzzer fuzz/corpus/streamcodec_fuzzer -runs=1000
```

The harnesses accept raw libFuzzer bytes and call the real module code. They do not special-case PoC files or intentionally crash in harness code.

## Seed Corpus

Seeds live under `fuzz/corpus/<target>/`, which is a recognized ClusterFuzzLite seed location. Each target has valid or near-valid examples covering small, multi-record, nested, metadata-heavy, transaction, expression-heavy, fragmented, and continuation-oriented inputs. `fuzz/dictionary.txt` contains format magic values, keywords, delimiters, section terms, frame markers, and script tokens.

## ClusterFuzzLite

`.clusterfuzzlite/build.sh` compiles all four fuzz targets into `$OUT` using `$SRC` or the current repository root. It performs no network fetches, uses no credentials, and does not rely on absolute local paths. `.clusterfuzzlite/project.yaml` lists:

- `binpack_fuzzer`
- `minidb_fuzzer`
- `cfgscript_fuzzer`
- `streamcodec_fuzzer`

## Fenrir Readiness Checklist

- [x] Substantial C++17 project with multiple related modules.
- [x] Deterministic, non-interactive build.
- [x] No external dependencies or network access.
- [x] Four connected fuzzing harnesses.
- [x] Per-harness seed corpus in a recognized location.
- [x] Useful fuzz dictionary.
- [x] ClusterFuzzLite build script at repository root.
- [x] Fuzz targets listed in project YAML.
- [ ] Repository is private on GitHub.
- [ ] Repository is manually reviewed and primarily human-written before submission.
- [ ] Any actual discovered PoC is saved as exact raw input bytes under `pocs/`.

## Manual Review Checklist Before Submission

Before submitting to Fenrir, manually review:

- Originality of the code.
- Correctness of the parsers.
- Realistic complexity of parser and decoder state.
- Build reproducibility from a clean checkout.
- Sanitizer compatibility.
- Fuzz harness connectivity.
- Seed corpus quality.
- Absence of copied public code.
- Absence of fake or obvious intentional bugs.
- Repository privacy.
- Commit history and human-written quality.

The final GitHub repository must be private, original, and primarily human-written.
