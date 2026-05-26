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
--   umbra            bool      run as background daemon (umbra)           (default: true)

local S = {}

S.memoria = {
    name     = "memoria",
    exec     = "/bin/memoria",
    restart  = "always",
    critical = true,
    umbra    = true
}

S.structa = {
    name     = "structa",
    exec     = "/bin/structa",
    restart  = "on-failure",
    requires = { "memoria" },
    umbra    = true
}

S.ignis = {
    name             = "ignis",
    exec             = "/bin/ignis",
    restart          = "always",
    restart_delay_ms = 500,
    requires         = { "memoria" },
    umbra            = true
}

--S.nox = {
--    name             = "nox",
--    exec             = "/bin/nox",
--    user             = "vespera",
--    args             = { "-v" },
--    restart          = "always",
--    restart_delay_ms = 2000,
--    requires         = { "memoria" },
--    umbra            = false
--}


S.crepusculum = {
    name             = "crepus",
    exec             = "/bin/crepus",
    user             = "vespera",
    args             = { "-v" },
    restart          = "always",
    restart_delay_ms = 2000,
    requires         = { "memoria" },
    umbra            = false
}


return S