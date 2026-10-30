project "Assimp"
    location "../build/Assimp"
    kind "Utility"
    prebuildmessage "Building Assimp (%{cfg.buildcfg})"
    prebuildcommands {
        'powershell -NoProfile -ExecutionPolicy Bypass -File "' .. path.getabsolute("../tools/BuildAssimp.ps1") .. '" -Configuration %{cfg.buildcfg}'
    }
