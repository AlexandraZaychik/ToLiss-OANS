-- One-shot, read-only SDK probe. Run inside FlyWithLua, on the simulator thread.
-- Does not register callbacks or write flight-plan entries/datarefs.
local ok, err = pcall(function()
    local ffi = require("ffi")
    ffi.cdef[[
        int XPLMCountFMSEntries(void);
        int XPLMCountFMSFlightPlanEntries(int plan);
        int XPLMGetDisplayedFMSEntry(void);
        int XPLMGetDestinationFMSEntry(void);
        void XPLMGetFMSEntryInfo(int index, int *type, char *id, int *ref,
                               int *altitude, float *lat, float *lon);
    ]]
    local sdk = ffi.load("XPLM_64")
    local path = "E:/python_project/toliss320ehancement/reports/runtime/native-fms-20261027.txt"
    local f = assert(io.open(path, "w"))
    f:write("captured_local=", os.date("%Y-%m-%dT%H:%M:%S"), "\n")
    f:write("source=XPLMCountFMSEntries / XPLMGetFMSEntryInfo\n")
    local count = sdk.XPLMCountFMSEntries()
    f:write("count=", count, "\ndisplayed_index=", sdk.XPLMGetDisplayedFMSEntry(),
            "\ndestination_index=", sdk.XPLMGetDestinationFMSEntry(), "\n")
    f:write("index\ttype\tid\tnav_ref\taltitude_ft\tlatitude\tlongitude\n")
    for i = 0, math.min(count, 10000) - 1 do
        local id = ffi.new("char[256]")
        local typ, ref, alt = ffi.new("int[1]"), ffi.new("int[1]", -1), ffi.new("int[1]")
        local lat, lon = ffi.new("float[1]"), ffi.new("float[1]")
        sdk.XPLMGetFMSEntryInfo(i, typ, id, ref, alt, lat, lon)
        f:write(string.format("%d\t%d\t%s\t%d\t%d\t%.8f\t%.8f\n",
            i, tonumber(typ[0]), ffi.string(id), tonumber(ref[0]), tonumber(alt[0]),
            tonumber(lat[0]), tonumber(lon[0])))
    end
    f:write("count_after=", sdk.XPLMCountFMSEntries(), "\n")
    local plans = {"pilot_primary", "copilot_primary", "pilot_approach",
                   "copilot_approach", "pilot_temporary", "copilot_temporary"}
    for i, name in ipairs(plans) do
        local available, n = pcall(function() return sdk.XPLMCountFMSFlightPlanEntries(i - 1) end)
        f:write("plan_", name, "_count=", available and tostring(n) or "unavailable", "\n")
    end
    f:close()
    logMsg("Native FMS read-only probe saved: " .. path)
end)
if not ok then logMsg("Native FMS read-only probe failed: " .. tostring(err)) end
