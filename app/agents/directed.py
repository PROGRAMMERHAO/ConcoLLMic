"""
Directed test generation infrastructure for ConcoLLMic.

Provides target specification, distance computation, call graph tracking,
target reachability checking, and adaptive strategy switching.
"""

import os
import re
import time
from dataclasses import dataclass, field

import yaml
from loguru import logger

from app.agents.common import FILE_TRACE_PATTERN
from app.agents.coverage import Coverage
from app.agents.trace import get_executed_blocks, trace_compress


@dataclass
class DirectedTarget:
    """A user-specified target location to reach."""

    file_path: str  # relative to project root (normalized)
    line_start: int
    line_end: int
    reached: bool = False
    reached_by_tc_id: int | None = None
    reached_at_time: float | None = None

    # Resolved lazily after coverage is initialized
    _target_function: str | None = field(default=None, repr=False)

    def to_dict(self) -> dict:
        return {
            "file_path": self.file_path,
            "line_start": self.line_start,
            "line_end": self.line_end,
            "reached": self.reached,
            "reached_by_tc_id": self.reached_by_tc_id,
            "reached_at_time": self.reached_at_time,
        }


@dataclass
class ReachabilityStep:
    """One step in the backward-reasoned path from entry to target."""

    file: str
    function: str
    condition: str  # natural language condition
    line_range: tuple[int, int] | None = None


@dataclass
class ReachabilityPlan:
    """Backward-reasoned path from program entry to target."""

    steps: list[ReachabilityStep] = field(default_factory=list)
    summary: str = ""

    def format_for_prompt(self) -> str:
        if not self.steps:
            return self.summary or "(no reachability plan available)"
        lines = [self.summary, ""]
        for i, step in enumerate(self.steps, 1):
            loc = f" (lines {step.line_range[0]}-{step.line_range[1]})" if step.line_range else ""
            lines.append(f"{i}. [{step.file}] {step.function}{loc}: {step.condition}")
        return "\n".join(lines)


@dataclass
class ProgressTracker:
    """Tracks directed progress and manages adaptive strategy switching."""

    distance_history: list[tuple[int, int]] = field(default_factory=list)
    best_distance: int = 4
    stall_counter: int = 0
    current_strategy: str = "directed"  # "directed" | "explore"
    explore_countdown: int = 0

    STALL_THRESHOLD: int = 3
    EXPLORE_ROUNDS: int = 2

    def update_strategy(self, round_cnt: int, new_min_distance: int) -> str:
        """Update strategy based on progress. Returns the new strategy."""
        self.distance_history.append((round_cnt, new_min_distance))

        if new_min_distance < self.best_distance:
            self.best_distance = new_min_distance
            self.stall_counter = 0
            self.current_strategy = "directed"
        else:
            self.stall_counter += 1

        if self.stall_counter >= self.STALL_THRESHOLD:
            if self.current_strategy == "directed":
                logger.info(
                    "Directed strategy stalled for {} rounds, switching to explore mode",
                    self.stall_counter,
                )
                self.current_strategy = "explore"
                self.explore_countdown = self.EXPLORE_ROUNDS
            elif self.current_strategy == "explore":
                self.explore_countdown -= 1
                if self.explore_countdown <= 0:
                    logger.info(
                        "Explore phase complete, re-focusing on directed strategy"
                    )
                    self.current_strategy = "directed"
                    self.stall_counter = 0

        return self.current_strategy

    def to_dict(self) -> dict:
        return {
            "distance_history": self.distance_history,
            "best_distance": self.best_distance,
            "stall_counter": self.stall_counter,
            "current_strategy": self.current_strategy,
        }


