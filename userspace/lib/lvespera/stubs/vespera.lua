-- vespera.lua
-- Auto-generated LuaLS stub for the VesperaOS vespera module.
-- DO NOT EDIT MANUALLY - regenerate with gen_stubs.py.

---@meta

---VesperaOS system module - provides Lua bindings for kernel syscalls.
vespera = {}

-- ---------------------------------------------------------------------------
-- Constants
-- ---------------------------------------------------------------------------

---@type number
vespera.SEEK_SET = SEEK_SET
---@type number
vespera.SEEK_CUR = SEEK_CUR
---@type number
vespera.SEEK_END = SEEK_END
---@type number
vespera.HANDLE_STDIN = HANDLE_STDIN
---@type number
vespera.HANDLE_STDOUT = HANDLE_STDOUT
---@type number
vespera.HANDLE_STDERR = HANDLE_STDERR
---@type number
vespera.HANDLE_VBUS = HANDLE_VBUS
---@type number
vespera.O_RDONLY = O_RDONLY
---@type number
vespera.O_WRONLY = O_WRONLY
---@type number
vespera.O_RDWR = O_RDWR
---@type number
vespera.O_CREAT = O_CREAT
---@type number
vespera.O_TRUNC = O_TRUNC
---@type number
vespera.O_APPEND = O_APPEND
---@type number
vespera.O_DIRECTORY = O_DIRECTORY
---@type number
vespera.C_FILE = C_FILE
---@type number
vespera.C_DIR = C_DIR
---@type number
vespera.POLLIN = POLLIN
---@type number
vespera.POLLOUT = POLLOUT
---@type number
vespera.POLLERR = POLLERR
---@type number
vespera.POLLHUP = POLLHUP
---@type number
vespera.SIGHUP = SIGHUP
---@type number
vespera.SIGINT = SIGINT
---@type number
vespera.SIGQUIT = SIGQUIT
---@type number
vespera.SIGILL = SIGILL
---@type number
vespera.SIGTRAP = SIGTRAP
---@type number
vespera.SIGABRT = SIGABRT
---@type number
vespera.SIGBUS = SIGBUS
---@type number
vespera.SIGFPE = SIGFPE
---@type number
vespera.SIGKILL = SIGKILL
---@type number
vespera.SIGSEGV = SIGSEGV
---@type number
vespera.SIGUSR1 = SIGUSR1
---@type number
vespera.SIGUSR2 = SIGUSR2
---@type number
vespera.SIGPIPE = SIGPIPE
---@type number
vespera.SIGALRM = SIGALRM
---@type number
vespera.SIGTERM = SIGTERM
---@type number
vespera.SIGCHLD = SIGCHLD
---@type number
vespera.SA_RESTART = SA_RESTART
---@type number
vespera.SA_NODEFER = SA_NODEFER
---@type number
vespera.SA_RESETHAND = SA_RESETHAND
---@type number
vespera.CLOCK_REALTIME = CLOCK_REALTIME
---@type number
vespera.CLOCK_MONOTONIC = CLOCK_MONOTONIC
---@type number
vespera.CLOCK_PROCESS_CPUTIME_ID = CLOCK_PROCESS_CPUTIME_ID
---@type number
vespera.REBOOT_RESTART = 0
---@type number
vespera.REBOOT_POWEROFF = 1
---@type number
vespera.REBOOT_HALT = 2
---@type number
vespera.EPERM = EPERM
---@type number
vespera.ENOENT = ENOENT
---@type number
vespera.ESRCH = ESRCH
---@type number
vespera.EINTR = EINTR
---@type number
vespera.EIO = EIO
---@type number
vespera.ENXIO = ENXIO
---@type number
vespera.ENOEXEC = ENOEXEC
---@type number
vespera.EBADH = EBADH
---@type number
vespera.EAGAIN = EAGAIN
---@type number
vespera.ENOMEM = ENOMEM
---@type number
vespera.EACCES = EACCES
---@type number
vespera.EFAULT = EFAULT
---@type number
vespera.EBUSY = EBUSY
---@type number
vespera.EEXIST = EEXIST
---@type number
vespera.ENODEV = ENODEV
---@type number
vespera.ENOTDIR = ENOTDIR
---@type number
vespera.EISDIR = EISDIR
---@type number
vespera.EINVAL = EINVAL
---@type number
vespera.ENFILE = ENFILE
---@type number
vespera.EMFILE = EMFILE
---@type number
vespera.ENOSPC = ENOSPC
---@type number
vespera.ESPIPE = ESPIPE
---@type number
vespera.EROFS = EROFS
---@type number
vespera.EPIPE = EPIPE
---@type number
vespera.ERANGE = ERANGE
---@type number
vespera.ENAMETOOLONG = ENAMETOOLONG
---@type number
vespera.ENOSYS = ENOSYS
---@type number
vespera.ENOTEMPTY = ENOTEMPTY
---@type number
vespera.ELOOP = ELOOP
---@type number
vespera.EOVERFLOW = EOVERFLOW
---@type number
vespera.EUNKNOWN = EUNKNOWN
---@type number
vespera.EUNSUPPORTED = EUNSUPPORTED
---@type number
vespera.EWOULDBLOCK = EWOULDBLOCK
---@type number
vespera.VSTAT_TYPE_UNKNOWN = VSTAT_TYPE_UNKNOWN
---@type number
vespera.VSTAT_TYPE_FILE = VSTAT_TYPE_FILE
---@type number
vespera.VSTAT_TYPE_DIR = VSTAT_TYPE_DIR
---@type number
vespera.VSTAT_TYPE_CHARDEV = VSTAT_TYPE_CHARDEV
---@type number
vespera.VSTAT_TYPE_BLOCKDEV = VSTAT_TYPE_BLOCKDEV
---@type number
vespera.VSTAT_TYPE_SYMLINK = VSTAT_TYPE_SYMLINK
---@type number
vespera.VSTAT_FLAG_READABLE = VSTAT_FLAG_READABLE
---@type number
vespera.VSTAT_FLAG_WRITABLE = VSTAT_FLAG_WRITABLE
---@type number
vespera.VSTAT_FLAG_EXEC = VSTAT_FLAG_EXEC
---@type number
vespera.VSTAT_FLAG_VIRTUAL = VSTAT_FLAG_VIRTUAL
---@type number
vespera.VSTAT_FLAG_PERMANENT = VSTAT_FLAG_PERMANENT
---@type number
vespera.DT_UNKNOWN = DT_UNKNOWN
---@type number
vespera.DT_FILE = DT_FILE
---@type number
vespera.DT_DIR = DT_DIR
---@type number
vespera.DT_SYMLINK = DT_SYMLINK
---@type number
vespera.DT_CHARDEV = DT_CHARDEV
---@type number
vespera.DT_BLOCKDEV = DT_BLOCKDEV
---@type number
vespera.DT_FIFO = DT_FIFO
---@type number
vespera.DT_SOCKET = DT_SOCKET
---@type number
vespera.DT_EXEC = DT_EXEC
---@type number
vespera.MS_RDONLY = MS_RDONLY
---@type number
vespera.MS_NOATIME = MS_NOATIME
---@type number
vespera.MS_NOEXEC = MS_NOEXEC
---@type number
vespera.MS_REMOUNT = MS_REMOUNT
---@type number
vespera.CAP_NONE = CAP_NONE
---@type number
vespera.CAP_READ = CAP_READ
---@type number
vespera.CAP_WRITE = CAP_WRITE
---@type number
vespera.CAP_RW = CAP_RW
---@type number
vespera.CAP_EXECUTE = CAP_EXECUTE
---@type number
vespera.CAP_NETWORK_BIND = CAP_NETWORK_BIND
---@type number
vespera.CAP_UNIT_SPAWN = CAP_UNIT_SPAWN
---@type number
vespera.CAP_DEVICE_ACCESS = CAP_DEVICE_ACCESS
---@type number
vespera.CAP_ALL = CAP_ALL
---@type number
vespera.VBUS_MSG_SIGNAL = VBUS_MSG_SIGNAL
---@type number
vespera.VBUS_MSG_CALL = VBUS_MSG_CALL
---@type number
vespera.VBUS_MSG_RETURN = VBUS_MSG_RETURN
---@type number
vespera.VBUS_MSG_ERROR = VBUS_MSG_ERROR
---@type number
vespera.VBUS_SUB_WILDCARD = VBUS_SUB_WILDCARD

