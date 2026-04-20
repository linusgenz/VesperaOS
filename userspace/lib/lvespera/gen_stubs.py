#!/usr/bin/env python3
"""
gen_stubs.py - Generate LuaLS stub file from lvespera.c
Usage: python3 gen_stubs.py lvespera.c > vespera.lua
       python3 gen_stubs.py lvespera.c -o vespera.lua
"""

import re
import sys
import argparse
from dataclasses import dataclass, field
from typing import Optional


# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------

@dataclass
class Param:
    name: str
    description: str
    optional: bool = False

@dataclass
class Function:
    lua_name: str          # e.g. "vespera.spawn"
    short_name: str        # e.g. "spawn"
    description: str
    params: list[Param] = field(default_factory=list)
    returns: str = ""
    extra_lines: list[str] = field(default_factory=list)

@dataclass
class Constant:
    name: str
    value: str


# ---------------------------------------------------------------------------
# Type inference helpers
# ---------------------------------------------------------------------------

def infer_param_type(name: str, desc: str) -> str:
    desc_l = desc.lower()
    name_l = name.lower()
    if "optional" in desc_l or name_l in ("args", "env"):
        pass
    if any(w in name_l for w in ("path", "mode", "fstype", "device", "message", "format", "data", "msg")):
        return "string"
    if any(w in name_l for w in ("handle", "count", "offset", "whence", "signal", "sig", "code", "capacity", "size")):
        return "number"
    if "seconds" in name_l or "ms" in name_l:
        return "number"
    if name_l in ("args", "env", "handles"):
        return "table"
    return "any"

def infer_return_type(ret_desc: str) -> str:
    """Map @return description to LuaLS type annotation."""
    d = ret_desc.lower()
    if "does not return" in d:
        return None   # noreturn
    if "iterator" in d:
        return "fun():string"
    if "true on success" in d:
        return "true|nil, number?"
    if "exit code" in d:
        return "number|nil, number?"
    if "bytes written" in d or "bytes sent" in d:
        return "number|nil, number?"
    if "handle" in d:
        return "number|nil, number?"
    if "path string" in d or "path" in d:
        return "string|nil, number?"
    if "seconds since epoch" in d or "seconds since boot" in d:
        return "number|nil, number?"
    if "realm_id, unit_id" in d:
        return "number, number"
    if "read_handle, write_handle" in d:
        return "number, number|nil, number?"
    if "table" in d:
        return "table|nil, number?"
    if "data string" in d:
        return "string|nil, number?"
    return "any"


# ---------------------------------------------------------------------------
# Parser
# ---------------------------------------------------------------------------

# Matches the structured doc block used throughout lvespera.c:
#   /*
#   ** vespera.funcname(...)
#   **
#   ** Description text
#   **
#   ** @param ...
#   ** @return ...
#   */
DOC_BLOCK = re.compile(
    r'/\*\n'
    r'\*\* (vespera\.\w+)\(([^)]*)\)\n'   # signature line
    r'(.*?)'                                # body
    r'\*/',
    re.DOTALL
)

PARAM_LINE  = re.compile(r'@param\s+(\S+)\s+(.*)')
RETURN_LINE = re.compile(r'@return\s+(.*)')


def parse_functions(src: str) -> list[Function]:
    functions = []
    for m in DOC_BLOCK.finditer(src):
        full_sig  = m.group(1)          # "vespera.spawn"
        _args_str = m.group(2)          # "path, [args], [env]"
        body      = m.group(3)

        short_name = full_sig.split(".")[1]

        # Extract description (lines before first @tag)
        desc_lines = []
        params: list[Param] = []
        ret = ""
        extra: list[str] = []

        in_desc = True
        for line in body.splitlines():
            line = line.strip().lstrip("* ").rstrip()
            if not line:
                continue
            pm = PARAM_LINE.match(line)
            rm = RETURN_LINE.match(line)
            if pm:
                in_desc = False
                pname = pm.group(1)
                pdesc = pm.group(2)
                optional = pname.startswith("[") or "optional" in pdesc.lower()
                pname = pname.strip("[]")
                params.append(Param(pname, pdesc, optional))
            elif rm:
                in_desc = False
                ret = rm.group(1).strip()
            elif in_desc:
                desc_lines.append(line)
            else:
                extra.append(line)

        description = " ".join(desc_lines).strip()
        functions.append(Function(
            lua_name=full_sig,
            short_name=short_name,
            description=description,
            params=params,
            returns=ret,
            extra_lines=extra,
        ))

    return functions


# Matches entries in the vesp_constants[] C array
CONST_RE = re.compile(r'{"(\w+)",\s*(?:\(lua_Integer\))?\s*([^}]+)}')

