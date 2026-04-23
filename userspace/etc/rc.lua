-- /etc/rc.lua
-- VesperaOS boot-time configuration.
-- Called once by init.lua before any service is spawned.

local rc = {}
local U = dofile("/etc/lib/init_utils.lua")

local function create_run_tree()
    for _, d in ipairs({ "/run", "/run/services", "/run/mounts", "/run/power", "/run/log" }) do
        U.ensure_dir(d)
    end
    U.info("rc: /run tree ready")
end


local function setup_environment()
    local V = vespera.sys
    V.setenv("PATH",     "/bin:/usr/bin",                     0)
    V.setenv("LUA_PATH", "/etc/lib/?.lua;/usr/lib/lua/?.lua", 0)
    V.setenv("TZ",       "UTC",                               0)
    V.setenv("TERM",     "vespera-256",                       0)
    V.setenv("HOME",     "/root",                             0)
    V.setenv("USER",     "root",                              0)
    V.setenv("SHELL",    "/bin/nox",                          0)

    local hn = "vespera"
    local raw = U.read_file("/etc/hostname")
    if raw then hn = U.trim(raw) end
    V.setenv("HOSTNAME", hn, 0)

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