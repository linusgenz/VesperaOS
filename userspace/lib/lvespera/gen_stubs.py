#!/usr/bin/env python3
"""
gen_stubs.py - Generate LuaLS stub file from lvespera.c (submodule layout)
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
    description: str = ""
    optional: bool = False
    lua_type: Optional[str] = None   # set by C scanner to override infer_param_type

@dataclass
class Function:
    submodule: str          # e.g. "proc"
    short_name: str         # e.g. "spawn"
    description: str = ""
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
    name_l = name.lower()

    # Check exact / special names first to avoid false substring matches
    if name_l == "...":
        return "any"
    if name_l == "handler_fn":
        return "function|nil"
    if name_l in ("args", "args_table", "env", "env_table",
                  "handles", "handles_or_pollspec", "arg_table"):
        return "table"

    if any(w in name_l for w in (
            "path", "fstype", "source", "target", "message", "msg",
            "data", "iface", "interface", "member", "name", "value",
            "fmt", "format", "prefix",
    )):
        return "string"
    if any(w in name_l for w in (
            "handle", "count", "offset", "whence", "signum", "sig",
            "code", "capacity", "size", "realm_id", "unit_id", "request",
            "flags", "timeout", "timeout_ms", "ms", "milliseconds",
            "entry_addr", "arg_ptr", "stack_size", "max_bytes",
            "clk_id", "overwrite", "mode", "secs",
    )):
        return "number"
    return "any"


def infer_return_type(ret_desc: str, description: str = "") -> Optional[str]:
    """Return a LuaLS type string, or None for noreturn functions."""
    combined = (ret_desc + " " + description).lower()

    if "does not return" in combined:
        return None  # ---@return never

    if not ret_desc:
        return "any"

    d = ret_desc.lower()

    if "iterator" in d:
        return "fun():string"
    if "true on success" in d:
        return "true|nil, number?"
    if "exit_code" in d or "exit code" in d:
        return "number|nil, number?"
    if "bytes written" in d or "bytes sent" in d or "bytes_written" in d:
        return "number|nil, number?"
    if "realm_id, unit_id" in d:
        return "number, number"
    if "read_handle, write_handle" in d:
        return "number, number|nil, number?"
    if "sec, nsec" in d:
        return "number, number|nil, number?"
    if "realm_id" in d or "unit_id" in d or "channel_handle" in d:
        return "number|nil, number?"
    if "handle" in d:
        return "number|nil, number?"
    if "data string" in d or "data_string" in d:
        return "string|nil, number?"
    if "path string" in d or "path" in d:
        return "string|nil, number?"
    if "uid" in d or "numeric uid" in d:
        return "number|nil, number?"
    if "table" in d or "{" in d:
        return "table|nil, number?"
    if "seconds since epoch" in d or "seconds since boot" in d or "monotonic" in d:
        return "number"
    if "float" in d or "floating" in d:
        return "number"
    if d.strip() in (":", ""):
        return "any"

    # Generic nil-or-error pattern
    if "nil" in d and "error" in d:
        return "any|nil, number?"

    return "any"


# ---------------------------------------------------------------------------
# Argument string parser
# ---------------------------------------------------------------------------

def parse_args(args_str: str) -> list[Param]:
    """
    Parse a Lua function signature arg string into a Param list.

    Handles forms like:
        path [, args_table [, env_table]]
        realm_id [, signum]
        [capacity]
        source, target, fstype [, flags]
        fmt, ...
    """
    s = args_str.strip()
    if not s:
        return []

    params: list[Param] = []
    optional_depth = 0
    token = ""
    token_optional = False

    def flush(tok: str, opt: bool) -> None:
        name = tok.strip().strip("[] ").strip()
        if name:
            params.append(Param(name=name, description="", optional=opt))

    for ch in s:
        if ch == "[":
            optional_depth += 1
            # Opening bracket at the start of a new token → this param is optional
            if not token.strip():
                token_optional = True
        elif ch == "]":
            optional_depth -= 1
        elif ch == ",":
            flush(token, token_optional)
            token = ""
            token_optional = optional_depth > 0
        else:
            token += ch

    flush(token, token_optional or optional_depth > 0)
    return params


# ---------------------------------------------------------------------------
# Parsers
# ---------------------------------------------------------------------------

# Matches a "rich" single-signature doc block:
#   /*
#   ** submod.func(args) [— optional inline description]
#   ** body…
#   */
SINGLE_DOC_RE = re.compile(
    r"/\*\n"
    r"\*\* (\w+)\.(\w+)\(([^)]*)\)"        # groups 1=submod, 2=func, 3=args
    r"(?:[ \t]*[—\-]+[ \t]*([^\n]*))?\n"   # group 4=inline description (optional)
    r"(.*?)"                                # group 5=body
    r"\*/",
    re.DOTALL,
)

# Matches any block comment
ANY_BLOCK_RE = re.compile(r"/\*(.*?)\*/", re.DOTALL)

# Matches a signature line inside a block comment:
#   ** submod.func(args) [— description]
SIG_IN_BLOCK_RE = re.compile(
    r"^\*\*\s+(\w+)\.(\w+)\(([^)]*)\)"
    r"(?:[ \t]*[—\-]+[ \t]*(.*))?$"
)

# Matches a "** Returns …" line
RETURNS_RE = re.compile(r"^\*\*\s+[Rr]eturns?\s+(.*)")

# Matches a "** param_name  :  description" line
PARAM_DESC_RE = re.compile(r"^\*\*\s+(\w+)\s+:\s+(.*)")

# ---------------------------------------------------------------------------
# C function body scanner (fallback for undocumented functions)
# ---------------------------------------------------------------------------

# Finds a static C binding function, e.g. "static int lproc_spawn(lua_State *L) { … }"
C_FUNC_BODY_RE = re.compile(
    r"static int l(\w+)\s*\(lua_State \*L\)\s*\{(.*?)\n\}",
    re.DOTALL,
)

# Matches luaL_check*/luaL_opt* calls that reveal param types and positions
CHECK_ARG_RE = re.compile(
    r"luaL_(check|opt)(string|lstring|integer|number)\s*\(L,\s*(\d+)"
)


def _scan_c_func(src: str, c_func_name: str,
                 _visited: Optional[set] = None) -> list[Param]:
    """
    Extract param info from a C binding function body by scanning
    luaL_check*/luaL_opt* calls.  Follows one level of delegation
    (e.g. llog_info → llog_prefixed).
    """
    if _visited is None:
        _visited = set()
    if c_func_name in _visited:
        return []
    _visited.add(c_func_name)

    # Find the function body — allow extra C params after lua_State *L
    pattern = re.compile(
        rf"static int {re.escape(c_func_name)}\s*\(lua_State \*L[^)]*\)\s*\{{(.*?)\n\}}",
        re.DOTALL,
    )
    m = pattern.search(src)
    if not m:
        return []
    body = m.group(1)

    # Collect direct check/opt calls
    by_idx: dict[int, tuple[str, bool]] = {}
    for cm in CHECK_ARG_RE.finditer(body):
        kind  = cm.group(1)           # "check" or "opt"
        ctype = cm.group(2)           # "string", "lstring", "integer", "number"
        idx   = int(cm.group(3))
        if idx not in by_idx:
            lua_type = "string" if ctype in ("string", "lstring") else "number"
            by_idx[idx] = (lua_type, kind == "opt")

    if by_idx:
        params: list[Param] = []
        for idx in sorted(by_idx.keys()):
            lua_type, optional = by_idx[idx]
            name = f"arg{idx}"  # generic — no better info available
            params.append(Param(name=name, description="", optional=optional,
                                lua_type=lua_type))
        return params

    # No direct calls found — try one-level delegation to another l* function
    delegate_re = re.compile(r"\b(l\w+)\s*\(L\b")
    for dm in delegate_re.finditer(body):
        delegate = dm.group(1)
        result = _scan_c_func(src, delegate, _visited)
        if result:
            return result

    return []


def infer_params_from_c(src: str, submod: str, fname: str) -> list[Param]:
    """Try to infer params for an undocumented function via its C body."""
    c_func_name = f"l{submod}_{fname}"
    return _scan_c_func(src, c_func_name)
LIB_TABLE_RE = re.compile(
    r"static const luaL_Reg (\w+)_lib\[\].*?\{(.*?)\};",
    re.DOTALL,
)
LIB_ENTRY_RE = re.compile(r'\{"(\w+)",\s*\w+\s*\}')

# Finds submodule registration order from luaopen_vespera
REGISTER_ORDER_RE = re.compile(r'register_submodule\(L,\s*"(\w+)"')

# Finds the constant table
CONST_TABLE_RE = re.compile(r"vesp_constants\[\].*?\{(.*?)\};", re.DOTALL)
CONST_ENTRY_RE = re.compile(r'\{"(\w+)",\s*(?:\(lua_Integer\))?\s*([^}]+)\}')


# ---------------------------------------------------------------------------

def _parse_body(submod: str, func: str, args_str: str,
                body: str, inline_desc: str) -> Function:
    """Parse the body of a single-signature doc block."""
    params = parse_args(args_str)

    # Index params by name for quick description injection
    param_by_name = {p.name: p for p in params}

    desc_lines: list[str] = []
    extra_lines: list[str] = []
    returns = ""
    in_desc = True

    for raw in body.splitlines():
        stripped = raw.strip()

        # Strip leading "** " or "**"
        if stripped.startswith("**"):
            content = stripped[2:].lstrip()
        else:
            content = stripped

        if not content:
            continue

        rm = RETURNS_RE.match(stripped)
        if rm:
            in_desc = False
            returns = rm.group(1).strip()
            continue

        pdm = PARAM_DESC_RE.match(stripped)
        if pdm:
            pname, pdesc = pdm.group(1), pdm.group(2).strip()
            if pname in param_by_name:
                param_by_name[pname].description = pdesc
            in_desc = False
            continue

        if in_desc:
            desc_lines.append(content)
        else:
            extra_lines.append(content)

    description = " ".join(desc_lines).strip() or inline_desc.strip()

    return Function(
        submodule=submod,
        short_name=func,
        description=description,
        params=params,
        returns=returns,
        extra_lines=extra_lines,
    )


def parse_functions(src: str) -> list[Function]:
    functions: list[Function] = []
    seen: set[tuple[str, str]] = set()

    # Pass 1 – rich single-signature doc blocks
    for m in SINGLE_DOC_RE.finditer(src):
        submod     = m.group(1)
        func       = m.group(2)
        args_str   = m.group(3)
        inline_desc = m.group(4) or ""
        body       = m.group(5) or ""

        key = (submod, func)
        if key in seen:
            continue
        seen.add(key)

        functions.append(_parse_body(submod, func, args_str, body, inline_desc))

    # Pass 2 – multi-signature blocks (table-of-contents style comments)
    for bm in ANY_BLOCK_RE.finditer(src):
        block = bm.group(1)
        sig_hits = [
            SIG_IN_BLOCK_RE.match(line.strip())
            for line in block.splitlines()
            if SIG_IN_BLOCK_RE.match(line.strip())
        ]

        if len(sig_hits) < 2:
            continue  # already handled by Pass 1 or irrelevant

        for sm in sig_hits:
            submod    = sm.group(1)
            func      = sm.group(2)
            args_str  = sm.group(3)
            inline    = (sm.group(4) or "").strip()
            key       = (submod, func)

            if key in seen:
                continue
            seen.add(key)

            functions.append(Function(
                submodule=submod,
                short_name=func,
                description=inline,
                params=parse_args(args_str),
            ))

    return functions


def parse_lib_tables(src: str) -> dict[str, list[str]]:
    """Return {submodule_name: [func_name, …]} from luaL_Reg arrays."""
    result: dict[str, list[str]] = {}
    for m in LIB_TABLE_RE.finditer(src):
        submod = m.group(1)
        entries = [e.group(1) for e in LIB_ENTRY_RE.finditer(m.group(2))]
        result[submod] = entries
    return result


def parse_submodule_order(src: str) -> list[str]:
    """Return submodule names in the order they are registered."""
    return REGISTER_ORDER_RE.findall(src)


def parse_constants(src: str) -> list[Constant]:
    tm = CONST_TABLE_RE.search(src)
    if not tm:
        return []
    constants: list[Constant] = []
    for m in CONST_ENTRY_RE.finditer(tm.group(1)):
        name  = m.group(1)
        value = m.group(2).strip().rstrip(",")
        if name == "NULL":
            continue
        constants.append(Constant(name, value))
    return constants


# ---------------------------------------------------------------------------
# Emitter
# ---------------------------------------------------------------------------

SUBMODULE_DESCRIPTIONS: dict[str, str] = {
    "proc":  "Process, realm, and unit management",
    "io":    "Low-level file and device I/O",
    "fs":    "Filesystem operations",
    "ipc":   "Inter-process communication: channels, pipes, poll",
    "sys":   "System utilities: time, sleep, reboot, environment",
    "vbus":  "Virtual event bus",
    "log":   "Logging helpers",
}


def build_param_list(params: list[Param]) -> str:
    return ", ".join(p.name for p in params)


def emit_function(func: Function) -> list[str]:
    lines: list[str] = []

    if func.description:
        lines.append(f"---{func.description}")

    for xl in func.extra_lines:
        lines.append(f"---{xl}")

    for p in func.params:
        ptype      = p.lua_type or infer_param_type(p.name, p.description)
        opt_marker = "?" if p.optional else ""
        desc_part  = f" {p.description}" if p.description else ""
        lines.append(f"---@param {p.name}{opt_marker} {ptype}{desc_part}")

    ret_type = infer_return_type(func.returns, func.description)
    if ret_type is None:
        lines.append("---@return never")
    elif func.returns:
        lines.append(f"---@return {ret_type} # {func.returns}")
    else:
        lines.append(f"---@return {ret_type}")

    param_str = build_param_list(func.params)
    lines.append(
        f"function vespera.{func.submodule}.{func.short_name}({param_str}) end"
    )
    lines.append("")
    return lines


def emit(
        src: str,
        functions: list[Function],
        constants: list[Constant],
        lib_tables: dict[str, list[str]],
        submod_order: list[str],
) -> str:
    out: list[str] = []

    out += [
        "-- vespera.lua",
        "-- Auto-generated LuaLS stub for the VesperaOS vespera module.",
        "-- DO NOT EDIT MANUALLY - regenerate with gen_stubs.py.",
        "",
        "---@meta",
        "",
        "---VesperaOS system module - provides Lua bindings for kernel syscalls.",
        "vespera = {}",
        "",
    ]

    # ── Constants ──────────────────────────────────────────────────────────
    if constants:
        out += [
            "-- ---------------------------------------------------------------------------",
            "-- Constants",
            "-- ---------------------------------------------------------------------------",
            "",
        ]
        for c in constants:
            out.append("---@type number")
            out.append(f"vespera.{c.name} = {c.value}")
        out.append("")

    # ── Submodules ─────────────────────────────────────────────────────────
    func_map: dict[tuple[str, str], Function] = {
        (f.submodule, f.short_name): f for f in functions
    }

    # Emit in registration order first, then any remaining lib tables
    all_submods = list(submod_order)
    for s in lib_tables:
        if s not in all_submods:
            all_submods.append(s)

    for submod in all_submods:
        func_names = lib_tables.get(submod, [])
        if not func_names:
            continue

        desc = SUBMODULE_DESCRIPTIONS.get(submod, submod)

        out += [
            "-- ---------------------------------------------------------------------------",
            f"-- vespera.{submod}  —  {desc}",
            "-- ---------------------------------------------------------------------------",
            "",
        ]

        # Declare the submodule table
        out.append(f"---{desc}")
        out.append(f"vespera.{submod} = {{}}")
        out.append("")

        for fname in func_names:
            func = func_map.get((submod, fname))
            if func is None:
                # No doc block — try to infer params from the C function body
                inferred_params = infer_params_from_c(src, submod, fname)
                func = Function(submodule=submod, short_name=fname,
                                params=inferred_params)
            out += emit_function(func)

    out.append("return vespera")
    return "\n".join(out) + "\n"


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Generate LuaLS stub from lvespera.c (submodule layout)"
    )
    parser.add_argument("input", help="Path to lvespera.c")
    parser.add_argument("-o", "--output", help="Output file (default: stdout)")
    args = parser.parse_args()

    with open(args.input, encoding="utf-8") as f:
        src = f.read()

    functions   = parse_functions(src)
    constants   = parse_constants(src)
    lib_tables  = parse_lib_tables(src)
    submod_order = parse_submodule_order(src)

    if not functions:
        print(
            "WARNING: no functions found – check that doc comments match "
            "the expected format.",
            file=sys.stderr,
        )

    stub = emit(src, functions, constants, lib_tables, submod_order)

    if args.output:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(stub)
        total = sum(len(v) for v in lib_tables.values())
        print(
            f"Wrote {total} functions across {len(lib_tables)} submodules "
            f"and {len(constants)} constants to {args.output}",
            file=sys.stderr,
        )
    else:
        sys.stdout.write(stub)


if __name__ == "__main__":
    main()