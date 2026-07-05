# deploy_dlls.cmake
# 复制项目 PDB 调试符号到 exe 所在目录（文件不存在则跳过）
#
# 注意: vcpkg 第三方 DLL 由 vcpkg 的 applocal.ps1 自动部署，无需手动复制。
#
# 用法:
#   cmake -DTARGET_BIN=<exe_path>
#         -DPDB_FILES=<pdb1;pdb2;...>
#         -P deploy_dlls.cmake

cmake_minimum_required(VERSION 3.21)

if(NOT DEFINED TARGET_BIN)
    message(FATAL_ERROR "TARGET_BIN is required")
endif()

get_filename_component(_exe_dir "${TARGET_BIN}" DIRECTORY)

# 复制 PDB 调试符号（不存在则跳过，Release 模式可能不生成）
if(DEFINED PDB_FILES)
    foreach(_pdb ${PDB_FILES})
        if(EXISTS "${_pdb}")
            file(COPY "${_pdb}" DESTINATION "${_exe_dir}")
        endif()
    endforeach()
endif()
