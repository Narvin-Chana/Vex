#pragma once

#define VEX_VK_DECLARE_ENUM_MAPPING(VexEnumType, VexEnumName, VkEnumType, VkEnumName)                                  \
    VkEnumType VexEnumName##To##VkEnumName(VexEnumType val)
#define VEX_VK_BEGIN_ENUM_MAPPING(VexEnumType, VexEnumName, VkEnumType, VkEnumName)                                    \
    VEX_VK_DECLARE_ENUM_MAPPING(VexEnumType, VexEnumName, VkEnumType, VkEnumName)                                      \
    {                                                                                                                  \
        using enum VkEnumType;                                                                                         \
        using enum VexEnumType;                                                                                        \
        switch (val)                                                                                                   \
        {

#define VEX_VK_DECLARE_ENUM_MAPPING_FLAGS(VexEnumType, VexEnumName, VkEnumType, VkEnumName)                            \
    VkEnumType##Flags VexEnumName##To##VkEnumName(VexEnumType val)
#define VEX_VK_BEGIN_ENUM_MAPPING_FLAGS(VexEnumType, VexEnumName, VkEnumType, VkEnumName)                              \
    VEX_VK_DECLARE_ENUM_MAPPING_FLAGS(VexEnumType, VexEnumName, VkEnumType, VkEnumName)                                \
    {                                                                                                                  \
        using enum VkEnumType##FlagBits;                                                                               \
        using enum VexEnumType;                                                                                        \
        switch (val)                                                                                                   \
        {

#define VEX_VK_END_ENUM_MAPPING                                                                                        \
    default:                                                                                                           \
        VEX_LOG(Fatal, "Enum mapping not supported.");                                                                 \
        }                                                                                                              \
        std::unreachable();                                                                                            \
        }

#define VEX_VK_ENUM_MAPPING_ENTRY(VexValue, VkValue)                                                                   \
    case VexValue:                                                                                                     \
        return VkValue;

#define VEX_VK_BEGIN_ENUM_MAPPING_STATIC(VexEnumType, VexEnumName, VkEnumType, VkEnumName)                             \
    static VEX_VK_BEGIN_ENUM_MAPPING(VexEnumType, VexEnumName, VkEnumType, VkEnumName)
#define VEX_VK_BEGIN_ENUM_MAPPING_FLAGS_STATIC(VexEnumType, VexEnumName, VkEnumType, VkEnumName)                       \
    static VEX_VK_BEGIN_ENUM_MAPPING_FLAGS(VexEnumType, VexEnumName, VkEnumType, VkEnumName)
