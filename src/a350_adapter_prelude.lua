-- A320 host state only. The extracted file registers no simulator callbacks,
-- commands, FFI, brake writes or download processes.
local D2R, R_EARTH = math.pi / 180, 6371000
local S = {marks = {}}
local CFG = {guess_terms = false}
local FMS = {}
function FMS.rwy_end(apt)
    if not apt or apt.icao ~= FMS.airport or not FMS.runway or FMS.runway == '' then return nil end
    local function norm(id)
        local n, side = tostring(id):match('^0?(%d+)([LCR]?)$')
        if n then return string.format('%02d%s', tonumber(n), side) end
    end
    local wanted = norm(FMS.runway)
    if not wanted then return nil end
    for _, e in ipairs(apt.ends or {}) do
        if norm(e.id) == wanted then return e end
    end
end
local DR = {lat = {[0] = 0}, lon = {[0] = 0}, psi = {[0] = 0}, mag = {[0] = 0}}
local SCRIPTS = ''
local bit = {band = function(a,b) return a & b end,
             bor = function(a,b) return a | b end,
             lshift = function(a,b) return a << b end,
             rshift = function(a,b) return a >> b end}
math.atan2 = math.atan
local function pnl_log(s) error(s) end
