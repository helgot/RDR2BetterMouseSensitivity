#include "renderer_hook_vulkan.hpp"

#include <MinHook.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_win32.h>
#include <vector>
#include <vulkan/vulkan.h>
#include <windows.h>

#include "logger.hpp"
#include "user_interface.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd,
                                                             UINT msg,
                                                             WPARAM wParam,
                                                             LPARAM lParam);

extern bool MENU_VISIBLE;

// Vulkan function pointers:
PFN_vkQueuePresentKHR OriginalVkQueuePresent = nullptr;
PFN_vkCreateDevice OriginalVkCreateDevice = nullptr;
PFN_vkGetDeviceProcAddr OriginalVkGetDeviceProcAddr = nullptr;
PFN_vkCreateSwapchainKHR OriginalVkCreateSwapchain = nullptr;
PFN_vkDestroySwapchainKHR OriginalVkDestroySwapchain = nullptr;
PFN_vkCreateInstance OriginalVkCreateInstance = nullptr;

// Global Vulkan objects captured from the game
static VkDevice Device = VK_NULL_HANDLE;
static VkQueue Queue = VK_NULL_HANDLE;
static VkInstance Instance = VK_NULL_HANDLE;
static VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;

// ImGui related Vulkan objects
static VkDescriptorPool ImguiDescriptorPool = VK_NULL_HANDLE;
static VkRenderPass ImguiRenderPass = VK_NULL_HANDLE;
static std::vector<VkCommandPool> ImguiCommandPools;
static std::vector<VkCommandBuffer> ImguiCommandBuffers;
static std::vector<VkFramebuffer> ImguiFramebuffers;
static std::vector<VkImage> SwapchainImages;
static std::vector<VkImageView> SwapchainImageViews;
static VkFormat SwapchainImageFormat = VK_FORMAT_UNDEFINED;
static VkExtent2D SwapchainExtent = VKAPI_ATTR VkExtent2D{0, 0};
static VkSwapchainKHR Swapchain = VK_NULL_HANDLE;

// Synchronization objects for ImGui rendering
static std::vector<VkSemaphore> RenderCompleteSemaphores;
static std::vector<VkFence> RenderFences;
static bool ImguiInitialized = false;
static bool VulkanDeviceCreated = false;
// Win32 Objects:
static HWND WindowHandle = nullptr;
static WNDPROC OriginalWindowProc = nullptr;

constexpr bool kVkEnableValidationLayers = false;

static bool HookVulkanFunction(HMODULE VulkanLib,
                               const char *TargetFunctionName,
                               LPVOID DetourFunction,
                               LPVOID *OriginalFunctionPtr);

// Vulkan initialization and cleanup:
static VkResult CreateRenderPass();
static VkResult CreateDescriptorPool();

// ImGui initialization and cleanup:
static void InitializeImGui();
static void DeinitializeImGui();

// Hooked Vulkan functions:
static VkResult VKAPI_PTR
Hooked_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *pPresentInfo);
static VkResult VKAPI_PTR Hooked_vkCreateDevice(
    VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkDevice *pDevice);
static VkResult VKAPI_PTR Hooked_vkCreateSwapchainKHR(
    VkDevice device, const VkSwapchainCreateInfoKHR *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkSwapchainKHR *pSwapchain);
static void VKAPI_PTR
Hooked_vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR pSwapchain,
                             const VkAllocationCallbacks *pAllocator);
static VkResult VKAPI_PTR Hooked_vkCreateInstance(
    const VkInstanceCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkInstance *pInstance);
static PFN_vkVoidFunction VKAPI_PTR
Hooked_vkGetDeviceProcAddr(VkDevice device, const char *pName);

// Hooked WndProc function:
static LRESULT CALLBACK Hooked_WndProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                       LPARAM lPARAM);