-- ---------------------------------------------------------------------------
-- vespera.proc  —  Process, realm, and unit management
-- ---------------------------------------------------------------------------

---Process, realm, and unit management
vespera.proc = {}

---Spawns a new realm from the given executable.
---@param path string
---@param args_table? table array of strings  {"arg0", "arg1", …}  (optional)
---@param env_table? table array of "K=V" strings                 (optional)
---@return number|nil, number? # realm_id on success, or nil + error_code.
function vespera.proc.spawn(path, args_table, env_table) end

---Spawns a new unit (thread) inside an existing realm.
---@param realm_id number
---@param entry_addr number user-space function address (integer)
---@param arg_ptr number opaque argument forwarded in RDI (integer, may be 0)
---@param stack_size? number optional, 0 = kernel default
---@return number|nil, number? # unit_id on success, or nil + error_code.
function vespera.proc.spawn_unit(realm_id, entry_addr, arg_ptr, stack_size) end

---Terminate the current unit. Does not return.
---@param code? number
---@return never
function vespera.proc.exit(code) end

---Block until realm_id terminates.
---@param realm_id number
---@return number|nil, number? # exit_code on success, or nil + error_code.
function vespera.proc.wait(realm_id) end

---Send a signal to a realm. signum defaults to SIGKILL (9).
---@param realm_id number
---@param signum? number
---@return true|nil, number? # true on success, or nil + error_code.
function vespera.proc.kill(realm_id, signum) end

