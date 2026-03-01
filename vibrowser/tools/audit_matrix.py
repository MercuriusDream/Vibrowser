#!/usr/bin/env python3
"""Generate an audit matrix from AUDIT_FULL.md and detect stale claims via code probes."""

from __future__ import annotations

import argparse
import json
import re
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Dict, List


CHECKBOX_RE = re.compile(r"^- \[(?P<check>[ xX])\] (?P<body>.+?)\s*$")
HEADER_RE = re.compile(r"^(?P<level>#{2,3})\s+(?P<title>.+?)\s*$")


@dataclass
class AuditItem:
    item_id: str
    section: str
    subsection: str
    feature: str
    checked: bool
    declared_status: str
    note: str
    source_line: int
    stale_evidence: List[str]


@dataclass
class Probe:
    probe_id: str
    description: str
    feature_regex: str
    file_path: str
    required_snippets: List[str]
    required_if_claim_present: bool = True


PROBES: List[Probe] = [
    Probe(
        probe_id="events-keyboard-dispatch",
        description="Keyboard dispatch exists in shell path",
        feature_regex=r"^Keyboard events \(keydown, keyup, keypress\)",
        file_path="src/shell/browser_window.mm",
        required_snippets=["dispatch_keyboard_event(", "didKeyEvent"],
    ),
    Probe(
        probe_id="events-dblclick-dispatch",
        description="dblclick dispatch exists",
        feature_regex=r"^dblclick$",
        file_path="src/shell/browser_window.mm",
        required_snippets=['"dblclick"', "didDoubleClickAtX"],
    ),
    Probe(
        probe_id="events-contextmenu-dispatch",
        description="contextmenu dispatch exists",
        feature_regex=r"^contextmenu$",
        file_path="src/shell/browser_window.mm",
        required_snippets=['"contextmenu"', "didContextMenuAtX"],
    ),
    Probe(
        probe_id="events-form-dispatch",
        description="submit/reset default-action dispatch exists",
        feature_regex=r"^Form events \(submit, reset\)",
        file_path="src/js/js_dom_bindings.cpp",
        required_snippets=['"submit"', '"reset"', "dispatch_event_propagated("],
    ),
]


def slugify(text: str) -> str:
    slug = re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")
    return slug or "item"


def normalize_declared_status(checked: bool, raw_status: str) -> str:
    if checked:
        return "implemented"
    status = raw_status.strip().lower()
    if status in {"implemented", "partial", "missing"}:
        return status
    return "unspecified"


def parse_audit(audit_path: Path) -> List[AuditItem]:
    section = ""
    subsection = ""
    items: List[AuditItem] = []

    for line_number, raw_line in enumerate(audit_path.read_text(encoding="utf-8").splitlines(), start=1):
        line = raw_line.strip()
        header_match = HEADER_RE.match(line)
        if header_match:
            level = header_match.group("level")
            title = header_match.group("title")
            if level == "##":
                section = title
                subsection = ""
            elif level == "###":
                subsection = title
            continue

        checkbox_match = CHECKBOX_RE.match(line)
        if not checkbox_match:
            continue

        body = checkbox_match.group("body")
        checked = checkbox_match.group("check").lower() == "x"

        parts = [part.strip() for part in body.split(" — ")]
        feature = parts[0]
        raw_status = parts[1] if len(parts) >= 2 else ""
        note = " — ".join(parts[2:]).strip() if len(parts) >= 3 else ""

        item_id = slugify(f"{section}-{subsection}-{feature}")
        items.append(
            AuditItem(
                item_id=item_id,
                section=section,
                subsection=subsection,
                feature=feature,
                checked=checked,
                declared_status=normalize_declared_status(checked, raw_status),
                note=note,
                source_line=line_number,
                stale_evidence=[],
            )
        )

    return items


def apply_probes(items: List[AuditItem], repo_root: Path) -> Dict[str, Dict[str, object]]:
    probe_results: Dict[str, Dict[str, object]] = {}
    file_cache: Dict[Path, str] = {}

    for probe in PROBES:
        regex = re.compile(probe.feature_regex, re.IGNORECASE)
        matching_items = [
            item
            for item in items
            if regex.search(item.feature)
            and item.declared_status in {"partial", "missing", "unspecified"}
        ]

        file_path = repo_root / probe.file_path
        if file_path not in file_cache:
            file_cache[file_path] = file_path.read_text(encoding="utf-8") if file_path.exists() else ""
        content = file_cache[file_path]

        found = bool(content) and all(snippet in content for snippet in probe.required_snippets)
        if found:
            for item in matching_items:
                item.stale_evidence.append(probe.probe_id)

        probe_results[probe.probe_id] = {
            "description": probe.description,
            "matched_claim_count": len(matching_items),
            "evidence_found": found,
            "required_if_claim_present": probe.required_if_claim_present,
        }

    return probe_results


