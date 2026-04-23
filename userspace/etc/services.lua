-- /etc/services.lua
-- Service definitions for the VesperaOS init supervisor.
-- This file is loaded (and hot-reloaded on SIGUSR1) by init.lua.
--
-- FIELDS:
--   name             string    unique identifier, no spaces
--   exec             string    absolute path to the binary
--   args             string[]  argv[1..n]  (optional)
--   env              string[]  extra "KEY=VALUE" env vars (optional)
--   requires         string[]  services that must run first (optional)
--   restart          string    "always" | "on-failure" | "never"  (default: "always")
--   restart_delay_ms number    ms before restart attempt           (default: 1000)
--   restart_max      number    0 = unlimited                       (default: 0)
--   oneshot          bool      exits after task; never restarted   (default: false)
--   critical         bool      reboot if permanently dead          (default: false)

local S = {}

S.logd = {
    name     = "logd",
    exec     = "/bin/logd",
    restart  = "always",
    critical = true,
}

S.fsd = {
    name     = "fsd",
    exec     = "/bin/fsd",
    restart  = "on-failure",
    requires = { "logd" },
}

S.powerd = {
    name             = "powerd",
    exec             = "/bin/powerd",
    restart          = "always",
    restart_delay_ms = 500,
    requires         = { "logd" },
}

S.nox = {
    name             = "nox",
    exec             = "/bin/nox",
    env              = {"PATH=/bin"},
    args             = { "-v" },
    restart          = "always",
    restart_delay_ms = 2000,
    requires         = { "logd", "powerd", "fsd" },
}

return S