---@return number, number # realm_id, unit_id of the calling process.
function vespera.proc.getrid() end

---@return number|nil, number? # the numeric UID of the calling process.
function vespera.proc.getuid() end

---Register a Lua function as a signal handler.
---Pass nil to reset to SIG_DFL.
---NOTE: Because Lua signal handlers run asynchronously, only simple, safe
---operations should be performed inside handler_fn. Prefer using a
---channel or flag variable and polling from the main loop instead.
---@param signum number
---@param handler_fn function|nil Lua function — will be called with (signum) when triggered.
---@param flags? number SA_* bitmask (optional, default 0)
---@return true|nil, number? # true on success, or nil + error_code.
function vespera.proc.sigaction(signum, handler_fn, flags) end

-- ---------------------------------------------------------------------------
-- vespera.io  —  Low-level file and device I/O
-- ---------------------------------------------------------------------------

---Low-level file and device I/O
vespera.io = {}

---Open a file or device.
---Defaults to O_RDONLY.
---Also accepts POSIX-style mode string ("r", "w", "r+", "w+", "a").
---@param path string
---@param flags? number integer bitmask (O_RDONLY / O_WRONLY / O_RDWR / O_CREAT / O_TRUNC).
---@return number|nil, number? # handle on success, or nil + error_code.
function vespera.io.open(path, flags) end

---Close a file or device handle.
---@param handle number
---@return true|nil, number? # true on success, or nil + error_code.
function vespera.io.close(handle) end

---Read up to count bytes from handle.
---or nil + error_code on failure.
---@param handle number
---@param count number
---@return any # empty string "" at EOF.
function vespera.io.read(handle, count) end

---Read the entire file into a string, starting from the current position. Uses repeated 4096-byte reads and concatenates via luaL_Buffer.
---@param handle number
---@return string|nil, number? # data string on success, or nil + error_code.
function vespera.io.read_all(handle) end

---Write data string to handle.
---@param handle number
---@param data string
---@return number|nil, number? # bytes_written on success, or nil + error_code.
function vespera.io.write(handle, data) end

---Reposition the file offset. whence defaults to SEEK_SET.
---@param handle number
---@param offset number
---@param whence? number
---@return any|nil, number? # new absolute offset on success, or nil + error_code.
function vespera.io.seek(handle, offset, whence) end

