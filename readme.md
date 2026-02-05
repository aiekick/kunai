# Kunai

**Fast dependency analyzer for Ninja build systems**

Kunai parses Ninja build files and tells you which binaries need to be rebuilt when source files change. 

## Why "Kunai" ?

A **kunai** is a Japanese throwing blade used by ninjas (shinobis). Small, fast, and precise.

## What it does

Given a Ninja build directory, Kunai:

1. Parses `build.ninja` and `.ninja_deps` to build a dependency graph
2. Stores it in a SQLite database for fast queries
3. Answers questions like:
   - "Which executables depend on this header?"
   - "What tests should I run after modifying this `.cpp`?"
   - "List all binaries in the project"

## How it works

```
┌─────────────────┐     ┌──────────────┐     ┌─────────────┐
│  build.ninja    │───->│              │────>│  kunai.db   │
│  .ninja_deps    │     │    Kunai     │     │  (SQLite)   │
└─────────────────┘     │              │     └─────────────┘
                        │   Parser &   │            │
┌─────────────────┐     │   Analyzer   │<───────────┘
│  Changed files  │────>│              │
│  (from git, CI) │     │              │────> Affected targets
└─────────────────┘     └──────────────┘
```

Kunai uses SHA1 checksums to detect when `build.ninja` or `.ninja_deps` change, 
and only rebuilds the database when necessary.

## Requirements

- C++17 compiler
- CMake 3.20+

## Usage

```
kunai v0.0.XXX
parse Ninja files and Find which executables to rebuild for changed file(s)

usage : kunai [--help] [--rebuild] <build_dir> <command> [<options>]

Positionnal arguments :
  <build_dir>                    The build directory

Optional arguments :
  --rebuild                      Force the kunia database rebuild

Commands :
  stats                          Get stats of the kunai database
  all                            Get all targets by type
    -b, --bins                     Get binaries targets
    -l, --libs                     Get libraries targets
    -s, --sources                  Get sources targets
    -h, --headers                  Get headers targets
    --match <pattern>              match pattern for filtering targets (ex : --match test_*). not case sensitive
  pointed                        Get targets pointed by modified files
    <source_files>  (unlimited)    The source file non case sensitive pattern. Can be a sub-string without wildcards
    -b, --bins                     Get binaries targets
    -l, --libs                     Get libraries targets
    -s, --sources                  Get sources targets
    -h, --headers                  Get headers targets
    --match <pattern>              match pattern for filtering targets (ex : --match test_*). not case sensitive
```

Short options can be combined: `-bls` is equivalent to `-b -l -s`.

## Examples

### List all binaries

```bash
$ kunai build all -b

my_app.exe
test_unit.exe
test_integration.exe
benchmark.exe
```

### Find affected binaries when a header changes

```bash
$ kunai build pointed -b config.h

my_app.exe
test_unit.exe
test_integration.exe
```

### Find affected tests when a source file changes

```bash
$ kunai build pointed -bl toto.cpp

libtoto.so
test_toto.exe
test_valgrind_toto.exe
```

### Filter results with pattern matching

```bash
$ kunai build pointed -b --match test_* config.h

test_unit.exe
test_integration.exe
```

### Multiple files at once

```bash
$ kunai build pointed -b utils.cpp utils.h logger.cpp

my_app.exe
test_utils.exe
test_logger.exe
```

## Use case: CI/CD optimization

Instead of running all tests on every commit, use Kunai to run only affected tests:

```bash
# Get changed files from git
CHANGED_FILES=$(git diff --name-only HEAD~1)

# Find affected test binaries
TESTS=$(kunai build pointed -b --match test_* $CHANGED_FILES)

# Run only affected tests
for test in $TESTS; do
    ./$test
done
```

## Performance

Kunai is designed for large codebases:

- First run: parses and builds the database (~1-2 seconds for 10k files)
- Subsequent runs: instant queries from cached database
- Database rebuild only when build files change (SHA1 verification)

## License

MIT License - See [LICENSE](LICENSE) for details.

## Author

Stéphane Cuillerdier ([@aiekick](https://github.com/aiekick))

Enjoy :)
