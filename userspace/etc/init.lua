-- /etc/init.lua
-- VesperaOS PID-1 supervisor.

local U = dofile("/etc/lib/init_utils.lua")
U.info("init.lua starting")

local services_def  = {}
local running       = {}
local realm_to_name = {}
local shutdown_flag = false
local vbus_subs     = {}

local g_log_channel = nil

local LOG_CHANNEL_CAP = 128 * 1024

local function svc_count()
    local n = 0
    for _ in pairs(running) do n = n + 1 end
    return n
end

local function track(name, realm_id)
    local prev = running[name] or { restarts = 0 }
    running[name] = {
        realm_id   = realm_id,
        restarts   = prev.restarts,
        started_at = vespera.sys.uptime(),
    }
    realm_to_name[realm_id] = name
    U.info(string.format("started  %-16s realm=%d  restarts=%d",
            name, realm_id, running[name].restarts))
end

local function untrack(realm_id)
    local name = realm_to_name[realm_id]
    if not name then return nil end
    realm_to_name[realm_id] = nil
    running[name] = nil
    return name
end

local function write_rid(name, realm_id)
    U.write_file("/run/services/" .. name .. ".rid", tostring(realm_id))
end

local function create_log_channel()
    local hid, err = vespera.ipc.channel_create(LOG_CHANNEL_CAP)
    if not hid then
        U.err("failed to create log channel: errno " .. tostring(err))
        return nil
    end
    U.info("log channel created: handle=" .. tostring(hid))
    return hid
end

local function transfer_log_channel_to(name, realm_id)
    if not g_log_channel then return nil end
    local child_hid, err = vespera.proc.handle_transfer(
            g_log_channel, realm_id, vespera.CAP_WRITE)
    if not child_hid then
        U.warn(string.format("handle_transfer for %s failed: errno %d", name, err))
        return nil
    end
    U.write_file("/run/services/" .. name .. ".log_chan", tostring(child_hid))
    return child_hid
end

