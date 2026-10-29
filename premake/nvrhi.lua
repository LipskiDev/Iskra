project "NVRHI"
    location "../build/NVRHI"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    includedirs { "../external/nvrhi/include", "../external/DirectX-Headers/include" }
    defines {
        "NOMINMAX", "NVRHI_WITH_AFTERMATH=0",
        "NVRHI_D3D12_WITH_NVAPI=0", "NVRHI_D3D12_WITH_DXRCLUSTER=0",
        "NVRHI_D3D12_WITH_DXR12_OPACITY_MICROMAP=0"
    }
    files {
        "../external/nvrhi/include/**.h", "../external/nvrhi/src/common/**.h",
        "../external/nvrhi/src/common/**.cpp", "../external/nvrhi/src/validation/**.h",
        "../external/nvrhi/src/validation/**.cpp", "../external/nvrhi/src/d3d12/**.h",
        "../external/nvrhi/src/d3d12/**.cpp", "../external/DirectX-Headers/src/dxguids.cpp",
        "../external/nvrhi/tools/nvrhi.natvis"
    }

