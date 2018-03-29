// Hello Vulkan triangle demo
// See original from: https://vulkan-tutorial.com/, for more details.

#include "goboVkTriangle/goboVkTriangle.h"

#include "sorban_loom/sorban_loom.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <set>

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

static bool readFile(const std::string& fileName, std::vector<char>& resultBuffer)
{
    std::ifstream file(fileName, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        lerror("Failed to open file {}", fileName.c_str());
        return false;
    }

    size_t fileSize = (size_t) file.tellg();
    resultBuffer.resize(fileSize);
    file.seekg(0);
    file.read(resultBuffer.data(), fileSize);
    file.close();

    ldebug("Read file {}, {} bytes", fileName.c_str(), fileSize);

    return true;
}

bool chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes,
                           VkPresentModeKHR& chosenPresentMode)
{
    bool fifoFound = false, immediateFound = false;
    for (const auto& availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            chosenPresentMode = availablePresentMode;
            return true;
        }
        if (availablePresentMode == VK_PRESENT_MODE_FIFO_KHR)
        {
            fifoFound = true;
        }
        if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
        {
            immediateFound = true;
        }
    }
    if (immediateFound)
    {
        chosenPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
        return true;
    }
    if (fifoFound)
    {
        chosenPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        return true;
    }

    return false;
}

bool chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                      uint32_t windowWidth,
                      uint32_t windowHeight,
                      VkExtent2D& chosenSwapExtent)
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        chosenSwapExtent = capabilities.currentExtent;
        return true;
    }
    else
    {
        VkExtent2D actualExtent = {windowWidth, windowHeight};
        actualExtent.width =
            std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, windowWidth));
        actualExtent.height =
            std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, windowHeight));

        chosenSwapExtent = actualExtent;
        return true;
    }
}

bool chooseSwapChainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats,
                                  VkSurfaceFormatKHR& chosenFormat)
{
    if (availableFormats.size() == 1 &&
        availableFormats[0].format == VK_FORMAT_UNDEFINED) // Best case, surface doesn't have preferences
    {
        chosenFormat = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        return true;
    }
    for (const auto& availableFormat : availableFormats)
    {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            chosenFormat = availableFormat;
            return true;
        }
    }
    if (availableFormats.size() >= 1)
    {
        chosenFormat = availableFormats[0];
        return true;
    }

    lerror("Failed to choose swap chain surface format!");
    return false;
}

VkResult CreateDebugReportCallbackEXT(VkInstance instance,
                                      const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
                                      const VkAllocationCallbacks* pAllocator,
                                      VkDebugReportCallbackEXT* pCallback)
{
    auto func = (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
    if (func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pCallback);
    }
    else
    {
        lerror("Failed to find createDebugReportCallback function.");
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugReportCallbackEXT(VkInstance instance,
                                   VkDebugReportCallbackEXT callback,
                                   const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallback");
    if (func != nullptr)
    {
        return func(instance, callback, pAllocator);
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT flags,
                                                    VkDebugReportObjectTypeEXT objectm,
                                                    uint64_t objm,
                                                    size_t location,
                                                    int32_t code,
                                                    const char* layerPrefix,
                                                    const char* msg,
                                                    void* userData)
{
    linfo("Validation layer message: {}", msg);

    return VK_FALSE;
}

struct QueueFamilyIndices
{
    int graphicsFamily = -1;
    int presentFamily = -1;

    bool isComplete()
    {
        return graphicsFamily >= 0 && presentFamily >= 0;
    }
};

struct SwapChainDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

bool querySwapChainSupport(const VkPhysicalDevice& device,
                           const VkSurfaceKHR& surface,
                           SwapChainDetails& swapchainDetails)
{
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &swapchainDetails.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0)
    {
        swapchainDetails.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, swapchainDetails.formats.data());
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0)
    {
        swapchainDetails.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device,
                                                  surface,
                                                  &presentModeCount,
                                                  swapchainDetails.presentModes.data());
    }

    return true;
}

