#include "helper.hpp"

#include <ShlObj.h>
#include <stdio.h>
#include <windows.h>

#include "logger.hpp"

static char *ReadFileToString(const wchar_t *filepath);

GraphicsAPI GetGraphicsApiFromSettingsFile()
{
    GraphicsAPI Result = GRAPHICS_API_UNKNOWN;
    PWSTR DocumentsPath = NULL;

    if (SUCCEEDED(
            SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &DocumentsPath)))
    {
        wchar_t SettingsFilePath[MAX_PATH];

        swprintf(
            SettingsFilePath, MAX_PATH,
            L"%s\\Rockstar Games\\Red Dead Redemption 2\\Settings\\system.xml",
            DocumentsPath);

        CoTaskMemFree(DocumentsPath);

        LOG_DEBUG("%s: Reading settings file at %ws", __func__,
                  SettingsFilePath);

        char *SettingsFileContent = ReadFileToString(SettingsFilePath);

        if (!SettingsFileContent)
        {
            LOG_DEBUG("%s: Failed to read settings file.", __func__);
            return GRAPHICS_API_UNKNOWN;
        }

        if (strstr(SettingsFileContent, "kSettingAPI_Vulkan"))
        {
            Result = GRAPHICS_API_VULKAN;
            LOG_DEBUG("%s: Vulkan API configured in settings file.", __func__);
        }
        else if (strstr(SettingsFileContent, "kSettingAPI_DX12"))
        {
            Result = GRAPHICS_API_DX12;
            LOG_DEBUG("%s: DX12 API configured in settings file.", __func__);
        }
        else
        {
            LOG_DEBUG(
                "%s: Could not determine Graphics API from settings file.",
                __func__);
        }

        free(SettingsFileContent);
    }
    else
    {
        LOG_ERROR("%s: Could not find user Documents folder.", __func__);
    }

    return Result;
}

static char *ReadFileToString(const wchar_t *filepath)
{
    FILE *file = _wfopen(filepath, L"rb");
    if (!file)
        return NULL;

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    char *buffer = (char *)malloc(size + 1);
    if (!buffer)
    {
        fclose(file);
        return NULL;
    }

    fread(buffer, 1, size, file);
    buffer[size] = '\0';

    fclose(file);
    return buffer;
}