def summarize(items: List[AuditItem]) -> Dict[str, object]:
    summary: Dict[str, object] = {
        "total_items": len(items),
        "by_status": {"implemented": 0, "partial": 0, "missing": 0, "unspecified": 0},
        "by_section": {},
        "stale_evidence_items": 0,
    }

    by_section: Dict[str, Dict[str, int]] = {}
    for item in items:
        summary["by_status"][item.declared_status] += 1  # type: ignore[index]
        sec = item.section or "(unsectioned)"
        if sec not in by_section:
            by_section[sec] = {"implemented": 0, "partial": 0, "missing": 0, "unspecified": 0}
        by_section[sec][item.declared_status] += 1
        if item.stale_evidence:
            summary["stale_evidence_items"] += 1

    summary["by_section"] = by_section
    return summary


def write_markdown(items: List[AuditItem], summary: Dict[str, object], probe_results: Dict[str, Dict[str, object]], md_out: Path) -> None:
    lines: List[str] = []
    lines.append("# Audit Reconciliation Matrix")
    lines.append("")
    lines.append("Generated from `AUDIT_FULL.md` with code-evidence probes.")
    lines.append("")
    lines.append("## Summary")
    lines.append("")
    lines.append(f"- Total checklist items: {summary['total_items']}")
    by_status = summary["by_status"]
    lines.append(
        "- Status counts: "
        f"implemented={by_status['implemented']}, "
        f"partial={by_status['partial']}, "
        f"missing={by_status['missing']}, "
        f"unspecified={by_status['unspecified']}"
    )
    lines.append(f"- Items flagged with stale evidence: {summary['stale_evidence_items']}")
    lines.append("")

    lines.append("## Probe Results")
    lines.append("")
    for probe_id, result in probe_results.items():
        lines.append(
            f"- `{probe_id}`: claims={result['matched_claim_count']}, "
            f"evidence_found={str(result['evidence_found']).lower()}"
        )
    lines.append("")

    flagged = [item for item in items if item.stale_evidence]
    lines.append("## Flagged Claims")
    lines.append("")
    if not flagged:
        lines.append("- None")
    else:
        for item in flagged:
            lines.append(
                f"- Line {item.source_line}: `{item.feature}` "
                f"({item.section} / {item.subsection}) -> evidence: {', '.join(item.stale_evidence)}"
            )

    md_out.write_text("\n".join(lines) + "\n", encoding="utf-8")


def verify(summary: Dict[str, object], probe_results: Dict[str, Dict[str, object]]) -> int:
    errors: List[str] = []

    total_items = int(summary["total_items"])
    if total_items < 600:
        errors.append(f"Expected >=600 audit items, got {total_items}")

    by_section = summary["by_section"]
    for required in ("CSS Properties", "JS Events API", "Networking & Protocols"):
        if required not in by_section:
            errors.append(f"Missing required section in parsed output: {required}")

    for probe_id, result in probe_results.items():
        claims = int(result["matched_claim_count"])
        evidence_found = bool(result["evidence_found"])
        required = bool(result["required_if_claim_present"])
        if required and claims > 0 and not evidence_found:
            errors.append(f"Probe {probe_id} matched {claims} claim(s) but found no evidence")

    if errors:
        for err in errors:
            print(f"VERIFY ERROR: {err}")
        return 1
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate audit reconciliation artifacts.")
    parser.add_argument("--audit", required=True, type=Path, help="Path to AUDIT_FULL.md")
    parser.add_argument("--repo-root", required=True, type=Path, help="Repository root")
    parser.add_argument("--json-out", required=True, type=Path, help="Output JSON path")
    parser.add_argument("--md-out", required=True, type=Path, help="Output Markdown path")
    parser.add_argument("--verify", action="store_true", help="Enable strict validation checks")
    args = parser.parse_args()

    items = parse_audit(args.audit)
    probe_results = apply_probes(items, args.repo_root)
    summary = summarize(items)

    args.json_out.parent.mkdir(parents=True, exist_ok=True)
    args.md_out.parent.mkdir(parents=True, exist_ok=True)

    payload = {
        "audit_file": str(args.audit),
        "summary": summary,
        "probe_results": probe_results,
        "items": [asdict(item) for item in items],
    }
    args.json_out.write_text(json.dumps(payload, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")
    write_markdown(items, summary, probe_results, args.md_out)

    if args.verify:
        return verify(summary, probe_results)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