static QueueFamilyIndices findQueueFamilies(const VkPhysicalDevice& device, VkSurfaceKHR& surface)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    linfo("Found {} queue families.", queueFamilyCount);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto& queueFamily : queueFamilies)
    {
        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (queueFamily.queueCount > 0 && presentSupport)
        {
            indices.presentFamily = i;
        }

        if (indices.isComplete())
        {
            break;
        }
        ++i;
    }

    return indices;
}

class HelloVkTriangleApplication
{
public:
    HelloVkTriangleApplication() : m_enableValidationLayers(false), m_physicalDevice(VK_NULL_HANDLE)
    {
        m_validationLayers.push_back("VK_LAYER_LUNARG_standard_validation");
        m_requiredDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    int rateDeviceSuitability(const VkPhysicalDevice& device, VkSurfaceKHR& surface)
    {
        int score = 0;
        VkPhysicalDeviceProperties deviceProperties;
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

        if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            score += 100;
        }

        score += deviceProperties.limits.maxImageDimension2D;

        if (!deviceFeatures.geometryShader)
        {
            return 0;
        }

        m_queueFamilyIndices = findQueueFamilies(device, surface);
        if (!m_queueFamilyIndices.isComplete())
        {
            return 0;
        }

        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
        for (const auto& deviceExtension : m_requiredDeviceExtensions)
        {
            const auto found = find_if(availableExtensions.cbegin(),
                                       availableExtensions.cend(),
                                       [&](const VkExtensionProperties& ext) {
                                           return std::string(ext.extensionName) == deviceExtension;
                                       });
            if (found == availableExtensions.cend())
            {
                return 0;
            }
        }

        SwapChainDetails swapchainDetails;
        if (!querySwapChainSupport(device, surface, swapchainDetails))
        {
            lerror("Failed to query swap chain support!");
            return 0;
        }
        if (swapchainDetails.formats.empty() || swapchainDetails.presentModes.empty())
        {
            return 0;
        }

        return score;
    }

    bool isDeviceSuitable(VkPhysicalDevice device)
    {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
        return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && deviceFeatures.geometryShader;
    }

    bool initWindow()
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        double factor = 0.25;
        m_windowWidth = factor * 1920;
        m_windowHeight = factor * 1080;
        m_window = glfwCreateWindow(m_windowWidth, m_windowHeight, "Vk", nullptr, nullptr);