---Perform a device-specific ioctl.
---For simple requests, pass the arg as a plain integer in field [1].
---@param handle number
---@param request number
---@param arg_table? table optional table whose fields are copied into a uint64_t arg.
---@return any|nil, number? # the ioctl result (device-dependent) on success, or nil + error_code.
function vespera.io.ioctl(handle, request, arg_table) end

-- ---------------------------------------------------------------------------
-- vespera.fs  —  Filesystem operations
-- ---------------------------------------------------------------------------

---Filesystem operations
vespera.fs = {}

---fs.rmdir(path)  — remove an empty directory fs.unlink(path) — remove a file fs.rename(old, new) — rename / move fs.chdir(path)  — change working directory fs.getcwd()     — get current working directory fs.stat(path)   — query file metadata fs.exists(path) — return true/false fs.readdir(path) — return iterator over entry names fs.mount(source, target, fstype [, flags]) fs.umount(target [, flags]) fs.create(path [, flags]) — create file or directory
---@param path string
---@return any
function vespera.fs.mkdir(path) end

---remove an empty directory
---@param path string
---@return any
function vespera.fs.rmdir(path) end

---remove a file
---@param path string
---@return any
function vespera.fs.unlink(path) end

---rename / move
---@param old any
---@param new any
---@return any
function vespera.fs.rename(old, new) end

---change working directory
---@param path string
---@return any
function vespera.fs.chdir(path) end

---get current working directory
---@return any
function vespera.fs.getcwd() end

---@param path string
---@return table|nil, number? # a table with the following fields:
function vespera.fs.stat(path) end

---returns true/false (never errors)
---@param path string
---@return any
function vespera.fs.exists(path) end

---Each call of the iterator returns one entry name string, or nil when done.
---@param path string
---@return any
function vespera.fs.readdir(path) end

---convenience: returns an array table of all entry names
---@param path string
---@return any
function vespera.fs.readdir_table(path) end

---fs.umount(target [, flags])
---@param source string
---@param target string
---@param fstype string
---@param flags? number
---@return any
function vespera.fs.mount(source, target, fstype, flags) end

---@param target string
---@param flags? number
---@return any
function vespera.fs.umount(target, flags) end

---Creates a file (C_FILE) by default, or a directory if flags == C_DIR.
---@param path string
---@param flags? number
---@return any
function vespera.fs.create(path, flags) end

-- ---------------------------------------------------------------------------
-- vespera.ipc  —  Inter-process communication: channels, pipes, poll
-- ---------------------------------------------------------------------------

---Inter-process communication: channels, pipes, poll
vespera.ipc = {}

---Create a new IPC channel. capacity defaults to 4096 bytes.
---@param capacity? number
---@return number|nil, number? # channel_handle on success, or nil + error_code.
function vespera.ipc.channel_create(capacity) end

---Write data (string) to a channel.
----EAGAIN means the channel is full (non-blocking).
---@param handle number
---@param data string
---@return any|nil, number? # bytes_sent on success, or nil + error_code.
function vespera.ipc.channel_send(handle, data) end

---Read up to max_bytes from a channel.
----EAGAIN means the channel is empty.
---@param handle number
---@param max_bytes number
---@return string|nil, number? # data_string on success, or nil + error_code.
function vespera.ipc.channel_recv(handle, max_bytes) end

---Create a unidirectional pipe.
---@return number, number|nil, number? # read_handle, write_handle on success, or nil + error_code.
function vespera.ipc.pipe() end

---Wait for I/O events on one or more handles. Two input formats are supported: 1. Simple array:  { handle1, handle2, … } All handles are monitored for POLLIN | POLLERR | POLLHUP. 2. Array of spec tables:  { {hdl=h, events=POLLIN|POLLOUT}, … } Allows per-handle event masks. timeout_ms:  -1 = block forever, 0 = non-blocking, >0 = milliseconds.
---{ { hdl = handle, revents = bitmask }, … }
---(only entries with non-zero revents are included)
---@param handles_or_pollspec table
---@param timeout_ms number
---@return any|nil, number? # nil + error_code on failure.
function vespera.ipc.poll(handles_or_pollspec, timeout_ms) end