class DirectedTargetManager:
    """Manages directed targets, distance computation, and strategy."""

    def __init__(self, targets: list[DirectedTarget], ce_start_time: float, monitor_only: bool = False, no_backward_reasoning: bool = False):
        self.targets = targets
        self.ce_start_time = ce_start_time
        self.reachability_plan: ReachabilityPlan | None = None
        self.progress = ProgressTracker()
        self.monitor_only = monitor_only  # if True, track reach times but never inject directed prompts
        self.no_backward_reasoning = no_backward_reasoning  # if True, skip backward reasoning (directed prompts still active)

        # Observed call graph: "file:func" -> set of "file:func" callees
        self.call_graph: dict[str, set[str]] = {}

    @staticmethod
    def parse_target_args(
        target_args: list[str], project_dir: str, monitor_only: bool = False, no_backward_reasoning: bool = False
    ) -> "DirectedTargetManager":
        """Parse --target CLI arguments into a DirectedTargetManager."""
        targets = []
        for arg in target_args:
            # Format: file:line or file:line_start-line_end
            if ":" not in arg:
                raise ValueError(
                    f"Invalid target format '{arg}'. Expected file:line or file:line_start-line_end"
                )
            file_part, line_part = arg.rsplit(":", 1)
            file_path = os.path.normpath(file_part)

            if "-" in line_part:
                start, end = line_part.split("-", 1)
                line_start, line_end = int(start), int(end)
            else:
                line_start = line_end = int(line_part)

            targets.append(
                DirectedTarget(
                    file_path=file_path,
                    line_start=line_start,
                    line_end=line_end,
                )
            )
            logger.info("Directed target: {}:{}-{}", file_path, line_start, line_end)

        return DirectedTargetManager(targets, time.time(), monitor_only=monitor_only, no_backward_reasoning=no_backward_reasoning)

    def resolve_target_functions(self):
        """Resolve which function contains each target line. Call after coverage is initialized."""
        coverage = Coverage.get_instance()
        for target in self.targets:
            file_cov = coverage.get_file_coverage(target.file_path)
            if file_cov is None:
                logger.warning(
                    "No coverage data for target file: {}", target.file_path
                )
                continue
            for (func_name, block_id), (
                real_start,
                real_end,
            ) in file_cov.block2real_lines.items():
                if real_start <= target.line_start <= real_end:
                    target._target_function = func_name
                    logger.info(
                        "Target {}:{} is in function '{}'",
                        target.file_path,
                        target.line_start,
                        func_name,
                    )
                    break
            if target._target_function is None:
                logger.warning(
                    "Could not resolve function for target {}:{}",
                    target.file_path,
                    target.line_start,
                )

    def update_call_graph_from_trace(self, execution_trace: str):
        """Update observed call graph from an execution trace."""
        call_chain = trace_compress(execution_trace)
        for i in range(len(call_chain) - 1):
            caller_file, caller_func, _ = call_chain[i]
            callee_file, callee_func, _ = call_chain[i + 1]
            caller_key = f"{caller_file}:{caller_func}"
            callee_key = f"{callee_file}:{callee_func}"
            if caller_key not in self.call_graph:
                self.call_graph[caller_key] = set()
            self.call_graph[caller_key].add(callee_key)

    def compute_distance(self, execution_trace: str, target: DirectedTarget) -> int:
        """Compute distance from an execution trace to a target.

        Returns:
            0 = target line covered
            1 = same function as target entered
            2 = same file as target entered
            3 = a known caller of target function entered
            4 = no connection
        """
        if execution_trace is None:
            return 4

        call_chain = trace_compress(execution_trace)
        executed_files = set()
        executed_funcs = set()  # "file:func" keys
        executed_func_names_by_file: dict[str, set[str]] = {}

        for file_name, func_name, blocks in call_chain:
            file_name = os.path.normpath(file_name)
            executed_files.add(file_name)
            executed_funcs.add(f"{file_name}:{func_name}")
            if file_name not in executed_func_names_by_file:
                executed_func_names_by_file[file_name] = set()
            executed_func_names_by_file[file_name].add(func_name)

        target_file = os.path.normpath(target.file_path)
        target_func = target._target_function

        # Distance 0: Check if target lines are covered
        coverage = Coverage.get_instance()
        file_cov = coverage.get_file_coverage(target_file)
        if file_cov:
            for line in range(target.line_start, target.line_end + 1):
                if line in file_cov.real_line2line:
                    actual_line = file_cov.real_line2line[line]
                    if file_cov.get_line_covered_times(actual_line) > 0:
                        return 0

        # Distance 1: Same function entered
        if target_func and target_file in executed_func_names_by_file:
            if target_func in executed_func_names_by_file[target_file]:
                return 1

        # Distance 2: Same file entered
        if target_file in executed_files:
            return 2

        # Distance 3: A known caller of the target function entered
        if target_func:
            target_key = f"{target_file}:{target_func}"
            for caller_key, callees in self.call_graph.items():
                if target_key in callees and caller_key in executed_funcs:
                    return 3

        return 4

    def compute_min_distance(self, execution_trace: str) -> int:
        """Compute minimum distance across all targets."""
        unreached = [t for t in self.targets if not t.reached]
        if not unreached:
            return 0  # all targets reached
        return min(self.compute_distance(execution_trace, t) for t in unreached)

    def check_target_reached(self, tc_id: int, execution_trace: str):
        """Check if any target was newly reached by this test case's execution.

        Checks the individual tc's trace (not the cumulative Coverage singleton)
        so the correct tc is credited even when multiple tcs run in the same round.
        """
        if execution_trace is None:
            return

        coverage = Coverage.get_instance()
        for target in self.targets:
            if target.reached:
                continue

            target_file = os.path.normpath(target.file_path)
            file_cov = coverage.get_file_coverage(target_file)
            if file_cov is None:
                continue

            # Extract only the trace entries for this target's file, then derive
            # which blocks THIS tc executed (independent of cumulative coverage).
            file_trace_lines = []
            for trace_line in execution_trace.splitlines():
                for fp, action, func_name, block_id in re.findall(
                    FILE_TRACE_PATTERN, trace_line
                ):
                    if os.path.normpath(fp) == target_file:
                        file_trace_lines.append(f"{action} {func_name} {block_id}")

            if not file_trace_lines:
                continue

            executed_blocks = get_executed_blocks("\n".join(file_trace_lines))

            # Check if any executed block covers the target line range.
            for block, (real_start, real_end) in file_cov.block2real_lines.items():
                if block not in executed_blocks:
                    continue
                for line in range(target.line_start, target.line_end + 1):
                    if real_start <= line <= real_end:
                        target.reached = True
                        target.reached_by_tc_id = tc_id
                        target.reached_at_time = time.time() - self.ce_start_time
                        logger.info(
                            "Directed target {}:{}-{} REACHED by test case #{} at {:.1f}s",
                            target.file_path,
                            target.line_start,
                            target.line_end,
                            tc_id,
                            target.reached_at_time,
                        )
                        break
                if target.reached:
                    break

    def all_targets_reached(self) -> bool:
        return all(t.reached for t in self.targets)

    def get_target_descriptions(self) -> str:
        """Format target descriptions for agent prompts."""
        parts = []
        for i, t in enumerate(self.targets):
            status = "REACHED" if t.reached else "NOT YET REACHED"
            func_info = f" (function: {t._target_function})" if t._target_function else ""
            parts.append(
                f"Target {i + 1}: {t.file_path}:{t.line_start}-{t.line_end}{func_info} [{status}]"
            )
        return "\n".join(parts)

    def get_target_code_contexts(self) -> dict[str, str]:
        """Extract code context around each target for prompt injection."""
        contexts = {}
        coverage = Coverage.get_instance()
        for target in self.targets:
            if target.reached:
                continue
            file_cov = coverage.get_file_coverage(
                os.path.normpath(target.file_path)
            )
            if file_cov is None:
                continue

            # Get the target function's full code with coverage info
            if target._target_function:
                func_lines = file_cov.get_function_real_lines(target._target_function)
                if func_lines:
                    min_line = min(func_lines)
                    max_line = max(func_lines)
                    code_lines = []
                    real_line_cov = file_cov.get_real_line_coverage()
                    for rl in range(min_line, max_line + 1):
                        content = file_cov.get_real_line_content(rl)
                        cov_count = real_line_cov.get(rl, 0)
                        marker = ">>>" if target.line_start <= rl <= target.line_end else "   "
                        cov_marker = f"[cov:{cov_count}]" if cov_count > 0 else "[NOT COV]"
                        code_lines.append(f"{marker} {rl:4d} {cov_marker} {content}")
                    key = f"{target.file_path}:{target.line_start}-{target.line_end}"
                    contexts[key] = "\n".join(code_lines)
            else:
                # Fallback: just show lines around the target
                context_radius = 10
                code_lines = []
                for rl in range(
                    max(1, target.line_start - context_radius),
                    target.line_end + context_radius + 1,
                ):
                    try:
                        content = file_cov.get_real_line_content(rl)
                        marker = ">>>" if target.line_start <= rl <= target.line_end else "   "
                        code_lines.append(f"{marker} {rl:4d} {content}")
                    except (KeyError, IndexError):
                        break
                key = f"{target.file_path}:{target.line_start}-{target.line_end}"
                contexts[key] = "\n".join(code_lines)

        return contexts

    def is_directed_active(self) -> bool:
        """Whether directed prompts should be used (not in explore phase, not monitor-only)."""
        if self.monitor_only:
            return False
        return self.progress.current_strategy == "directed"

    def save(self, out_dir: str):
        """Save directed target state to disk."""
        data = {
            "targets": [t.to_dict() for t in self.targets],
            "progress": self.progress.to_dict(),
            "call_graph_size": sum(len(v) for v in self.call_graph.values()),
        }
        path = os.path.join(out_dir, "directed_targets.yaml")
        with open(path, "w") as f:
            yaml.dump(data, f, default_flow_style=False, sort_keys=False)