bool vulkan_hook_install()
{
    HMODULE VulkanHandle = GetModuleHandle("vulkan-1.dll");
    if (!VulkanHandle)
    {
        LOG_DEBUG("Failed to get handle for vulkan-1.dll!");
        MH_Uninitialize();
        return false;
    }

    if (!HookVulkanFunction(VulkanHandle, "vkCreateInstance",
                            (LPVOID)Hooked_vkCreateInstance,
                            (LPVOID *)&OriginalVkCreateInstance))
        return false;
    if (!HookVulkanFunction(VulkanHandle, "vkCreateDevice",
                            (LPVOID)Hooked_vkCreateDevice,
                            (LPVOID *)&OriginalVkCreateDevice))
        return false;
    if (!HookVulkanFunction(VulkanHandle, "vkGetDeviceProcAddr",
                            (LPVOID)Hooked_vkGetDeviceProcAddr,
                            (LPVOID *)&OriginalVkGetDeviceProcAddr))
        return false;

    LOG_DEBUG("Vulkan API hooks installed successfully.");
    return true;
}

void vulkan_hook_shutdown()
{
    DeinitializeImGui();
    MH_DisableHook((LPVOID)OriginalVkCreateInstance);
    MH_DisableHook((LPVOID)OriginalVkCreateDevice);
    MH_DisableHook((LPVOID)OriginalVkGetDeviceProcAddr);

    LOG_DEBUG("Vulkan Hooks Disabled.");
}

PFN_vkVoidFunction VKAPI_PTR Hooked_vkGetDeviceProcAddr(VkDevice device,
                                                        const char *pName)
{
    LOG_DEBUG("vkGetDeviceProcAddr requested: %s", pName);

    // Intercept requests for specific functions and return our hooks instead.
    if (strcmp(pName, "vkCreateSwapchainKHR") == 0)
    {
        if (!OriginalVkCreateSwapchain)
        {
            OriginalVkCreateSwapchain =
                (PFN_vkCreateSwapchainKHR)OriginalVkGetDeviceProcAddr(device,
                                                                      pName);
        }
        return (PFN_vkVoidFunction)Hooked_vkCreateSwapchainKHR;
    }
    if (strcmp(pName, "vkDestroySwapchainKHR") == 0)
    {
        if (!OriginalVkDestroySwapchain)
        {
            OriginalVkDestroySwapchain =
                (PFN_vkDestroySwapchainKHR)OriginalVkGetDeviceProcAddr(device,
                                                                       pName);
        }
        return (PFN_vkVoidFunction)Hooked_vkDestroySwapchainKHR;
    }
    if (strcmp(pName, "vkQueuePresentKHR") == 0)
    {
        if (!OriginalVkQueuePresent)
        {
            OriginalVkQueuePresent =
                (PFN_vkQueuePresentKHR)OriginalVkGetDeviceProcAddr(device,
                                                                   pName);
        }
        return (PFN_vkVoidFunction)Hooked_vkQueuePresentKHR;
    }

    // For all other functions, pass the request to the original.
    return OriginalVkGetDeviceProcAddr(device, pName);
}

static bool HookVulkanFunction(HMODULE VulkanLib,
                               const char *TargetFunctionName,
                               LPVOID DetourFunction,
                               LPVOID *OriginalFunctionPtr)
{
    // Hook vkCreateInstance (instance-level function, usually loaded directly)
    PFN_vkCreateInstance TargetFunction =
        (PFN_vkCreateInstance)GetProcAddress(VulkanLib, TargetFunctionName);
    if (!TargetFunction)
    {
        LOG_DEBUG("Failed to get address of %s.", TargetFunctionName);
        MH_Uninitialize();
        return false;
    }

    if (MH_CreateHook((LPVOID)TargetFunction, DetourFunction,
                      OriginalFunctionPtr) != MH_OK)
    {
        LOG_DEBUG("Failed to create hook for %s!", TargetFunctionName);
        LOG_DEBUG("Address of target function: %p", (void *)(TargetFunction));
        MH_Uninitialize();
        return false;
    }
    if (MH_EnableHook((LPVOID)TargetFunction) != MH_OK)
    {
        LOG_DEBUG("Failed to enable hook for %s!", TargetFunctionName);
        MH_Uninitialize();
        return false;
    }
    LOG_DEBUG("Hook for %s enabled.", TargetFunctionName);

    LOG_DEBUG("Address of target function: %p", (void *)TargetFunction);
    LOG_DEBUG("Address of detour function: %p", (void *)DetourFunction);
    LOG_DEBUG("Address of original function: %p",
              (void *)(*OriginalFunctionPtr));

    return true;
}

