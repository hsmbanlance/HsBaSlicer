# ---------------------------------------------------------------------------
# Coverage support (gcov + gcovr)
#
# Enabled through the HSBA_COVERAGE option (see top-level CMakeLists.txt) or
# the "linux-coverage" CMake preset. When enabled:
#   * all targets are compiled/linked with --coverage so that running the
#     Boost.Test executables produces .gcda files inside the build tree;
#   * a "coverage" target aggregates the data with gcovr and enforces the
#     line-coverage thresholds: HSBA_COVERAGE_THRESHOLD_CORE for the core
#     slicing modules and HSBA_COVERAGE_THRESHOLD_OTHER for everything else.
#
# The target exits with an error when a threshold is not met, so both local
# developers and CI gates fail loudly on coverage regressions.
# ---------------------------------------------------------------------------

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(WARNING "HSBA_COVERAGE is only supported with GCC or Clang; coverage disabled.")
    set(HSBA_COVERAGE OFF)
    return()
endif()

# Instrument every target created after include (must happen before the
# add_subdirectory() section of the top-level project).
add_compile_options(--coverage)
add_link_options(--coverage)

# Line-coverage gates. The defaults are anti-regression baselines measured on
# the current test suite (WSL/Linux, gcov): core ~53%, other ~29%. Raise them
# together with new tests; do not lower them without review.
set(HSBA_COVERAGE_THRESHOLD_CORE 45 CACHE STRING "Required line coverage (percent) for core modules")
set(HSBA_COVERAGE_THRESHOLD_OTHER 25 CACHE STRING "Required line coverage (percent) for non-core modules")

# Max concurrent gcov workers for the 'coverage' report. 0 = auto (CPU count).
# Cap this on memory-constrained CI runners (gcov parses run many processes in
# parallel and can starve the host); set e.g. -DHSBA_COVERAGE_JOBS=2 there.
set(HSBA_COVERAGE_JOBS 0 CACHE STRING "Parallel gcov workers for the coverage report (0 = auto-detect)")

# Directories treated as "core"; anything else inside the source tree falls
# into the "other" bucket. gcovr >= 7 matches --filter/--exclude regexes
# against absolute paths, so the entries below are plain directory names that
# get anchored to the source dir when building the command lines.
set(HSBA_COVERAGE_CORE_FILTERS
    "base"
    "2D"
    "utils"
    "cipher"
    "meshmodel"
    "paths"
    "preprocess"
    "support"
    "LibHsBaSlicer"
    CACHE STRING "Source directories counted as core modules for coverage gating")

# Common exclusions: tests, samples, vendored code, generated/build trees.
# "out" also drops the protobuf-generated *.pb.cc/.h living in the build tree.
set(HSBA_COVERAGE_EXCLUDES
    "tests"
    "samples"
    "static_tests"
    "static_check"
    "third_parts_without_vcpkg"
    "docs"
    "out"
    "build"
    CACHE STRING "Source directories always excluded from coverage gating")

find_program(GCOVR_EXECUTABLE NAMES gcovr)
if(NOT GCOVR_EXECUTABLE)
    message(WARNING "gcovr not found: the 'coverage' target will fail. Install it with 'apt install gcovr' or 'pip install gcovr'.")
    set(GCOVR_EXECUTABLE "gcovr-not-found")
endif()

