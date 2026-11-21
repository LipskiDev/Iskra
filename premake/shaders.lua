local shaderOutputDir = path.join(_MAIN_SCRIPT_DIR, "bin/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/Iskra/shaders")
local shaderCompiler = path.join(_MAIN_SCRIPT_DIR, "tools/CompileShader.py")
local shaderInputs = os.matchfiles(path.join(_MAIN_SCRIPT_DIR, "Iskra/shaders/**.hlsli"))
table.insert(shaderInputs, shaderCompiler)

defines { 'ISKRA_SHADER_DIR="' .. shaderOutputDir .. '"' }

local supportedStages = { vs = true, ps = true, gs = true, hs = true, ds = true, cs = true, ms = true, as = true, lib = true }

-- Read the same declaration as the compiler so MSBuild tracks the actual outputs.
for _, source in ipairs(os.matchfiles(path.join(_MAIN_SCRIPT_DIR, "Iskra/**.hlsl"))) do
    local file = assert(io.open(source, "r"))
    local firstLine = (file:read("*l") or ""):gsub("^\239\187\191", "")
    file:close()
    local declaration = firstLine:match("^%s*//%s*@stages%s+(.+)%s*$")
    filter { "files:**/" .. path.getrelative(path.join(_MAIN_SCRIPT_DIR, "Iskra"), source) }
        if declaration then
            local outputs = {}
            local seen = {}
            for stage in declaration:gmatch("%S+") do
                assert(supportedStages[stage], source .. ": unknown shader stage " .. stage)
                assert(not seen[stage], source .. ": duplicate shader stage " .. stage)
                seen[stage] = true
                table.insert(outputs, shaderOutputDir .. "/%{file.basename}." .. stage .. ".dxil")
            end
            assert(#outputs > 0, source .. ": empty @stages declaration")
            buildmessage "Compiling %{file.name}"
            buildcommands {
                'python "' .. shaderCompiler .. '" "%{file.abspath}" --output-directory "' .. shaderOutputDir .. '" --configuration %{cfg.buildcfg}'
            }
            buildoutputs(outputs)
            buildinputs(shaderInputs)
        else
            assert(not firstLine:find("@stages", 1, true), source .. ": malformed @stages declaration")
            -- Unannotated shader files may be unfinished or included by other shaders.
            buildaction "None"
        end
end
filter {}
