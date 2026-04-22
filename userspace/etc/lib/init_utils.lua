-- /etc/lib/init_utils.lua
-- Shared utilities for the VesperaOS init system.
-- Loaded via:  local U = dofile("/etc/lib/init_utils.lua")

local U = {}

-- ── Log levels ───────────────────────────────────────────────────────────────
U.LOG_DEBUG = 0
U.LOG_INFO  = 1
U.LOG_WARN  = 2
U.LOG_ERROR = 3
U.log_level = U.LOG_INFO

local _tags = { "[DEBUG] ", "[INFO]  ", "[WARN]  ", "[ERROR] " }

function U.log(level, msg)
    if level < U.log_level then return end
    vespera.log.write((_tags[level + 1] or "[?]     ") .. tostring(msg))
end
function U.debug(m) U.log(U.LOG_DEBUG, m) end
function U.info(m)  U.log(U.LOG_INFO,  m) end
function U.warn(m)  U.log(U.LOG_WARN,  m) end
function U.err(m)   U.log(U.LOG_ERROR, m) end

-- ── Error helpers ────────────────────────────────────────────────────────────

-- Assert a syscall result is non-nil; error() with context on failure.
function U.check(result, errval, ctx)
    if result == nil then
        error(string.format("[init] %s failed: errno %d", ctx, errval or 0), 2)
    end
    return result
end

-- Like check but only warns; returns true/false.
function U.try(result, errval, ctx)
    if result == nil then
        U.warn(string.format("%s failed: errno %d (continuing)", ctx, errval or 0))
        return false
    end
    return true
end

-- ── Topological sort ─────────────────────────────────────────────────────────
-- Returns an ordered array of service names (deps before dependents).
-- Raises on cycle.
function U.topo_sort(services)
    local order, visited, in_prog = {}, {}, {}
    local function visit(name)
        if visited[name] then return end
        if in_prog[name] then
            error("[init] dependency cycle at: " .. name)
        end
        local svc = services[name]
        if not svc then
            U.warn("unknown dependency: " .. name .. " (skipped)")
            return
        end
        in_prog[name] = true
        for _, dep in ipairs(svc.requires or {}) do visit(dep) end
        in_prog[name] = nil
        visited[name] = true
        order[#order + 1] = name
    end
    for name in pairs(services) do visit(name) end
    return order
end

-- ── String / path helpers ────────────────────────────────────────────────────
function U.trim(s)
    return (s or ""):match("^%s*(.-)%s*$")
end

function U.path_join(...)
    return (table.concat({...}, "/"):gsub("//+", "/"))
end

-- ── File helpers ─────────────────────────────────────────────────────────────
function U.read_file(path)
    local hnd, err = vespera.io.open(path, vespera.O_RDONLY)
    if not hnd then return nil, err end
    local content, rerr = vespera.io.read_all(hnd)
    vespera.io.close(hnd)
    return content, rerr
end

function U.write_file(path, content)
    local flags = vespera.O_WRONLY | vespera.O_CREAT | vespera.O_TRUNC
    local hnd, err = vespera.io.open(path, flags)
    if not hnd then return nil, err end
    local n, werr = vespera.io.write(hnd, content)
    vespera.io.close(hnd)
    if not n then return nil, werr end
    return true
end

-- ── Directory helper ─────────────────────────────────────────────────────────
function U.ensure_dir(path)
    if vespera.fs.exists(path) then return true end
    local ok, err = vespera.fs.mkdir(path)
    if not ok then
        U.warn(string.format("mkdir %s failed: errno %d", path, err))
        return false
    end
    return true
end

-- ── Mount helper ─────────────────────────────────────────────────────────────
function U.mount(source, target, fstype, flags)
    U.info(string.format("mounting %s on %s (%s)", source, target, fstype))
    local ok, err = vespera.fs.mount(source, target, fstype, flags or 0)
    if not ok then
        U.err(string.format("mount %s -> %s failed: errno %d", source, target, err))
        return false
    end
    return true
end

-- ── Key=Value config parser ──────────────────────────────────────────────────
function U.parse_kv_file(path)
    local content, err = U.read_file(path)
    if not content then return nil, err end
    local result = {}
    for line in (content .. "\n"):gmatch("([^\n]*)\n") do
        line = U.trim(line)
        if line ~= "" and line:sub(1, 1) ~= "#" then
            local k, v = line:match("^([%w_%.%-]+)%s*=%s*(.*)$")
            if k then result[k] = v end
        end
    end
    return result
end

return U