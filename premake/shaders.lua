local shaderOutputDir = path.join(_MAIN_SCRIPT_DIR, "bin/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/Iskra/shaders")
local shaderCompiler = path.join(_MAIN_SCRIPT_DIR, "tools/CompileShader.ps1")
local shaderInputs = os.matchfiles(path.join(_MAIN_SCRIPT_DIR, "Iskra/shaders/**.hlsli"))
table.insert(shaderInputs, shaderCompiler)

defines { 'ISKRA_SHADER_DIR="' .. shaderOutputDir .. '"' }

-- Graphics shaders currently use VSMain and PSMain in the same HLSL file.
filter "files:**.hlsl"
    buildmessage "Compiling %{file.name}"
    buildcommands {
        'powershell -NoProfile -ExecutionPolicy Bypass -File "' .. shaderCompiler .. '" -Source "%{file.abspath}" -OutputDirectory "' .. shaderOutputDir .. '" -Configuration %{cfg.buildcfg}'
    }
    buildoutputs {
        shaderOutputDir .. "/%{file.basename}.vs.dxil",
        shaderOutputDir .. "/%{file.basename}.ps.dxil"
    }
    buildinputs(shaderInputs)
filter {}