static VkResult CreateRenderPass()
{
    LOG_DEBUG("%s: Called.", __func__);
    LOG_DEBUG("%s: vkDevice at entry: %p", __func__,
              (void *)Device); // LOG_DEBUG g_vkDevice value

    VkAttachmentDescription ColorAttachment{};
    ColorAttachment.format = SwapchainImageFormat;
    ColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    // We MUST load the game's content to render over it.
    ColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    ColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    ColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    ColorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Tell the pass to transition the image to the layout needed for
    // presentation when we are done.
    ColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference ColorAttachmentRef{};
    ColorAttachmentRef.attachment = 0;
    ColorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription Subpass{};
    Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    Subpass.colorAttachmentCount = 1;
    Subpass.pColorAttachments = &ColorAttachmentRef;

    VkSubpassDependency Dependency{};
    Dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    Dependency.dstSubpass = 0;
    Dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    Dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    Dependency.srcAccessMask = 0;
    Dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo RenderPassInfo{};
    RenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    RenderPassInfo.attachmentCount = 1;
    RenderPassInfo.pAttachments = &ColorAttachment;
    RenderPassInfo.subpassCount = 1;
    RenderPassInfo.pSubpasses = &Subpass;
    RenderPassInfo.dependencyCount = 1;
    RenderPassInfo.pDependencies = &Dependency;

    if (Device == VK_NULL_HANDLE)
    {
        LOG_DEBUG("%s: Error: Vulkan device is not initialized. Cannot create "
                  "ImGui render pass.",
                  __func__);
        return VkResult::VK_ERROR_DEVICE_LOST;
    }
    LOG_DEBUG("%s: Attempting to create render pass with VkDevice: %p",
              __func__, (void *)Device);
    return vkCreateRenderPass(Device, &RenderPassInfo, nullptr,
                              &ImguiRenderPass);
}

static VkResult CreateDescriptorPool()
{
    VkDescriptorPoolSize PoolSizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};
    VkDescriptorPoolCreateInfo PoolInfo = {};
    PoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    PoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    PoolInfo.maxSets = 1000 * IM_ARRAYSIZE(PoolSizes);
    PoolInfo.poolSizeCount = (uint32_t)IM_ARRAYSIZE(PoolSizes);
    PoolInfo.pPoolSizes = PoolSizes;

    return vkCreateDescriptorPool(Device, &PoolInfo, nullptr,
                                  &ImguiDescriptorPool);
}

