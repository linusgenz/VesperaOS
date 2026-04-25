-- /etc/rc.lua
-- VesperaOS boot-time configuration.
-- Called once by init.lua before any service is spawned.

local rc = {}
local base_env = {}
local U = dofile("/etc/lib/init_utils.lua")

local function create_run_tree()
    for _, d in ipairs({ "/run", "/run/services", "/run/mounts", "/run/power", "/run/log" }) do
        U.ensure_dir(d)
    end
    U.info("rc: /run tree ready")
end

local function set(k, v)
    vespera.sys.setenv(k, v, 0)
    base_env[k] = v
end

function rc.get_env()
    return base_env
end

local function setup_environment()
    local V = vespera.sys
    set("PATH",     "/bin:/usr/bin")
    set("LUA_PATH", "/etc/lib/?.lua;/usr/lib/lua/?.lua")
    set("TZ",       "UTC")
    set("TERM",     "vespera-256")
    set("HOME",     "/root")
    set("USER",     "root")
    set("SHELL",    "/bin/nox")

    local hn = "vespera"
    local raw = U.read_file("/etc/hostname")
    if raw then hn = U.trim(raw) end
    set("HOSTNAME", hn)

    U.info("rc: env ready (hostname=" .. hn .. ")")
end


local function apply_sysconfig()
    if not vespera.fs.exists("/etc/sysconfig") then return end
    local cfg, err = U.parse_kv_file("/etc/sysconfig")
    if not cfg then
        U.warn("rc: /etc/sysconfig parse error: " .. tostring(err))
        return
    end
    local lmap = {
        DEBUG = U.LOG_DEBUG,
        INFO  = U.LOG_INFO,
        WARN  = U.LOG_WARN,
        ERROR = U.LOG_ERROR,
    }
    if cfg.LOG_LEVEL then
        local l = lmap[cfg.LOG_LEVEL:upper()]
        if l then U.log_level = l end
    end
    for k, v in pairs(cfg) do
        vespera.sys.setenv("SYSCONF_" .. k, v, 0)
    end
    U.info("rc: sysconfig applied")
end

function rc.run()
    U.info("=== rc.lua starting ===")
    create_run_tree()
    setup_environment()
    apply_sysconfig()
    U.write_file("/run/boot_time", tostring(math.floor(vespera.sys.time())))
    U.info("=== rc.lua complete ===")
end

return rc