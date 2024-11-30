include(FetchContent)
if(HSBA_USE_BIT7Z)
    # download and unpack bit7z
    # bit7z use CPM to download 7-zip，so we need to use FetchContent to download it instead of vcpkg
    # use git_tag hash to avoid download every time    
    FetchContent_Declare(
        bit7z
        GIT_REPOSITORY https://github.com/rikyoz/bit7z
        GIT_TAG bec6a2206ee4032c6a424b41ee39771e155f5ea9 #v4.0.8
    )
    # add bit7z option BIT7Z_AUTO_FORMAT=ON,ect
    FetchContent_GetProperties(bit7z)
    if(NOT bit7z_POPULATED)
        FetchContent_Populate(bit7z)
        # add bit7z option BIT7Z_AUTO_FORMAT=ON,ect
        set(BIT7Z_AUTO_FORMAT ON CACHE BOOL "" FORCE)
        set(BIT7Z_USE_STD_BYTE ON CACHE BOOL "" FORCE)
        set(BIT7Z_DISABLE_ZIP_ASCII_PWD_CHECK ON CACHE BOOL "" FORCE)
        add_subdirectory(${bit7z_SOURCE_DIR} ${bit7z_BINARY_DIR})
    endif()
    FetchContent_makeAvailable(bit7z)
endif()