# gcov parsing is per-file and dominates runtime on slow filesystems (WSL /mnt
# drives), so gcovr supports a parallel worker count (-j). Cap it via
# HSBA_COVERAGE_JOBS on memory-constrained CI runners; when unset (0) fall back
# to the CPU count, but only if this gcovr actually advertises the option (the
# short flag shows as "-j, --jobs" in gcovr 7 and "-j [GCOV_PARALLEL]" in 8).
set(_hsba_cov_jobs "")
execute_process(
    COMMAND "${GCOVR_EXECUTABLE}" --help
    OUTPUT_VARIABLE _hsba_gcovr_help
    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
if(HSBA_COVERAGE_JOBS GREATER 0)
    set(_hsba_cov_jobs "-j" "${HSBA_COVERAGE_JOBS}")
elseif(_hsba_gcovr_help MATCHES "-j[ ,]")
    include(ProcessorCount)
    ProcessorCount(_hsba_cov_ncpu)
    if(_hsba_cov_ncpu GREATER 1)
        set(_hsba_cov_jobs "-j" "${_hsba_cov_ncpu}")
    endif()
endif()

set(_hsba_cov_dir "${CMAKE_BINARY_DIR}/coverage")
set(_hsba_cov_json "${_hsba_cov_dir}/coverage.json")

# gcovr matches regexes against absolute paths, so anchor every
# filter/exclude with the source dir. Normalize Windows separators with a
# literal (non-regex) replace, then escape the regex metacharacters that can
# realistically appear inside a filesystem path. The bracket class lists only
# characters that CMake's POSIX engine accepts together ('^' is kept out of
# the class because a leading '^' would turn it into negation; '[' and ']' do
# not occur in real source dirs and backslashes are already converted).
set(_hsba_cov_root_re "${CMAKE_SOURCE_DIR}")
string(REPLACE "\\" "/" _hsba_cov_root_re "${_hsba_cov_root_re}")
string(REGEX REPLACE "([.*+?(){}|$])" "\\\\\\1" _hsba_cov_root_re "${_hsba_cov_root_re}")

# Absolute-path exclude patterns shared by every gcovr invocation.
set(_hsba_cov_excludes_abs "")
foreach(_ex ${HSBA_COVERAGE_EXCLUDES})
    list(APPEND _hsba_cov_excludes_abs "--exclude" "${_hsba_cov_root_re}/${_ex}/")
endforeach()
list(APPEND _hsba_cov_excludes_abs
    "--exclude" "vcpkg_installed" "--exclude" "vcpkg/" "--exclude" "/usr/")

# Pass 1 parses the gcov data once into a JSON tracefile (expensive), the two
# gate passes below only import that tracefile and apply filters (cheap).
# The build tree is passed as explicit search directory so gcovr never scans
# the source tree for coverage data.
set(_hsba_gcovr_dump
    "${GCOVR_EXECUTABLE}"
    ${_hsba_cov_jobs}
    --root "${CMAKE_SOURCE_DIR}"
    --exclude-unreachable-branches
    ${_hsba_cov_excludes_abs}
    --json "${_hsba_cov_json}"
    "${CMAKE_BINARY_DIR}")

# Build the two gate command lines as lists so the custom target runs each
# gate independently and the build fails on the first missed threshold.
set(_hsba_gcovr_core
    "${GCOVR_EXECUTABLE}"
    --root "${CMAKE_SOURCE_DIR}"
    --add-tracefile "${_hsba_cov_json}"
    --print-summary
    --fail-under-line "${HSBA_COVERAGE_THRESHOLD_CORE}%"
    --txt "${_hsba_cov_dir}/core_summary.txt"
    --xml "${_hsba_cov_dir}/coverage_core.xml"
    --html-details "${_hsba_cov_dir}/core/")
foreach(_f ${HSBA_COVERAGE_CORE_FILTERS})
    list(APPEND _hsba_gcovr_core --filter "${_hsba_cov_root_re}/${_f}/")
endforeach()
list(APPEND _hsba_gcovr_core ${_hsba_cov_excludes_abs})

set(_hsba_gcovr_other
    "${GCOVR_EXECUTABLE}"
    --root "${CMAKE_SOURCE_DIR}"
    --add-tracefile "${_hsba_cov_json}"
    --print-summary
    --fail-under-line "${HSBA_COVERAGE_THRESHOLD_OTHER}%"
    --txt "${_hsba_cov_dir}/other_summary.txt"
    --xml "${_hsba_cov_dir}/coverage_other.xml"
    --html-details "${_hsba_cov_dir}/other/")
list(APPEND _hsba_gcovr_other ${_hsba_cov_excludes_abs})
foreach(_f ${HSBA_COVERAGE_CORE_FILTERS})
    list(APPEND _hsba_gcovr_other --exclude "${_hsba_cov_root_re}/${_f}/")
endforeach()

add_custom_target(coverage
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_hsba_cov_dir}"
    COMMAND "${CMAKE_COMMAND}" -E echo
        "=== Parsing gcov data into ${_hsba_cov_json} ==="
    COMMAND ${_hsba_gcovr_dump}
    COMMAND "${CMAKE_COMMAND}" -E echo
        "=== Core modules coverage, threshold ${HSBA_COVERAGE_THRESHOLD_CORE}% ==="
    COMMAND ${_hsba_gcovr_core}
    COMMAND "${CMAKE_COMMAND}" -E echo
        "=== Other modules coverage, threshold ${HSBA_COVERAGE_THRESHOLD_OTHER}% ==="
    COMMAND ${_hsba_gcovr_other}
    COMMAND "${CMAKE_COMMAND}" -E echo
        "Reports written to ${_hsba_cov_dir}"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Running gcovr coverage gates (build and run the tests first)")
