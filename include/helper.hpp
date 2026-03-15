#pragma once

enum GraphicsAPI
{
    GRAPHICS_API_DX12,
    GRAPHICS_API_VULKAN,
    GRAPHICS_API_UNKNOWN
};

GraphicsAPI GetGraphicsApiFromSettingsFile();