        return true;
    }

    void setupDebugCallback()
    {
        if (!m_enableValidationLayers)
        {
            return;
        }

        VkDebugReportCallbackCreateInfoEXT createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
        createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
        createInfo.pfnCallback = debugCallback;
        if (CreateDebugReportCallbackEXT(m_instance, &createInfo, nullptr, &m_debugCallback) != VK_SUCCESS)
        {
            lerror("Failed to register debug callback!");
            return;
        }
    }

    bool createSurface()
    {
        if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface) != VK_SUCCESS)
        {
            lerror("Failed to create window surface!");
            return false;
        }
        return true;
    }

    bool pickPhysicalDevice()
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
        if (deviceCount == 0)
        {
            return false;
        }
        linfo("Found {} devices.", deviceCount);

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

        // Select best graphics device
        std::multimap<int, VkPhysicalDevice> candidates;
        for (const auto& device : devices)
        {
            int score = rateDeviceSuitability(device, m_surface);
            candidates.insert(std::make_pair(score, device));
        }

        if (candidates.rbegin()->first > 0)
        {
            m_physicalDevice = candidates.rbegin()->second;
        }

        if (m_physicalDevice == VK_NULL_HANDLE)
        {
            lerror("Failed to select proper device!");
            return false;
        }

        return true;
    }

    bool createSwapChain(uint32_t windowWidth, uint32_t windowHeight)
    {
        SwapChainDetails swapchainSupport;
        if (!querySwapChainSupport(m_physicalDevice, m_surface, swapchainSupport))
        {
            lerror("Failed to query for swap chain support!");
            return false;
        }

        VkPresentModeKHR presentMode;
        if (!chooseSwapPresentMode(swapchainSupport.presentModes, presentMode))
        {
            lerror("Failed to choose swap present mode!");
            return false;
        }

        VkSurfaceFormatKHR surfaceFormat;
        if (!chooseSwapChainSurfaceFormat(swapchainSupport.formats, surfaceFormat))
        {
            lerror("Failed to choose surface format!");
            return false;
        }

        VkExtent2D extent;
        if (!chooseSwapExtent(swapchainSupport.capabilities, windowWidth, windowHeight, extent))
        {
            lerror("Failed to choose swap chain extent!");
            return false;
        }

        uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
        if (swapchainSupport.capabilities.maxImageCount > 0 && imageCount > swapchainSupport.capabilities.maxImageCount)
        {
            imageCount = swapchainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        uint32_t queueFamilyIndices[] = {(uint32_t) m_queueFamilyIndices.graphicsFamily,
                                         (uint32_t) m_queueFamilyIndices.presentFamily};
        if (m_queueFamilyIndices.graphicsFamily != m_queueFamilyIndices.presentFamily)
        {
            linfo("Graphics and present queues are different, using concurrent mode!");
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            linfo("Graphics and present queues are the same, using exclusive mode!");
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = nullptr;
        }
        createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(m_logicalDevice, &createInfo, nullptr, &m_swapchain) != VK_SUCCESS)
        {
            lerror("Failed to create swap chain!");
            return false;
        }
        uint32_t swapImageCount = 0;
        vkGetSwapchainImagesKHR(m_logicalDevice, m_swapchain, &swapImageCount, nullptr);
        m_swapchainImages.resize(swapImageCount);
        vkGetSwapchainImagesKHR(m_logicalDevice, m_swapchain, &swapImageCount, m_swapchainImages.data());

        m_swapchainImageFormat = surfaceFormat.format;
        m_swapchainExtent = extent;

        return true;
    }

    bool createSwapChainImageViews()
    {
        m_swapchainImageViews.resize(m_swapchainImages.size());
        for (size_t i = 0; i < m_swapchainImages.size(); ++i)
        {
            VkImageViewCreateInfo createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = m_swapchainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = m_swapchainImageFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;
            if (vkCreateImageView(m_logicalDevice, &createInfo, nullptr, &m_swapchainImageViews[i]) != VK_SUCCESS)
            {
                return false;
            }
        }

        ldebug("{} image views created!", m_swapchainImages.size());

        return true;
    }

    bool createShaderModule(const std::vector<char>& shader, VkShaderModule& shaderModule)
    {
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = shader.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(shader.data());
        if (vkCreateShaderModule(m_logicalDevice, &createInfo, nullptr, &shaderModule))
        {
            lerror("Failed to create shader module!");
            return false;
        }

        return true;
    }

    bool createRenderPass()
    {
        VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = m_swapchainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.colorAttachmentCount = 1;

        VkRenderPassCreateInfo createRenderPassInfo = {};
        createRenderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createRenderPassInfo.attachmentCount = 1;
        createRenderPassInfo.pAttachments = &colorAttachment;
        createRenderPassInfo.subpassCount = 1;
        createRenderPassInfo.pSubpasses = &subpass;

        VkSubpassDependency dependency = {};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        createRenderPassInfo.dependencyCount = 1;
        createRenderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(m_logicalDevice, &createRenderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
        {
            lerror("Failed to create the render pass!");
            return false;
        }
        ldebug("Render pass created!");

        return true;
    }

    bool createLogicalDevice()
    {
        std::set<int> uniqueQueueFamilyIndices = {m_queueFamilyIndices.graphicsFamily,
                                                  m_queueFamilyIndices.presentFamily};
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        float queuePriority = 1.0f;
        for (int queueFamily : uniqueQueueFamilyIndices)
        {
            VkDeviceQueueCreateInfo queueCreateInfo = {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures = {};

        VkDeviceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(m_requiredDeviceExtensions.size());
        createInfo.ppEnabledExtensionNames = m_requiredDeviceExtensions.data();

        if (m_enableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
            createInfo.ppEnabledLayerNames = m_validationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_logicalDevice) != VK_SUCCESS)
        {
            lerror("Failed to create logical device!");
            return false;
        }

        return true;
    }

    bool createGraphicsPipeline()
    {
        std::vector<char> vertShaderCode;
        if (!readFile("X:\\goboVkTriangle\\code\\src\\vert.spv", vertShaderCode))
        {
            return false;
        }

        std::vector<char> fragShaderCode;
        if (!readFile("X:\\goboVkTriangle\\code\\src\\frag.spv", fragShaderCode))
        {
            return false;
        }

        VkShaderModule vertShaderModule;
        VkShaderModule fragShaderModule;

        if (!createShaderModule(vertShaderCode, vertShaderModule))
        {
            return false;
        }
        if (!createShaderModule(fragShaderCode, fragShaderModule))
        {
            return false;
        }
        ldebug("Shader modules created!");

        VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";

        VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = nullptr;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = nullptr;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) m_swapchainExtent.width;
        viewport.height = (float) m_swapchainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = m_swapchainExtent;

        VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f;
        rasterizer.depthBiasClamp = 0.0f;
        rasterizer.depthBiasSlopeFactor = 0.0f;

        VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f;
        multisampling.pSampleMask = nullptr;
        multisampling.alphaToCoverageEnable = VK_FALSE;
        multisampling.alphaToOneEnable = VK_FALSE;

        VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo colorBlending = {};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pSetLayouts = nullptr;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = 0;
        if (vkCreatePipelineLayout(m_logicalDevice, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
        {
            lerror("Failed to create pipeline layout!");
            return false;
        }

        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = nullptr;
        pipelineInfo.layout = m_pipelineLayout;
        pipelineInfo.renderPass = m_renderPass;
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
        pipelineInfo.basePipelineIndex = -1;

        if (vkCreateGraphicsPipelines(m_logicalDevice,
                                      VK_NULL_HANDLE,
                                      1,
                                      &pipelineInfo,
                                      nullptr,
                                      &m_graphicsPipeline) != VK_SUCCESS)
        {
            lerror("Failed to create graphics pipeline!");
            return false;
        }
        ldebug("Graphics pipeline created!");

        vkDestroyShaderModule(m_logicalDevice, vertShaderModule, nullptr);
        vkDestroyShaderModule(m_logicalDevice, fragShaderModule, nullptr);
        ldebug("Shader modules cleaned up.");

        return true;
    }

    bool createFramebuffers()
    {
        m_swapchainFramebuffers.resize(m_swapchainImageViews.size());
        for (size_t i = 0; i < m_swapchainImageViews.size(); ++i)
        {
            VkImageView attachments[] = {m_swapchainImageViews[i]};
            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = m_swapchainExtent.width;
            framebufferInfo.height = m_swapchainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(m_logicalDevice, &framebufferInfo, nullptr, &m_swapchainFramebuffers[i]) !=
                VK_SUCCESS)
            {
                lerror("Failed to create framebuffer for image view {}", i);
                return false;
            }
            ldebug("Created framebuffer {}", i);
        }

        return true;
    }

    bool createCommandPool()
    {
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = m_queueFamilyIndices.graphicsFamily;
        poolInfo.flags = 0;
        if (vkCreateCommandPool(m_logicalDevice, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS)
        {
            lerror("Failed to create command pool!");
            return false;
        }

        ldebug("Command pool created.");
        return true;
    }

    bool createCommandBuffers()
    {
        m_commandBuffers.resize(m_swapchainFramebuffers.size());
        VkCommandBufferAllocateInfo buffAllocInfo = {};
        buffAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        buffAllocInfo.commandPool = m_commandPool;
        buffAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        buffAllocInfo.commandBufferCount = (uint32_t) m_commandBuffers.size();

        if (vkAllocateCommandBuffers(m_logicalDevice, &buffAllocInfo, m_commandBuffers.data()) != VK_SUCCESS)
        {
            lerror("Failed to allocate command buffer!");
            return false;
        }

        for (size_t i = 0; i < m_commandBuffers.size(); ++i)
        {
            VkCommandBufferBeginInfo beginInfo = {};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
            beginInfo.pInheritanceInfo = nullptr;

            vkBeginCommandBuffer(m_commandBuffers[i], &beginInfo);
            VkRenderPassBeginInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = m_renderPass;
            renderPassInfo.framebuffer = m_swapchainFramebuffers[i];
            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = m_swapchainExtent;

            VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
            renderPassInfo.clearValueCount = 1;
            renderPassInfo.pClearValues = &clearColor;

            vkCmdBeginRenderPass(m_commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
            vkCmdDraw(m_commandBuffers[i], 3, 1, 0, 0);
            vkCmdEndRenderPass(m_commandBuffers[i]);

            if (vkEndCommandBuffer(m_commandBuffers[i]) != VK_SUCCESS)
            {
                lerror("Failed to record command buffer!");
                return false;
            }
        }

        ldebug("Command buffers created!");
        return true;
    }

    bool createSemaphores()
    {
        VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        if (vkCreateSemaphore(m_logicalDevice, &semaphoreInfo, nullptr, &m_imageAvailableSemaphore) != VK_SUCCESS ||
            vkCreateSemaphore(m_logicalDevice, &semaphoreInfo, nullptr, &m_renderFinishedSemaphore) != VK_SUCCESS)
        {
            lerror("Failed to create the semaphores.");
            return false;
        }

        ldebug("Semaphores created!");
        return true;
    }

    bool initVulkan()
    {
#ifndef NDEBUG
        m_enableValidationLayers = true;
#endif
        // Create Physical instance
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Awsome Vk Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "Gobos";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_1;

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        if (m_enableValidationLayers)
        {
            if (!checkValidationLayerSupport(m_validationLayers))
            {
                lerror("Validation layer missing!");
                return false;
            }
            createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
            createInfo.ppEnabledLayerNames = m_validationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        auto extensions = getRequiredExtensions();
        linfo("Required extensions:");
        for (int i = 0; i < extensions.size(); ++i)
        {
            linfo("\t{}", extensions[i]);
        }
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        createInfo.enabledLayerCount = 0;

        VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
        if (result != VK_SUCCESS)
        {
            lerror("Failed to create vulkan instance!");
            return false;
        }
        setupDebugCallback();
        createSurface();

        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());
        linfo("Available extensions:");
        for (const auto& avExt : availableExtensions)
        {
            linfo("\t{}", avExt.extensionName);
        }

        if (!pickPhysicalDevice())
        {
            return false;
        }

        createLogicalDevice();
        vkGetDeviceQueue(m_logicalDevice, m_queueFamilyIndices.graphicsFamily, 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_logicalDevice, m_queueFamilyIndices.presentFamily, 0, &m_presentQueue);
        createSwapChain(m_windowWidth, m_windowHeight);
        createSwapChainImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPool();
        createCommandBuffers();
        createSemaphores();

        return true;
    }

    bool drawFrame()
    {
        // Update game state
        vkQueueWaitIdle(m_presentQueue);

        uint32_t imageIndex = 0;
        vkAcquireNextImageKHR(m_logicalDevice,
                              m_swapchain,
                              std::numeric_limits<uint64_t>::max(),
                              m_imageAvailableSemaphore,
                              VK_NULL_HANDLE,
                              &imageIndex);

        VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphore};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_commandBuffers[imageIndex];

        VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphore};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
        {
            lerror("Failed to submit commands!");
            return false;
        }

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapchains[] = {m_swapchain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = nullptr;

        vkQueuePresentKHR(m_presentQueue, &presentInfo);

        return true;
    }

    void mainLoop()
    {
        while (!glfwWindowShouldClose(m_window))
        {
            glfwPollEvents();
            drawFrame();
        }
        vkDeviceWaitIdle(m_logicalDevice);
    }

    void cleanup()
    {
        linfo("Cleaning up");
        vkDestroySemaphore(m_logicalDevice, m_imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(m_logicalDevice, m_renderFinishedSemaphore, nullptr);
        vkDestroyCommandPool(m_logicalDevice, m_commandPool, nullptr);
        for (size_t i = 0; i < m_swapchainFramebuffers.size(); ++i)
        {
            vkDestroyFramebuffer(m_logicalDevice, m_swapchainFramebuffers[i], nullptr);
        }
        m_swapchainFramebuffers.clear();
        vkDestroyPipeline(m_logicalDevice, m_graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(m_logicalDevice, m_pipelineLayout, nullptr);
        vkDestroyRenderPass(m_logicalDevice, m_renderPass, nullptr);
        for (size_t i = 0; i < m_swapchainImageViews.size(); ++i)
        {
            vkDestroyImageView(m_logicalDevice, m_swapchainImageViews[i], nullptr);
        }
        vkDestroySwapchainKHR(m_logicalDevice, m_swapchain, nullptr);
        vkDestroyDevice(m_logicalDevice, nullptr);

        DestroyDebugReportCallbackEXT(m_instance, m_debugCallback, nullptr);

        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        vkDestroyInstance(m_instance, nullptr);

        glfwDestroyWindow(m_window);

        glfwTerminate();
    }

    std::vector<const char*> getRequiredExtensions()
    {
        std::vector<const char*> extensions;

        unsigned int glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        for (unsigned int i = 0; i < glfwExtensionCount; ++i)
        {
            extensions.push_back(glfwExtensions[i]);
        }

        if (m_enableValidationLayers)
        {
            extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        }

        return extensions;
    }

    bool checkValidationLayerSupport(const std::vector<const char*>& validationLayers)
    {
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        linfo("Available layers:");
        for (const char* layerName : validationLayers)
        {
            bool layerFound = false;
            linfo("\t{}", layerName);

            for (const auto& layerProperties : availableLayers)
            {
                if (strcmp(layerName, layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound)
            {
                return false;
            }
        }

        return true;
    }

private:
    uint32_t m_windowWidth;
    uint32_t m_windowHeight;
    bool m_enableValidationLayers;
    std::vector<const char*> m_validationLayers;
    GLFWwindow* m_window;
    VkInstance m_instance;
    VkDebugReportCallbackEXT m_debugCallback;
    VkSurfaceKHR m_surface;
    VkPhysicalDevice m_physicalDevice;
    QueueFamilyIndices m_queueFamilyIndices;
    std::vector<const char*> m_requiredDeviceExtensions;
    VkDevice m_logicalDevice;
    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;
    SwapChainDetails m_swapchainDetails;
    VkSwapchainKHR m_swapchain;
    std::vector<VkImage> m_swapchainImages;
    VkFormat m_swapchainImageFormat;
    VkExtent2D m_swapchainExtent;
    std::vector<VkImageView> m_swapchainImageViews;
    VkRenderPass m_renderPass;
    VkPipelineLayout m_pipelineLayout;
    VkPipeline m_graphicsPipeline;
    std::vector<VkFramebuffer> m_swapchainFramebuffers;
    VkCommandPool m_commandPool;
    std::vector<VkCommandBuffer> m_commandBuffers;
    VkSemaphore m_imageAvailableSemaphore;
    VkSemaphore m_renderFinishedSemaphore;
};

int main(int argc, char* argv[])
{
    sorban::loom::loggerInit("./goboVkTriangle.log", 10, 3);
    {
        HelloVkTriangleApplication helloVk;
        helloVk.run();
    }

    linfo("Event loop finished, preparing to exit.");
    return EXIT_SUCCESS;
}
