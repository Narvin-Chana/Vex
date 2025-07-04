dxc.exe -E CSMain -spirv -T cs_6_6 shader.hlsl > shader.spv
spirv-as.exe shader.spv -o shader_bytecode.spvasm
del shader.spv