def parse_constants(src: str) -> list[Constant]:
    # Find the constants table
    table_m = re.search(r'vesp_constants\[\].*?\{(.*?)\};', src, re.DOTALL)
    if not table_m:
        return []
    constants = []
    for m in CONST_RE.finditer(table_m.group(1)):
        name  = m.group(1)
        value = m.group(2).strip().rstrip(",")
        if name == "NULL":
            continue
        constants.append(Constant(name, value))
    return constants


# ---------------------------------------------------------------------------
# Emitter
# ---------------------------------------------------------------------------

# Groups for header comments in the stub
GROUPS = {
    "Process":    ["spawn", "exit", "wait", "kill", "getrid"],
    "File I/O":   ["open", "close", "read", "write", "seek"],
    "Filesystem": ["mkdir", "unlink", "rmdir", "rename", "chdir", "getcwd", "stat", "readdir"],
    "System":     ["sleep", "time", "uptime", "reboot", "mount", "umount"],
    "Logging":    ["log", "logf"],
    "IPC":        ["channel_create", "channel_send", "channel_recv", "pipe", "poll"],
}

def group_of(name: str) -> str:
    for g, members in GROUPS.items():
        if name in members:
            return g
    return "Misc"

def build_param_list(params: list[Param]) -> str:
    # The ? for optional params belongs ONLY in ---@param annotations,
    # never in the function signature itself (invalid Lua syntax).
    return ", ".join(p.name for p in params)

def emit(functions: list[Function], constants: list[Constant]) -> str:
    lines = []

    lines += [
        "-- vespera.lua",
        "-- Auto-generated LuaLS stub for the VesperaOS vespera module.",
        "-- DO NOT EDIT MANUALLY - regenerate with gen_stubs.py.",
        "",
        "---@meta",
        "",
        "---VesperaOS system module - provides Lua bindings for kernel syscalls.",
        "local vespera = {}",
        "",
    ]

    # Constants
    if constants:
        lines += [
            "-- ---------------------------------------------------------------------------",
            "-- Constants",
            "-- ---------------------------------------------------------------------------",
            "",
        ]
        for c in constants:
            lines.append(f"---@type number")
            lines.append(f"vespera.{c.name} = {c.value}")
        lines.append("")

    # Functions grouped
    current_group = None
    func_by_name = {f.short_name: f for f in functions}

    # Emit in canonical group order, then any leftovers
    ordered = []
    seen = set()
    for members in GROUPS.values():
        for name in members:
            if name in func_by_name:
                ordered.append(func_by_name[name])
                seen.add(name)
    for f in functions:
        if f.short_name not in seen:
            ordered.append(f)

    for func in ordered:
        grp = group_of(func.short_name)
        if grp != current_group:
            current_group = grp
            lines += [
                "-- ---------------------------------------------------------------------------",
                f"-- {grp}",
                "-- ---------------------------------------------------------------------------",
                "",
            ]

        # Description
        if func.description:
            lines.append(f"---{func.description}")

        # Extra description lines (e.g. multi-line notes)
        for xl in func.extra_lines:
            lines.append(f"---{xl}")

        # @param annotations
        for p in func.params:
            ptype = infer_param_type(p.name, p.description)
            opt_marker = "?" if p.optional else ""
            lines.append(f"---@param {p.name}{opt_marker} {ptype} {p.description}")

        # @return annotation
        ret_type = infer_return_type(func.returns)
        if ret_type is not None:
            if func.returns:
                lines.append(f"---@return {ret_type} # {func.returns}")
            else:
                lines.append(f"---@return {ret_type}")
        else:
            lines.append("---@return never")

        # function stub
        param_str = build_param_list(func.params)
        lines.append(f"function vespera.{func.short_name}({param_str}) end")
        lines.append("")

    lines.append("return vespera")
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Generate LuaLS stub from lvespera.c")
    parser.add_argument("input", help="Path to lvespera.c")
    parser.add_argument("-o", "--output", help="Output file (default: stdout)")
    args = parser.parse_args()

    with open(args.input, "r", encoding="utf-8") as f:
        src = f.read()

    functions  = parse_functions(src)
    constants  = parse_constants(src)

    if not functions:
        print("WARNING: no functions found – check that doc comments match the expected format.",
              file=sys.stderr)

    stub = emit(functions, constants)

    if args.output:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(stub)
        print(f"Wrote {len(functions)} functions and {len(constants)} constants to {args.output}",
              file=sys.stderr)
    else:
        sys.stdout.write(stub)


if __name__ == "__main__":
    main()