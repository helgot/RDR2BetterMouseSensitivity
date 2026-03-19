#include "helper.hpp"

#include <ShlObj.h>
#include <stdio.h>
#include <windows.h>

#include "logger.hpp"

static char *read_file_into_buffer(const wchar_t *filepath);

GraphicsAPI get_graphics_api_from_settings_file()
{
    GraphicsAPI result = GRAPHICS_API_UNKNOWN;
    PWSTR documents_path = NULL;

    if (SUCCEEDED(
            SHGetKnownFolderPath(FOLDERID_Documents, 0, NULL, &documents_path)))
    {
        wchar_t settings_file_path[MAX_PATH];

        swprintf(
            settings_file_path, MAX_PATH,
            L"%s\\Rockstar Games\\Red Dead Redemption 2\\Settings\\system.xml",
            documents_path);

        CoTaskMemFree(documents_path);

        LOG_DEBUG("%s: Reading settings file at %ws", __func__,
                  settings_file_path);

        char *settings_file_content = read_file_into_buffer(settings_file_path);

        if (!settings_file_content)
        {
            LOG_DEBUG("%s: Failed to read settings file.", __func__);
            return GRAPHICS_API_UNKNOWN;
        }

        if (strstr(settings_file_content, "kSettingAPI_Vulkan"))
        {
            result = GRAPHICS_API_VULKAN;
            LOG_DEBUG("%s: Vulkan API configured in settings file.", __func__);
        }
        else if (strstr(settings_file_content, "kSettingAPI_DX12"))
        {
            result = GRAPHICS_API_DX12;
            LOG_DEBUG("%s: DX12 API configured in settings file.", __func__);
        }
        else
        {
            LOG_DEBUG(
                "%s: Could not determine Graphics API from settings file.",
                __func__);
        }

        free(settings_file_content);
    }
    else
    {
        LOG_ERROR("%s: Could not find user Documents folder.", __func__);
    }

    return result;
}

static char *read_file_into_buffer(const wchar_t *filepath)
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