static void InitializeImGui()
{
    LOG_DEBUG("%s: Called.", __func__);
    if (ImguiInitialized)
    {
        LOG_DEBUG("%s: ImGui already initialized.", __func__);
        return;
    }

    if (Instance == VK_NULL_HANDLE || Device == VK_NULL_HANDLE ||
        PhysicalDevice == VK_NULL_HANDLE || Queue == VK_NULL_HANDLE ||
        SwapchainImages.empty() || ImguiRenderPass == VK_NULL_HANDLE)
    {
        LOG_DEBUG(
            "%s: ImGui Vulkan backend initialization skipped: missing required "
            "Vulkan objects (Instance: %p, PhysicalDevice: %p, Device: %p, "
            "Queue: %p, SwapchainImages count: %zu, RenderPass: %p).",
            __func__, (void *)Instance, (void *)PhysicalDevice, (void *)Device,
            (void *)Queue, SwapchainImages.size(), (void *)ImguiRenderPass);
        return;
    }

    LOG_DEBUG("Initializing ImGui Vulkan backend...");

    // 2. Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &IO = ImGui::GetIO();
    IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    IO.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    IO.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    IO.MouseDrawCursor = MENU_VISIBLE;

    ImGui::StyleColorsDark();
    {
        WindowHandle = FindWindow(NULL, "Red Dead Redemption 2");
        if (WindowHandle == NULL)
        {
            LOG_DEBUG("Could not find game window handle. Make sure the game "
                      "is running and the window title is correct.");
            return;
        }
        else
        {
            LOG_DEBUG("Found game window handle: %p", WindowHandle);
            // Replace the WndProc and store the original one
            OriginalWindowProc = (WNDPROC)SetWindowLongPtrA(
                WindowHandle, GWLP_WNDPROC, (LONG_PTR)Hooked_WndProc);
            if (OriginalWindowProc)
            {
                LOG_DEBUG("Successfully hooked WndProc.");
            }
            else
            {
                LOG_DEBUG("Failed to hook WndProc!");
            }
        }
        ImGui_ImplWin32_Init(WindowHandle);
    }

    if (ImguiDescriptorPool == VK_NULL_HANDLE)
    {
        if (CreateDescriptorPool() != VK_SUCCESS)
        {
            LOG_DEBUG("Error: Failed to create ImGui descriptor pool!");
            return;
        }
    }

    ImGui_ImplVulkan_InitInfo InitInfo = {};
    InitInfo.Instance = Instance;
    InitInfo.PhysicalDevice = PhysicalDevice;
    InitInfo.Device = Device;
    // Assuming queue family 0 is graphics and present. You might need to find
    // the correct one dynamically.
    InitInfo.QueueFamily = 0;
    InitInfo.Queue = Queue;
    InitInfo.PipelineCache = VK_NULL_HANDLE; // Can be VK_NULL_HANDLE
    InitInfo.DescriptorPool = ImguiDescriptorPool;
    InitInfo.Subpass = 0;
    InitInfo.MinImageCount = (uint32_t)SwapchainImages.size();
    InitInfo.ImageCount = (uint32_t)SwapchainImages.size();
    InitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    InitInfo.Allocator = nullptr;
    InitInfo.CheckVkResultFn = [](VkResult VulkanError) {
        if (VulkanError != VK_SUCCESS)
        {
            LOG_DEBUG("Vulkan ImGui Error: VkResult = %d", VulkanError);
        }
    };
    InitInfo.RenderPass = ImguiRenderPass;
    // Set API Version (0 will default to ImGui's internal default, typically
    // VK_API_VERSION_1_0)
    InitInfo.ApiVersion = 0; // Or VK_API_VERSION_1_0, VK_API_VERSION_1_1, etc.
    // We are using a fixed RenderPass, so dynamic rendering is not enabled.
    InitInfo.UseDynamicRendering = false;

    ImGui_ImplVulkan_Init(&InitInfo);

    ImguiInitialized = true;
    LOG_DEBUG("ImGui Vulkan backend initialized successfully.");
}

static void DeinitializeImGui()
{
    if (!ImguiInitialized)
    {
        return;
    }

    LOG_DEBUG("Shutting down ImGui Vulkan backend...");

    vkDeviceWaitIdle(Device);

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplWin32_Shutdown();

    ImGui::DestroyContext();

    if (ImguiRenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(Device, ImguiRenderPass, nullptr);
        ImguiRenderPass = VK_NULL_HANDLE;
    }
    for (auto &Framebuffer : ImguiFramebuffers)
    {
        if (Framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(Device, Framebuffer, nullptr);
        }
    }
    ImguiFramebuffers.clear();
    for (auto &ImageView : SwapchainImageViews)
    {
        if (ImageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(Device, ImageView, nullptr);
        }
    }
    SwapchainImageViews.clear();

    // Destroy command pools
    for (int ImageIdx = 0; ImageIdx < ImguiCommandPools.size(); ++ImageIdx)
    {
        if (ImguiCommandPools[ImageIdx] != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(Device, ImguiCommandPools[ImageIdx], nullptr);
            if (ImguiCommandBuffers[ImageIdx] != VK_NULL_HANDLE)
            {
                vkFreeCommandBuffers(Device, ImguiCommandPools[ImageIdx], 1,
                                     &ImguiCommandBuffers[ImageIdx]);
            }
        }
    }
    ImguiCommandPools.clear();
    ImguiCommandBuffers.clear();

    if (ImguiDescriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(Device, ImguiDescriptorPool, nullptr);
        ImguiDescriptorPool = VK_NULL_HANDLE;
    }

    for (auto &Semaphore : RenderCompleteSemaphores)
    {
        vkDestroySemaphore(Device, Semaphore, nullptr);
    }
    RenderCompleteSemaphores.clear();

    for (auto &Fence : RenderFences)
    {
        vkDestroyFence(Device, Fence, nullptr);
    }
    RenderFences.clear();

    ImguiInitialized = false;
    LOG_DEBUG("ImGui Vulkan backend shut down.");
}

