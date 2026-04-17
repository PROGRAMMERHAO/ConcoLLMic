# Reproducing Experiments

This directory contains Docker-based setups for reproducing the evaluation results. All benchmarks are pre-instrumented and ready to run with `docker-compose`.

## Getting Started

**⚠️ Important: First, initialize and update the submodule to download benchmark artifacts:**

```bash
# Auto-navigate to root and initialize
cd $(git rev-parse --show-toplevel) && git submodule update --init --recursive
```

This will clone the experiment artifacts from the [ConcoLLMic-artifact](https://github.com/ConcoLLMic/ConcoLLMic-artifact) repository into `experiments/benchmarks/`.

## Benchmark Categories

After initializing the submodule, you will have access to:

- **`benchmarks/c_c++_programs/`** — All C/C++ benchmarks are configured in [benchmarks/c_c++_programs/docker-compose.yml](https://github.com/ConcoLLMic/ConcoLLMic-artifact/blob/main/c_c%2B%2B_programs/docker-compose.yml)
- **`benchmarks/multi-language/`** — All Multi-language benchmarks are configured in [benchmarks/multi-language/docker-compose.yml](https://github.com/ConcoLLMic/ConcoLLMic-artifact/blob/main/multi-language/docker-compose.yml)

---

## Running ConcoLLMic

Navigate to the benchmark directory and run (using `bc` from `benchmarks/c_c++_programs/` as an example):

```bash
cd experiments/benchmarks/c_c++_programs

TIMEOUT=48h ANTHROPIC_API_KEY="your_api_key" \
docker-compose up --build \
    --scale bc-concolic=1 \
    bc-concolic
```


**Note**: `48h` here is upper bound of the runtime. By default, ConcoLLMic automatically terminates after 30 minutes of coverage plateau, and it typically runs for ~4.6 hours on `bc`.

---

## Running Comparison Tools

### Available Comparison Tools

Supported comparison tools (availability varies by benchmark):
- `<benchmark>-klee` — KLEE symbolic executor
- `<benchmark>-klee-pending` — KLEE with pending constraints optimization
- `<benchmark>-symcc` — SymCC compiler-based symbolic execution
- `<benchmark>-symsan` — SymSan sanitizer-based symbolic execution
- `<benchmark>-aflplusplus` — AFL++ fuzzer

**Note**: The base image for `klee-pending` needs to be prepared separately from [here](https://srg.doc.ic.ac.uk/projects/pending-constraints/artifact.html). Other tools are built from Dockerfiles we provide.

### Example: Running All Tools on `bc`

Run 5 parallel instances of all comparison tools for 48 hours:

```bash
cd experiments/benchmarks/c_c++_programs

TIMEOUT=48h docker-compose up --build \
    --scale bc-klee=5 --scale bc-klee-pending=5 \
    --scale bc-symcc=5 --scale bc-symsan=5 \
    --scale bc-aflplusplus=5 \
    bc-klee bc-klee-pending bc-symcc bc-symsan bc-aflplusplus
```


---

## Directed Testing Experiments

The `krep` benchmark includes a pre-configured directed testing experiment targeting a known vulnerability at `krep.c:1556`. These services are defined in `docker-compose.override.yml` (automatically merged with `docker-compose.yml` by Docker Compose).

### ConcoLLMic Directed Variants

Three ConcoLLMic modes are available for comparison:

```bash
cd experiments/benchmarks/c_c++_programs

# Full directed mode: backward reasoning + distance-aware DFS, stops on target
ANTHROPIC_API_KEY="your_api_key" TIMEOUT=14400 \
docker-compose up --build krep-directed

# Directed without backward reasoning: distance-aware DFS only
ANTHROPIC_API_KEY="your_api_key" TIMEOUT=14400 \
docker-compose up --build krep-directed-nobr

# Monitor-only: undirected exploration that tracks when the target is first reached
ANTHROPIC_API_KEY="your_api_key" TIMEOUT=14400 \
docker-compose up --build krep-monitor
```

### Baseline Tool Directed Variants

All baseline tools have corresponding directed variants. These tools do not perform directed execution — they fuzz/run normally, and `run.sh` records when `krep.c:1556` is first covered during post-run replay:

```bash
cd experiments/benchmarks/c_c++_programs

# Build base images first
docker-compose build klee aflplusplus symcc symsan

# Run all baseline directed variants
TIMEOUT=14400 docker-compose up --build \
    krep-klee-directed \
    krep-klee-pending-directed \
    krep-aflplusplus-directed \
    krep-symcc-directed \
    krep-symsan-directed
```

### Output Files

Results are written to `krep/results/<tool>-<timestamp>-<hostname>/`:

| File | Description |
|------|-------------|
| `target_reached.log` | Records elapsed time when `krep.c:1556` was first covered; ends with `SUMMARY: <tool> reached/did NOT reach krep.c:1556 after Xs` |
| `coverage_summary.csv` | Time-series coverage with columns: `Time` (Unix timestamp), `l_per`/`l_abs` (line coverage % and count), `b_per`/`b_abs` (branch coverage % and count), `covered_times_of_line` (target line hit count) |
| `execution_command.log` | Full command, environment variables, and start time for the run |

---

## Cleanup

Stop and remove all containers:
```bash
docker-compose down
```

---

## Benchmark Directory Organization

Each *benchmark directory* contains:
- Pre-instrumented code packaged as `<benchmark>-instr.tar.gz`
- Dockerfiles for different tools
- Seed inputs in `seeds/` (for `AFL++`, `SymCC`, `SymSan`) or `seeds_klee/` (for `KLEE`, `KLEE-Pending`)
- Test harnesses in `seed_execs/` (for `ConcoLLMic`)

Results (test cases, logs, coverage data) are stored in subdirectories after execution, e.g., `bc/results`

