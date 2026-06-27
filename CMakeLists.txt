cmake_minimum_required(VERSION 3.16)
project(helioforge LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HELIOFORGE_BUILD_FUZZERS "Build libFuzzer targets" OFF)

add_library(helioforge
  src/helioforge/byte_tools.cc
  src/helioforge/crosscheck.cc
  src/helioforge/schema.cc
  src/binpack/binpack.cc
  src/binpack/manifest.cc
  src/minidb/minidb.cc
  src/minidb/query.cc
  src/cfgscript/cfgscript.cc
  src/cfgscript/evaluator.cc
  src/streamcodec/streamcodec.cc
  src/streamcodec/session.cc
)
target_include_directories(helioforge PUBLIC include)
if(MSVC)
  target_compile_options(helioforge PRIVATE /W4)
else()
  target_compile_options(helioforge PRIVATE -Wall -Wextra -Wpedantic)
endif()

function(add_hf_test name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE helioforge)
  add_test(NAME ${name} COMMAND ${name})
endfunction()

enable_testing()
add_hf_test(binpack_tests tests/binpack/binpack_tests.cc)
add_hf_test(minidb_tests tests/minidb/minidb_tests.cc)
add_hf_test(cfgscript_tests tests/cfgscript/cfgscript_tests.cc)
add_hf_test(streamcodec_tests tests/streamcodec/streamcodec_tests.cc)

add_executable(hf_inspect tools/hf_inspect.cc)
target_link_libraries(hf_inspect PRIVATE helioforge)

if(HELIOFORGE_BUILD_FUZZERS)
  add_executable(binpack_fuzzer fuzz/binpack_fuzzer.cc)
  add_executable(minidb_fuzzer fuzz/minidb_fuzzer.cc)
  add_executable(cfgscript_fuzzer fuzz/cfgscript_fuzzer.cc)
  add_executable(streamcodec_fuzzer fuzz/streamcodec_fuzzer.cc)
  foreach(t binpack_fuzzer minidb_fuzzer cfgscript_fuzzer streamcodec_fuzzer)
    target_link_libraries(${t} PRIVATE helioforge)
    target_compile_options(${t} PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_options(${t} PRIVATE -fsanitize=fuzzer,address,undefined)
  endforeach()
endif()