-- ---------------------------------------------------------------------------
-- vespera.sys  —  System utilities: time, sleep, reboot, environment
-- ---------------------------------------------------------------------------

---System utilities: time, sleep, reboot, environment
vespera.sys = {}

---Sleep for the specified duration. Accepts fractions (e.g. 0.25).
---@param seconds any
---@return true|nil, number? # true on success, or nil + error_code if interrupted.
function vespera.sys.sleep(seconds) end

---convenience wrapper
---@param milliseconds number
---@return any
function vespera.sys.sleep_ms(milliseconds) end

---(seconds since 1970-01-01 00:00:00 UTC, with sub-second precision).
---@return number # the current wall-clock time as a floating-point Unix timestamp
function vespera.sys.time() end

---Use this for measuring elapsed durations — it cannot jump backwards.
---@return number # the monotonic time since boot in seconds (float).
function vespera.sys.uptime() end

---Raw clock_gettime interface.
---clk_id values are exposed as constants:
---vespera.CLOCK_REALTIME, vespera.CLOCK_MONOTONIC, vespera.CLOCK_PROCESS_CPUTIME_ID
---@param clk_id number
---@return number, number|nil, number? # sec, nsec  on success, or nil + error_code.
function vespera.sys.clock_gettime(clk_id) end

---Reboot or power off the system. Does not return on success. mode: 0=restart (default), 1=power_off, 2=halt
---@param mode? number
---@return never
function vespera.sys.reboot(mode) end

---sys.setenv(name, value [, overwrite]) sys.unsetenv(name) sys.environ()     — return copy of the environment as a table
---@param name string
---@return any
function vespera.sys.getenv(name) end

---@param name string
---@param value string
---@param overwrite? number
---@return any
function vespera.sys.setenv(name, value, overwrite) end

---@param name string
---@return any
function vespera.sys.unsetenv(name) end

---return copy of the environment as a table
---@return any
function vespera.sys.environ() end

-- ---------------------------------------------------------------------------
-- vespera.vbus  —  Virtual event bus
-- ---------------------------------------------------------------------------

---Virtual event bus
vespera.vbus = {}

---Subscribe to vbus signals matching interface/member. member = "" or omitted = all members on the interface. Example: vespera.vbus.subscribe(vespera.VBUS_IFACE_POWER, vespera.VBUS_SIG_BATTERY_CHANGED) After subscribing, use vbus.recv() or poll(HANDLE_VBUS, POLLIN) to wait.
---@param interface string
---@param member? string
---@return true|nil, number? # true on success, or nil + error_code.
function vespera.vbus.subscribe(interface, member) end

---Remove all subscriptions for the calling realm.
---@return any
function vespera.vbus.unsubscribe() end

---Read one vbus message from HANDLE_VBUS.
---{
---interface = string,
---member    = string,
---payload   = string (raw bytes, may be empty),
---}
---@return any|nil, number? # nil, error_code on failure.
function vespera.vbus.recv() end

---Returns: { percent, charging, present, critical, remaining_mwh, rate_mw, full_capacity_mwh, index }
---@return any
function vespera.vbus.recv_battery() end

---Convenience: receive an AC adapter status update.
---@return table|nil, number? # { online = bool } or nil + error.
function vespera.vbus.recv_ac() end

-- ---------------------------------------------------------------------------
-- vespera.log  —  Logging helpers
-- ---------------------------------------------------------------------------

---Logging helpers
vespera.log = {}

---Write a message to the kernel log (via HANDLE_STDERR), with newline.
---@param message string
---@return number|nil, number? # bytes_written on success, or nil + error_code.
function vespera.log.write(message) end

---Formatted variant — delegates to string.format, then calls log.write.
---@param fmt string
---@param ... any
---@return any
function vespera.log.writef(fmt, ...) end

---@param arg1 string
---@return any
function vespera.log.info(arg1) end

---@param arg1 string
---@return any
function vespera.log.warn(arg1) end

---@param arg1 string
---@return any
function vespera.log.error(arg1) end

---@param arg1 string
---@return any
function vespera.log.debug(arg1) end

return vespera