static LRESULT CALLBACK Hooked_WndProc(HWND Window, UINT Message, WPARAM WParam,
                                       LPARAM LParam)
{
    if (Message >= WM_APP)
    {
        if (OriginalWindowProc)
        {
            return CallWindowProc(OriginalWindowProc, Window, Message, WParam,
                                  LParam);
        }
    }

    // For all standard messages, let ImGui process them first.
    if (ImguiInitialized &&
        ImGui_ImplWin32_WndProcHandler(Window, Message, WParam, LParam))
    {
        // If ImGui wants to capture input, we should absorb the message and not
        // pass it to the game.
        ImGuiIO &IO = ImGui::GetIO();
        if (IO.WantCaptureMouse || IO.WantCaptureKeyboard)
        {
            return TRUE;
        }
    }

    handle_input();

    return CallWindowProc(OriginalWindowProc, Window, Message, WParam, LParam);
}

static VkResult VKAPI_PTR Hooked_vkQueuePresentKHR(
    VkQueue InputQueue, const VkPresentInfoKHR *pPresentInfo)
{
    if (Queue == VK_NULL_HANDLE)
    {
        Queue = InputQueue;
    }

    if (!ImguiInitialized)
    {
        InitializeImGui();
    }

    if (ImguiInitialized && (pPresentInfo->waitSemaphoreCount > 0) &&
        MENU_VISIBLE)
    {
        VkPresentInfoKHR ModifiedPresentInfo = *pPresentInfo;
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        render_ui(); // Submit Imgui draw commands.
        ImGui::Render();

        ImDrawData *DrawData = ImGui::GetDrawData();
        uint32_t ImageIdx = pPresentInfo->pImageIndices[0];

        VkFence FrameFence = RenderFences[ImageIdx];
        VkSemaphore FrameSemaphore = RenderCompleteSemaphores[ImageIdx];

        // vkResetCommandPool(Device, ImguiCommandPools[ImageIdx], 0);
        vkResetCommandBuffer(ImguiCommandBuffers[ImageIdx], 0);

        VkCommandBufferBeginInfo BeginInfo{};
        BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(ImguiCommandBuffers[ImageIdx], &BeginInfo);
        VkRenderPassBeginInfo RenderPassBeginInfo{};
        RenderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        RenderPassBeginInfo.renderPass = ImguiRenderPass;
        RenderPassBeginInfo.framebuffer = ImguiFramebuffers[ImageIdx];
        RenderPassBeginInfo.renderArea.extent = SwapchainExtent;

        vkCmdBeginRenderPass(ImguiCommandBuffers[ImageIdx],
                             &RenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        ImGui_ImplVulkan_RenderDrawData(DrawData,
                                        ImguiCommandBuffers[ImageIdx]);
        vkCmdEndRenderPass(ImguiCommandBuffers[ImageIdx]);
        vkEndCommandBuffer(ImguiCommandBuffers[ImageIdx]);

        VkSubmitInfo SubmitInfo{};
        SubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore GameRenderSemaphore = pPresentInfo->pWaitSemaphores[0];
        VkPipelineStageFlags WaitStage =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        SubmitInfo.waitSemaphoreCount = 1;
        SubmitInfo.pWaitSemaphores = &GameRenderSemaphore;
        SubmitInfo.pWaitDstStageMask = &WaitStage;

        SubmitInfo.commandBufferCount = 1;
        SubmitInfo.pCommandBuffers = &ImguiCommandBuffers[ImageIdx];

        SubmitInfo.signalSemaphoreCount = 1;
        SubmitInfo.pSignalSemaphores = &FrameSemaphore;

        // vkResetFences(Device, 1, &FrameFence);
        // vkQueueSubmit(InputQueue, 1, &SubmitInfo, FrameFence);
        vkQueueSubmit(InputQueue, 1, &SubmitInfo, VK_NULL_HANDLE);

        ModifiedPresentInfo.pWaitSemaphores = &FrameSemaphore;
        return OriginalVkQueuePresent(InputQueue, &ModifiedPresentInfo);
    }
    return OriginalVkQueuePresent(InputQueue, pPresentInfo);
}

static VkResult VKAPI_PTR Hooked_vkCreateDevice(
    VkPhysicalDevice physicalDevice, const VkDeviceCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkDevice *pDevice)
{
    LOG_DEBUG("%s: Called.", __func__);
    VkPhysicalDeviceTimelineSemaphoreFeatures TimelineSemaphoreFeatures{};
    TimelineSemaphoreFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
    TimelineSemaphoreFeatures.timelineSemaphore = VK_TRUE;

    // Important: You must chain this structure to the pNext pointer of
    // VkDeviceCreateInfo. Since we don't know what might already be in the
    // pNext chain, we must preserve it.
    TimelineSemaphoreFeatures.pNext =
        (void *)pCreateInfo->pNext; // Preserve existing pNext chain

    // Create a modifiable copy of the original create info.
    VkDeviceCreateInfo ModifiedCreateInfo = *pCreateInfo;

    // Point its pNext to our new feature structure.
    ModifiedCreateInfo.pNext = &TimelineSemaphoreFeatures;

    // Call the original function with the MODIFIED create info.
    VkResult Result = OriginalVkCreateDevice(
        physicalDevice, &ModifiedCreateInfo, pAllocator, pDevice);
    if (Result == VK_SUCCESS)
    {
        PhysicalDevice = physicalDevice;
        Device = *pDevice;
        VulkanDeviceCreated = true;
        LOG_DEBUG("%s: Captured VkDevice: %p, VkPhysicalDevice: %p", __func__,
                  (void *)Device, (void *)PhysicalDevice);

        // Capture VkInstance (can be obtained from vkGetInstanceProcAddr or
        // passed to vkCreateDevice) For simplicity, assuming it's available
        // globally or from a previous hook In a real scenario, you'd likely
        // hook vkCreateInstance as well to get g_vkInstance. If not, you'd need
        // to establish how to get it. For now, let's try to get a queue from
        // the created device.
        uint32_t QueueFamilyIdx =
            0; // Assuming the first queue family supports graphics and present
        vkGetDeviceQueue(Device, QueueFamilyIdx, 0, &Queue);
        LOG_DEBUG("%s: Captured VkQueue: %p", __func__, (void *)Queue);
    }
    return Result;
}
static void VKAPI_PTR
Hooked_vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR pSwapchain,
                             const VkAllocationCallbacks *pAllocator)
{
    LOG_DEBUG("%s: Called.", __func__);
    DeinitializeImGui();
    OriginalVkDestroySwapchain(device, pSwapchain, pAllocator);
}

