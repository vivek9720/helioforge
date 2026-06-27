# Helioforge

Helioforge is an offline diagnostics toolkit for small embedded services that need to ship compact incident bundles from devices, labs, and field deployments. It provides C++17 libraries and a command-line inspector for reading device support packages, reconstructing local state snapshots, evaluating deployment configuration snippets, and decoding captured framed message streams.

The project is designed for environments where diagnostics have to be reviewed without network access, without external package managers, and without the original device present. A typical support bundle may contain a binary package manifest, a local key/value snapshot, a policy configuration, and a stream capture from a device session. Helioforge keeps those pieces separate internally, then joins them through a shared cross-check layer that helps operators understand which subsystem produced a file and what records were reconstructed.

## Use Cases

- Inspect a field incident bundle collected by a gateway, sensor appliance, or controller.
- Reconstruct the visible record state from an embedded page-store snapshot.
- Audit configuration snippets before they are deployed to constrained devices.
- Decode captured framed streams from serial links, lab fixtures, or local transport logs.
- Produce concise summaries for support tickets without uploading customer data to a service.
- Run deterministic offline regression tests for parsers used by diagnostic tooling.

## Components

- `binpack`: package-container reader for support bundles. It handles a magic/version header, metadata, section table, length-prefixed payloads, nested records, optional indexes, checksums, and cross-section references.
- `minidb`: embedded snapshot reader for small device state stores. It handles pages, slots, typed fields, varints, string table references, page chains, freelist metadata, and journal replay.
- `cfgscript`: deployment-configuration parser. It handles assignments, nested blocks, quoted strings, escapes, comments, data-only include records, arrays, maps, expressions, and symbol references.
- `streamcodec`: framed stream decoder for local captures. It handles partial reads, fragmented frames, metadata-only compression flags, rolling decoder state, message IDs, continuation frames, and multi-frame reconstruction.
- `helioforge`: shared byte profiling, schema inference, rule evaluation, and cross-module triage used by the inspector and by tests.

## Command-Line Inspector

`hf_inspect` reads a file and prints a compact diagnostic summary:

```sh
hf_inspect summary incident.bin
hf_inspect binpack bundle.hfp
hf_inspect minidb state.mdb
hf_inspect cfgscript policy.cfg
hf_inspect streamcodec capture.hfs
```

The `summary` mode profiles the bytes, scores the file against known diagnostic families, runs schema inference, and attempts safe reconstruction through all module readers. The format-specific modes focus on one subsystem and print reconstructed object counts plus parser errors.

## Input Formats

### binpack

Package files begin with `BPK1`, a 16-bit version, 16-bit flags, 16-bit section count, and metadata pairs. Each section table entry stores `id`, `kind`, payload offset, payload length, checksum, and a short name. Kind `1` payloads contain nested records. Kind `2` payloads contain cross-section references. Flag bit `0` enables an index list that is validated against parsed section IDs.

This format is intended for support bundles where individual sections may contain state snapshots, configuration fragments, stream captures, or vendor-specific records. Section references let one payload annotate another without merging unrelated data into a single blob.

### minidb

Snapshot files begin with `MDB1`, page size, page count, string table count, and journal record count. The string table stores varint IDs and length-prefixed strings. Pages contain a page ID, next-page link, freelist slots, and length-prefixed encoded records. Records use varint IDs and typed fields: null, zigzag integer, string, bytes, and string-table reference. After page-chain reconstruction, journal records are replayed into the visible record map.

This format is useful for small device-side state stores where a full database engine would be too expensive, but support tools still need deterministic reconstruction after a crash or power loss.

### cfgscript

Configuration files support `include "name" { key: value }` records and top-level assignments. Values include strings, integers, booleans, arrays, maps, bare identifiers, and `$symbol` or `$call(...)` expressions. Line comments use `#` or `//`. Include records are data references only; the parser does not access the filesystem.

The evaluator resolves symbol expressions from an in-memory environment and reports unresolved references with paths, which makes it useful for dry-run policy inspection.

### streamcodec

Frames start with `SC`, then flags, type, message ID, sequence number, metadata length, payload length, and state byte. Metadata is a sequence of short key/value pairs. Flags record compression metadata and fragmentation/continuation state. The decoder accepts partial input and reconstructs completed messages across calls.

This format is intended for captured diagnostic streams where logs may be split across serial reads, transport chunks, or interrupted device sessions.

## Library Layout

```text
include/
  binpack/
  minidb/
  cfgscript/
  streamcodec/
  helioforge/
src/
  binpack/
  minidb/
  cfgscript/
  streamcodec/
  helioforge/
tools/
  hf_inspect.cc
tests/
```

The shared `helioforge` layer is intentionally separate from the parsers. It provides byte profiling, schema inference, rule evaluation, and cross-check summaries that can be reused by other internal tools without depending on a specific file family.

## Build

```sh
cmake -S . -B build
cmake --build build
```

## Test

```sh
ctest --test-dir build --output-on-failure
```

The tests build small representative diagnostic objects in memory and verify parser reconstruction, reference analysis, evaluation, and stream-session tracking.

## Design Notes

- No network access is performed by the library, tests, or inspector.
- The build has no external dependencies.
- Includes in `cfgscript` are parsed as data records only.
- Checksums are used as diagnostics, not as a gate that prevents deeper inspection.
- Partial and damaged inputs are reported through structured errors where possible.
- The project favors deterministic summaries over best-effort repair.

## Operational Review Checklist

Before using Helioforge in a production diagnostic workflow, review:

- Whether the package, snapshot, config, and stream formats match the devices being supported.
- Whether local support procedures require redaction before summaries are shared.
- Whether checksum mismatch handling matches the team’s incident-response process.
- Whether configuration symbol resolution should be connected to a real deployment inventory.
- Whether the CLI output is sufficient for support tickets or needs a JSON exporter.
- Whether additional tests are needed for organization-specific section and record types.
