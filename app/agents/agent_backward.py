"""
Backward reasoning agent for directed test generation.

Runs ONCE at startup. Analyzes the target location and reasons backward
from the target to produce a ReachabilityPlan -- an ordered chain of
conditions from program entry to the target.
"""

import json

from loguru import logger
from xml.sax.saxutils import unescape

from app.agents.common import (
    ExecutionInformation,
    Instructions,
    SourceCode,
    wrap_between_tags,
)
from app.agents.directed import DirectedTarget, ReachabilityPlan, ReachabilityStep
from app.data_structures import MessageThread
from app.log import print_step
from app.model import common
from app.model.common import Usage


SYSTEM_PROMPT = """
You are an expert program analyst. Your task is to analyze a program and determine how to reach a specific target location by reasoning BACKWARD from the target.

You will be given:
1. The target location (file and line number)
2. The source code of relevant files
3. The test harness that executes the program

Your goal is to produce a **Reachability Plan**: an ordered list of steps from the program entry point to the target, identifying the conditions that must be satisfied at each step.

## Reasoning Process

1. Find the target line in the source code. What function contains it?
2. What condition (if/else, switch, loop) gates the target line? What must be true to execute it?
3. Where is that function called from? What conditions must hold at the call site?
4. Continue backward until you reach main() or the program entry point.

## Output Format

You MUST respond with a JSON object (and nothing else) in this exact format:
```json
{
  "summary": "Brief natural language summary of the path from entry to target",
  "steps": [
    {
      "file": "filename.c",
      "function": "main",
      "condition": "Natural language description of condition that must hold",
      "line_start": 10,
      "line_end": 15
    },
    {
      "file": "filename.c",
      "function": "process_input",
      "condition": "The input buffer must contain at least 5 characters",
      "line_start": 30,
      "line_end": 35
    }
  ]
}
```

Steps should be ordered from program entry to target (forward order).
Each step's condition should describe what the INPUT or ENVIRONMENT must satisfy, not internal program state.
Be specific and concrete -- avoid vague conditions like "the right input".
"""

BACKWARD_REASONING_TEMPERATURE = 0.0


def backward_reasoning(
    targets: list[DirectedTarget],
    exec_code: str,
    source_files: dict[str, str],
) -> tuple[ReachabilityPlan, Usage]:
    """Run backward reasoning from target to produce a reachability plan.

    Args:
        targets: The directed targets to reason about.
        exec_code: The initial test harness code.
        source_files: Dict of {relative_file_path: source_code_string}.

    Returns:
        - plan: The reachability plan.
        - usage: LLM token usage.
    """
    msg_thread = MessageThread()
    msg_thread.add_system(
        unescape(Instructions(instructions=SYSTEM_PROMPT).to_xml().decode())
    )

    # Build target description
    target_desc_parts = []
    for i, t in enumerate(targets):
        func_info = f" (in function: {t._target_function})" if t._target_function else ""
        target_desc_parts.append(
            f"Target {i + 1}: {t.file_path}:{t.line_start}-{t.line_end}{func_info}"
        )
    target_desc = "\n".join(target_desc_parts)

    # Build user prompt
    harness_section = wrap_between_tags(
        ExecutionInformation.__xml_tag__,
        f"Test harness:\n```python\n{exec_code}\n```",
    )

    source_sections = []
    for file_path, source_code in source_files.items():
        lines = source_code.split("\n")
        if len(lines) > 500:
            source_code = "\n".join(lines[:500]) + f"\n... ({len(lines) - 500} more lines)"
        source_sections.append(
            wrap_between_tags(
                SourceCode.__xml_tag__,
                f"File: {file_path}\n```\n{source_code}\n```",
            )
        )

    user_prompt = (
        f"**Target Location(s):**\n{target_desc}\n\n"
        + harness_section
        + "\n\n"
        + "\n\n".join(source_sections)
        + "\n\nReason backward from the target and produce the reachability plan as JSON."
    )

    msg_thread.add_user(user_prompt)

    response_content, _, usage = common.SELECTED_MODEL.call(
        msg_thread.to_msg(),
        temperature=BACKWARD_REASONING_TEMPERATURE,
        response_format="json_object",
    )

    # Parse the response
    plan = _parse_reachability_plan(response_content)

    logger.info(
        "Backward reasoning complete. Plan has {} steps: {}",
        len(plan.steps),
        plan.summary,
    )

    return plan, usage


def _parse_reachability_plan(response: str) -> ReachabilityPlan:
    """Parse LLM response into a ReachabilityPlan."""
    # Try to extract JSON from the response
    response = response.strip()

    # Handle markdown code blocks
    if "```json" in response:
        start = response.index("```json") + len("```json")
        end = response.index("```", start)
        response = response[start:end].strip()
    elif "```" in response:
        start = response.index("```") + len("```")
        end = response.index("```", start)
        response = response[start:end].strip()

    try:
        data = json.loads(response)
    except json.JSONDecodeError:
        logger.warning("Failed to parse backward reasoning response as JSON")
        return ReachabilityPlan(
            steps=[],
            summary=response[:500],  # Use raw response as summary fallback
        )

    steps = []
    for step_data in data.get("steps", []):
        line_range = None
        if step_data.get("line_start") and step_data.get("line_end"):
            line_range = (int(step_data["line_start"]), int(step_data["line_end"]))
        steps.append(
            ReachabilityStep(
                file=step_data.get("file", ""),
                function=step_data.get("function", ""),
                condition=step_data.get("condition", ""),
                line_range=line_range,
            )
        )

    return ReachabilityPlan(
        steps=steps,
        summary=data.get("summary", ""),
    )