local function spawn_service(name)
    local def = services_def[name]
    if not def then
        U.err("spawn: unknown service '" .. name .. "'")
        return nil
    end

    local args = { def.exec }
    for _, a in ipairs(def.args or {}) do args[#args + 1] = a end

    local realm_id, err = vespera.proc.spawn(def.exec, args, def.env or {})
    if not realm_id then
        U.err(string.format("spawn %s (%s) failed: errno %d", name, def.exec, err))
        return nil
    end

    transfer_log_channel_to(name, realm_id)
    write_rid(name, realm_id)
    track(name, realm_id)
    return realm_id
end

local function should_restart(name, exit_code)
    local def = services_def[name]
    if not def or def.oneshot then return false, 0 end

    local policy = def.restart or "always"
    if policy == "never" then return false, 0 end
    if policy == "on-failure" and exit_code == 0 then return false, 0 end

    local state = running[name]
    local restarts = state and state.restarts or 0
    local max_r = def.restart_max or 0

    if max_r > 0 and restarts >= max_r then
        U.err(string.format("service %s exhausted restart_max=%d", name, max_r))
        if def.critical then
            U.err("critical: " .. name .. " died — rebooting in 5 s")
            vespera.sys.sleep(5)
            vespera.sys.reboot(vespera.REBOOT_RESTART)
        end
        return false, 0
    end

    return true, (def.restart_delay_ms or 1000)
end

local function reap_one()
    local specs = {}
    for rid in pairs(realm_to_name) do
        specs[#specs + 1] = { hdl = rid, events = vespera.POLLHUP }
    end
    if #specs == 0 then return nil end

    local ready = vespera.ipc.poll(specs, 0)
    if not ready or #ready == 0 then return nil end

    local hdl = ready[1].hdl
    local exit_code = vespera.proc.wait(hdl) or -1
    return untrack(hdl), exit_code
end

local function on_vbus(iface, member, fn)
    local key = iface .. "/" .. (member or "*")
    vbus_subs[key] = fn
    vespera.vbus.subscribe(iface, (member ~= "*") and member or "")
end

local function dispatch_vbus()
    local msg = vespera.vbus.recv()
    if not msg then return false end
    local fn = vbus_subs[msg.interface .. "/" .. msg.member]
            or  vbus_subs[msg.interface .. "/*"]
    if fn then
        local ok, e = pcall(fn, msg)
        if not ok then U.err("vbus handler error: " .. tostring(e)) end
    end
    return true
end

local function setup_vbus_handlers()
    on_vbus("power", "*", function(msg)
        U.debug("vbus power: " .. msg.member)
    end)

    on_vbus("thermal", "throttle", function()
        U.warn("thermal throttle event")
    end)

    on_vbus("hotplug", "add", function(msg)
        U.info("vbus hotplug add: " .. tostring(msg.payload or ""))
    end)

    on_vbus("hotplug", "remove", function(msg)
        U.info("vbus hotplug remove: " .. tostring(msg.payload or ""))
    end)
end

local function hot_reload()
    U.info("hot-reload: /etc/services.lua")
    local ok, new_def = pcall(dofile, "/etc/services.lua")
    if not ok then
        U.err("hot-reload failed: " .. tostring(new_def))
        return
    end
    for name, def in pairs(new_def) do
        if not services_def[name] then
            services_def[name] = def
            spawn_service(name)
        else
            services_def[name] = def
        end
    end
    U.info("hot-reload done (" .. svc_count() .. " running)")
end

local function start_all()
    local ok, order = pcall(U.topo_sort, services_def)
    if not ok then
        order = {}
        for n in pairs(services_def) do order[#order + 1] = n end
    end
    U.info(string.format("starting %d services: %s",
            #order, table.concat(order, " → ")))
    for _, name in ipairs(order) do
        local def = services_def[name]
        if def.requires and #def.requires > 0 then
            vespera.sys.sleep_ms(80)
        end
        spawn_service(name)
    end
end

local function shutdown(mode)
    shutdown_flag = true
    U.info("=== shutdown (mode=" .. tostring(mode) .. ") ===")
    local ok, order = pcall(U.topo_sort, services_def)
    if ok then
        for i = #order, 1, -1 do
            local st = running[order[i]]
            if st then vespera.proc.kill(st.realm_id, vespera.SIGTERM) end
        end
    end
    local deadline = vespera.sys.uptime() + 4.0
    while svc_count() > 0 and vespera.sys.uptime() < deadline do
        if not reap_one() then vespera.sys.sleep_ms(100) end
    end
    for rid, name in pairs(realm_to_name) do
        U.warn("SIGKILL: " .. name)
        vespera.proc.kill(rid, vespera.SIGKILL)
    end
    vespera.sys.sleep_ms(300)
    vespera.sys.reboot(mode)
end

local function setup_signals()
    vespera.proc.sigaction(vespera.SIGUSR1, function() hot_reload() end)
    vespera.proc.sigaction(vespera.SIGTERM, function() shutdown(vespera.REBOOT_RESTART) end)
    vespera.proc.sigaction(vespera.SIGUSR2, function() shutdown(vespera.REBOOT_POWEROFF) end)
end

local function supervisor_loop()
    local vbus_spec = { { hdl = vespera.HANDLE_VBUS, events = vespera.POLLIN } }
    U.info("supervisor loop started (" .. svc_count() .. " services)")
    while not shutdown_flag do
        for _ = 1, 8 do if not dispatch_vbus() then break end end

        local name, exit_code = reap_one()
        if name then
            U.info(string.format("exited %-16s exit=%d", name, exit_code))
            local do_restart, delay_ms = should_restart(name, exit_code)
            if do_restart then
                local st = running[name] or { restarts = 0 }
                running[name] = { restarts = st.restarts + 1 }
                if delay_ms > 0 then
                    vespera.sys.sleep_ms(delay_ms)
                end
                spawn_service(name)
            end
        end

        vespera.ipc.poll(vbus_spec, 200)
    end
end

local function main()
    local ok, err = pcall(function()
        local rc = dofile("/etc/rc.lua")
        rc.run()
    end)
    if not ok then
        vespera.log.error("FATAL: rc.lua: " .. tostring(err))
        vespera.sys.sleep(3)
        vespera.sys.reboot(vespera.REBOOT_RESTART)
    end

    g_log_channel = create_log_channel()

    local ok2, svc_or_err = pcall(dofile, "/etc/services.lua")
    if not ok2 then
        vespera.log.error("FATAL: services.lua: " .. tostring(svc_or_err))
        vespera.sys.sleep(3)
        vespera.sys.reboot(vespera.REBOOT_RESTART)
    end
    services_def = svc_or_err

    setup_signals()
    setup_vbus_handlers()
    start_all()

    local ok3, lerr = pcall(supervisor_loop)
    if not ok3 then
        vespera.log.error("FATAL: supervisor: " .. tostring(lerr))
        vespera.sys.sleep(2)
        vespera.sys.reboot(vespera.REBOOT_RESTART)
    end
end

main()