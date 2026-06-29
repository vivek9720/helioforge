# cfgscript_fuzzer PoC Report

- Fuzz target: `cfgscript_fuzzer`
- PoC file path: `pocs/cfgscript_fuzzer_stack_overflow_poc.bin`
- Minimized PoC file path: `pocs/cfgscript_fuzzer_minimized_poc.bin`
- Sanitizer type: AddressSanitizer
- Crash class: stack-overflow
- Raw input bytes: yes. The PoC files are direct bytes consumed by `LLVMFuzzerTestOneInput` through `cfgscript_fuzzer`; no generator script is required.
- Deterministic: yes. The primary PoC was run 3 times with `ASAN_OPTIONS=detect_leaks=0:abort_on_error=1:handle_segv=1:use_sigaltstack=1:print_stacktrace=1`; all 3 runs reported `ERROR: AddressSanitizer: stack-overflow` and exited 134.
- Reaches real project code: yes. The stack trace reaches recursive parser code in `src/cfgscript/cfgscript.cc`.

## Reproduction Command

```sh
docker run --rm -v "$PWD:/src" -w /src fenrir-clang16:latest \
  bash -lc 'ASAN_OPTIONS=detect_leaks=0:abort_on_error=1:handle_segv=1:use_sigaltstack=1:print_stacktrace=1 UBSAN_OPTIONS=print_stacktrace=1 ./out/cfgscript_fuzzer pocs/cfgscript_fuzzer_stack_overflow_poc.bin'
```

## Build Command Used

```sh
docker run --rm -v "F:\\Projects\\Fenrir\\helioforge:/src" -w /src \
  -e SRC=/src -e OUT=/src/out -e CXX=clang++-16 \
  -e CXXFLAGS='-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer' \
  -e LIB_FUZZING_ENGINE='-fsanitize=fuzzer' \
  fenrir-clang16:latest bash -lc '.clusterfuzzlite/build.sh'
```

## Sanitizer Summary

```text
ERROR: AddressSanitizer: stack-overflow
SUMMARY: AddressSanitizer: stack-overflow (/src/out/cfgscript_fuzzer+0x221dce) in __asan_memset
```

## Top Project Stack Frames

```text
#5  helioforge::cfgscript::(anonymous namespace)::Parser::parse_array(...)
    /src/src/cfgscript/cfgscript.cc:138:11
#6  helioforge::cfgscript::(anonymous namespace)::Parser::parse_value(...)
    /src/src/cfgscript/cfgscript.cc:221:31
#7  helioforge::cfgscript::(anonymous namespace)::Parser::parse_array(...)
    /src/src/cfgscript/cfgscript.cc:142:12
#8  helioforge::cfgscript::(anonymous namespace)::Parser::parse_value(...)
    /src/src/cfgscript/cfgscript.cc:221:31
```

## Notes

The input is a deeply nested cfgscript array assignment. The crash occurs in recursive cfgscript parsing before the harness does anything crash-specific.

Validation logs:

- `crash_logs/cfgscript_primary_validation_20260629.log`
- `crash_logs/cfgscript_minimized_altstack_validation.log`
- `crash_logs/cfgscript_deep_array_candidate.log`