-- vespera.lua
-- Auto-generated LuaLS stub for the VesperaOS vespera module.
-- DO NOT EDIT MANUALLY - regenerate with gen_stubs.py.

---@meta

---VesperaOS system module - provides Lua bindings for kernel syscalls.
local vespera = {}

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
vespera.SIGKILL = SIGKILL
---@type number
vespera.SIGTERM = SIGTERM
---@type number
vespera.SIGINT = SIGINT
---@type number
vespera.POLLIN = POLLIN
---@type number
vespera.POLLOUT = POLLOUT
---@type number
vespera.POLLERR = POLLERR
---@type number
vespera.POLLHUP = POLLHUP
---@type number
vespera.O_RDONLY = O_RDONLY
---@type number
vespera.O_WRONLY = O_WRONLY
---@type number
vespera.O_RDWR = O_RDWR

-- ---------------------------------------------------------------------------
-- Process
-- ---------------------------------------------------------------------------

---Spawns a new process (realm) with the given executable path.
---@param path string Path to executable
---@param args? table Optional table of command-line arguments
---@param env? table Optional table of environment variables
---@return number|nil, number? # Unit handle on success, or nil + error code on failure
function vespera.spawn(path, args, env) end

---Terminate the current unit.
---@param code? number Optional exit code (default: 0)
---@return never
function vespera.exit(code) end

---Wait for a realm to terminate.
---@param handle number Realm handle to wait for
---@return number|nil, number? # Exit code on success, or nil + error code
function vespera.wait(handle) end

---Send a signal to a unit/realm.
---@param handle number Target handle
---@param signal number Signal number (default: 9/SIGKILL)
---@return true|nil, number? # true on success, or nil + error code
function vespera.kill(handle, signal) end

---Get the current realm and unit IDs.
---@return number, number # realm_id, unit_id
function vespera.getrid() end

-- ---------------------------------------------------------------------------
-- File I/O
-- ---------------------------------------------------------------------------

---Open a file.
---@param path string File path
---@param mode string Open mode ("r", "w", "rw", etc.)
---@return number|nil, number? # File handle on success, or nil + error code
function vespera.open(path, mode) end

---Close a file handle.
---@param handle number File handle
---@return true|nil, number? # true on success, or nil + error code
function vespera.close(handle) end

---Read data from a file handle.
---@param handle number File handle
---@param count number Maximum bytes to read
---@return string|nil, number? # Data string on success, or nil + error code
function vespera.read(handle, count) end

---Write data to a file handle.
---@param handle number File handle
---@param data string String to write
---@return number|nil, number? # Bytes written on success, or nil + error code
function vespera.write(handle, data) end

---Seek to a position in a file.
---@param handle number File handle
---@param offset number Byte offset
---@param whence number SEEK_SET, SEEK_CUR, or SEEK_END
---@return any # New position on success, or nil + error code
function vespera.seek(handle, offset, whence) end

-- ---------------------------------------------------------------------------
-- Filesystem
-- ---------------------------------------------------------------------------

---Create a directory.
---@param path string Directory path
---@return true|nil, number? # true on success, or nil + error code
function vespera.mkdir(path) end

---Remove a file.
---@param path string File path
---@return true|nil, number? # true on success, or nil + error code
function vespera.unlink(path) end

---Remove a directory.
---@param path string Directory path
---@return true|nil, number? # true on success, or nil + error code
function vespera.rmdir(path) end

---Rename a file or directory.
---@param oldpath string Old path
---@param newpath string New path
---@return true|nil, number? # true on success, or nil + error code
function vespera.rename(oldpath, newpath) end

---Change current working directory.
---@param path string Directory path
---@return true|nil, number? # true on success, or nil + error code
function vespera.chdir(path) end

---Get current working directory.
---@return string|nil, number? # Path string on success, or nil + error code
function vespera.getcwd() end

---Get file/directory metadata.
---@param path string File path
---@return table|nil, number? # Table with stat info, or nil + error code
function vespera.stat(path) end

---Read directory contents - returns an iterator.
---@param path string Directory path
---@return fun():string # Iterator function
function vespera.readdir(path) end

-- ---------------------------------------------------------------------------
-- System
-- ---------------------------------------------------------------------------

---Sleep for the specified number of seconds.
---@param seconds number Sleep duration (can be fractional)
---@return true|nil, number? # true on success, or nil + error code
function vespera.sleep(seconds) end

---Get current time.
---@return number|nil, number? # Seconds since epoch
function vespera.time() end

---Get monotonic time since system boot. Suitable for measuring elapsed time and timeouts. Unlike time(), this clock cannot jump backwards.
---@return number|nil, number? # Seconds since boot as a float (millisecond resolution)
function vespera.uptime() end

---Reboot the system.
---@return never
function vespera.reboot() end

---Mount a filesystem.
---@param fstype string Filesystem type ("devfs", "ext4", etc.)
---@param path string Mount point
---@param device? string Optional device path
---@return true|nil, number? # true on success, or nil + error code
function vespera.mount(fstype, path, device) end

---Unmount a filesystem.
---@param path string Mount point
---@return true|nil, number? # true on success, or nil + error code
function vespera.umount(path) end

-- ---------------------------------------------------------------------------
-- Logging
-- ---------------------------------------------------------------------------

---Write a message to the kernel log (via stderr handle).
---@param message string Log message
---@return number|nil, number? # bytes written on success, or nil + error code
function vespera.log(message) end

---Write a formatted message to the kernel log.
---@param format string Format string
---@param ... any Format arguments
---@return true|nil, number? # true on success
function vespera.logf(format, ...) end

-- ---------------------------------------------------------------------------
-- IPC
-- ---------------------------------------------------------------------------

---Create an IPC channel.
---@param capacity? number Optional buffer capacity (default: 4096)
---@return number|nil, number? # Channel handle on success, or nil + error code
function vespera.channel_create(capacity) end

---Send data on a channel.
---@param handle number Channel handle
---@param data string Data to send
---@return number|nil, number? # Bytes sent on success, or nil + error code
function vespera.channel_send(handle, data) end

---Receive data from a channel.
---@param handle number Channel handle
---@param size number Maximum bytes to receive
---@return string|nil, number? # Data string on success, or nil + error code
function vespera.channel_recv(handle, size) end

---Create a pipe.
---@return number|nil, number? # read_handle, write_handle on success, or nil + error code
function vespera.pipe() end

---Wait for events on handles.
---@param handles number Table of handle IDs to poll
---@param timeout_ms number Timeout in milliseconds (-1 = infinite)
---@return number|nil, number? # Table of ready handles, or nil + error code
function vespera.poll(handles, timeout_ms) end

return vespera