static VkResult VKAPI_PTR Hooked_vkCreateSwapchainKHR(
    VkDevice device, const VkSwapchainCreateInfoKHR *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkSwapchainKHR *pSwapchain)
{
    LOG_DEBUG("%s: Called.", __func__);
    VkResult Result =
        OriginalVkCreateSwapchain(device, pCreateInfo, pAllocator, pSwapchain);

    if (Result == VK_SUCCESS)
    {
        Swapchain = *pSwapchain; // Store the created swapchain object
        LOG_DEBUG("%s: Swapchain created successfully. Capturing details...",
                  __func__);

        // Store swapchain properties
        SwapchainImageFormat = pCreateInfo->imageFormat;
        SwapchainExtent = pCreateInfo->imageExtent;
        LOG_DEBUG("%s: Swapchain Format: %d, Extent: %dx%d", __func__,
                  SwapchainImageFormat, SwapchainExtent.width,
                  SwapchainExtent.height);

        // Get swapchain images
        uint32_t ImageCount;
        vkGetSwapchainImagesKHR(device, *pSwapchain, &ImageCount, nullptr);
        SwapchainImages.resize(ImageCount);
        vkGetSwapchainImagesKHR(device, *pSwapchain, &ImageCount,
                                SwapchainImages.data());
        LOG_DEBUG("%s: Captured %u swapchain images.", __func__, ImageCount);

        // Resize vectors for image views, framebuffers, and command pools
        SwapchainImageViews.resize(ImageCount);
        ImguiFramebuffers.resize(ImageCount);
        ImguiCommandPools.resize(ImageCount);
        ImguiCommandBuffers.resize(ImageCount);

        // In Hooked_vkCreateSwapchainKHR, after resizing the other vectors...
        RenderCompleteSemaphores.resize(ImageCount);
        RenderFences.resize(ImageCount);

        VkSemaphoreCreateInfo SemaphoreInfo{};
        SemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo FenceInfo{};
        FenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        FenceInfo.flags =
            VK_FENCE_CREATE_SIGNALED_BIT; // Create signaled for first use

        for (uint32_t i = 0; i < ImageCount; ++i)
        {
            if (vkCreateSemaphore(device, &SemaphoreInfo, nullptr,
                                  &RenderCompleteSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(device, &FenceInfo, nullptr, &RenderFences[i]) !=
                    VK_SUCCESS)
            {
                LOG_DEBUG(
                    "Failed to create synchronization objects for frame %u!",
                    i);
            }
        }
        LOG_DEBUG("Created per-frame synchronization objects.");
        // Create a simple render pass for ImGui (if not already created).
        if (Device == VK_NULL_HANDLE)
        { // Double-check g_vkDevice here
            LOG_DEBUG("%s: CRITICAL ERROR: g_vkDevice is NULL before calling "
                      "CreateImGuiRenderPass! Cannot create render pass.",
                      __func__);
            return Result;
        }
        if (CreateRenderPass() != VK_SUCCESS)
        {
            LOG_DEBUG("%s: Error: Failed to create ImGui render pass during "
                      "swapchain creation!",
                      __func__);
        }
        else
        {
            LOG_DEBUG("%s: ImGui render pass created.", __func__);
        }

        // Create image views, framebuffers, and command pools for each
        // swapchain image
        VkImageViewCreateInfo ImageViewInfo{};
        ImageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ImageViewInfo.format = SwapchainImageFormat;
        ImageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        ImageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        ImageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        ImageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        ImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ImageViewInfo.subresourceRange.baseMipLevel = 0;
        ImageViewInfo.subresourceRange.levelCount = 1;
        ImageViewInfo.subresourceRange.baseArrayLayer = 0;
        ImageViewInfo.subresourceRange.layerCount = 1;

        VkFramebufferCreateInfo FramebufferInfo{};
        FramebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        FramebufferInfo.renderPass = ImguiRenderPass;
        FramebufferInfo.attachmentCount = 1;
        FramebufferInfo.width = SwapchainExtent.width;
        FramebufferInfo.height = SwapchainExtent.height;
        FramebufferInfo.layers = 1;

        VkCommandPoolCreateInfo CmdPoolInfo{};
        CmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        // Assuming queue family 0 is graphics and present. You might need to
        // find the correct one.
        CmdPoolInfo.queueFamilyIndex = 0;
        CmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VkCommandBufferAllocateInfo CommandBufferInfo{};
        CommandBufferInfo.sType =
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        CommandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        CommandBufferInfo.commandBufferCount = 1;

        for (uint32_t ImageIdx = 0; ImageIdx < ImageCount; ++ImageIdx)
        {
            ImageViewInfo.image = SwapchainImages[ImageIdx];
            if (vkCreateImageView(device, &ImageViewInfo, nullptr,
                                  &SwapchainImageViews[ImageIdx]) != VK_SUCCESS)
            {
                LOG_DEBUG("%s: Error: Failed to create image view for "
                          "swapchain image %u!",
                          __func__, ImageIdx);
            }

            FramebufferInfo.pAttachments = &SwapchainImageViews[ImageIdx];
            if (vkCreateFramebuffer(device, &FramebufferInfo, nullptr,
                                    &ImguiFramebuffers[ImageIdx]) != VK_SUCCESS)
            {
                LOG_DEBUG("%s: Error: Failed to create framebuffer for "
                          "swapchain image %u!",
                          __func__, ImageIdx);
                // Handle error
            }

            if (vkCreateCommandPool(device, &CmdPoolInfo, nullptr,
                                    &ImguiCommandPools[ImageIdx]) != VK_SUCCESS)
            {
                LOG_DEBUG(
                    "%s: Error: Failed to create command pool for ImGui %u!",
                    __func__, ImageIdx);
                // Handle error
            }

            CommandBufferInfo.commandPool = ImguiCommandPools[ImageIdx];
            if (vkAllocateCommandBuffers(device, &CommandBufferInfo,
                                         &ImguiCommandBuffers[ImageIdx]) !=
                VK_SUCCESS)

            {
                LOG_DEBUG(
                    "%s: Error: Failed to create command buffer for ImGui %u!",
                    __func__, ImageIdx);
                // Handle error
            }
        }
        LOG_DEBUG(
            "%s: Created image views, framebuffers, and command pools, buffers "
            "for ImGui.",
            __func__);

        // Initialize ImGui Vulkan backend now that we have all swapchain
        // details This should be called only once after all necessary Vulkan
        // objects are available
        if (!ImguiInitialized)
        {
            InitializeImGui();
        }
    }
    return Result;
}

static VkResult VKAPI_PTR Hooked_vkCreateInstance(
    const VkInstanceCreateInfo *pCreateInfo,
    const VkAllocationCallbacks *pAllocator, VkInstance *pInstance)
{
    LOG_DEBUG("%s: Called.", __func__);

    // Copy the original CreateInfo so we can modify it (if we would like to add
    // validation).
    VkInstanceCreateInfo ModifiedCreateInfo = *pCreateInfo;

    if (kVkEnableValidationLayers)
    {
        // Copy the original enabled layers, if any.
        std::vector<const char *> EnabledLayers;
        if (pCreateInfo->enabledLayerCount > 0)
        {
            EnabledLayers.assign(pCreateInfo->ppEnabledLayerNames,
                                 pCreateInfo->ppEnabledLayerNames +
                                     pCreateInfo->enabledLayerCount);
        }

        // Check whether validation layer is already in the list.
        const char *ValidationLayerName = "VK_LAYER_KHRONOS_validation";
        auto it = std::find(EnabledLayers.begin(), EnabledLayers.end(),
                            ValidationLayerName);
        if (it == EnabledLayers.end())
        {
            EnabledLayers.push_back(ValidationLayerName);
            LOG_DEBUG("%s: Injected validation layer.", __func__);
        }
        // Update the modifiedCreateInfo to include new layers
        ModifiedCreateInfo.enabledLayerCount =
            static_cast<uint32_t>(EnabledLayers.size());
        ModifiedCreateInfo.ppEnabledLayerNames = EnabledLayers.data();
    }
    // Call original function with modified CreateInfo.
    VkResult Result =
        OriginalVkCreateInstance(&ModifiedCreateInfo, pAllocator, pInstance);

    if (Result == VK_SUCCESS)
    {
        Instance = *pInstance;
        LOG_DEBUG("%s: Captured VkInstance: %p", __func__, (void *)Instance);
    }

    return Result;
}
