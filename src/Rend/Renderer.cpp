#include "Renderer.hpp"
#include <stdexcept>
#include <vector>
#include <iostream>
#include <map>
#include <cassert>
#include <set>
#include <vulkan/vulkan_core.h>
#include "../Logger.hpp"
#include "spdlog/spdlog.h"

namespace game
{
static std::vector<const char*> instanceExtensions{
  #ifdef DEBUG
  VK_EXT_DEBUG_UTILS_EXTENSION_NAME
  #endif
};
static std::vector<const char*> instanceLayers{
  #ifdef DEBUG
   "VK_LAYER_KHRONOS_validation"
  #endif
};
static std::vector<const char*> deviceLayers{};
static std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};


static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                    VkDebugUtilsMessageTypeFlagsEXT type,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                    void* userData)
{
  std::cerr << "[Vulkan]" << pCallbackData->pMessage << "\n";
  return VK_FALSE;
}

void Renderer::createDebugMessenger()
{
  VkDebugUtilsMessengerCreateInfoEXT createInfo
  {
    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
    .messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
    .messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
    .pfnUserCallback = debugCallback 
      
  };

  if(vkCreateDebugUtilsMessengerEXT(mInstance, &createInfo, nullptr, &mDebugMessenger) != VK_SUCCESS)
  {
    spdlog::error("Failed to create Debug Utils Messenger.");
    throw std::runtime_error("err");
  }

  spdlog::info("Created Debug Utils Messenger");
}
void Renderer::createVulkanInstance()
{
  volkInitialize();

  VkApplicationInfo appInfo{
    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,  
    .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
    .pEngineName = "None",
    .engineVersion = VK_MAKE_VERSION(1,0,0),
    .apiVersion = VK_API_VERSION_1_4,
  };

  uint32_t glfwExtensionCount = 0;
  const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  if(!glfwExtensions)
  {
    spdlog::error("Failed to get required GLFW instance extensions");
    throw std::runtime_error("err"); 
  }
  for(uint32_t i = 0; i < glfwExtensionCount; i++)
      instanceExtensions.push_back(glfwExtensions[i]);


  VkInstanceCreateInfo createInfo{
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0, 
    .pApplicationInfo = &appInfo,
    .enabledLayerCount = static_cast<uint32_t>(instanceLayers.size()),
    .ppEnabledLayerNames = instanceLayers.data(),  
    .enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size()),
    .ppEnabledExtensionNames = instanceExtensions.data(),
  };

  if(vkCreateInstance(&createInfo, nullptr, &mInstance) != VK_SUCCESS)
  {
    spdlog::critical("FAILED TO CREATE VULKAN INSTANCE!");
    throw std::runtime_error("crit-err");
  }

  spdlog::info("Created Vulkan instance");

  volkLoadInstance(mInstance);

  spdlog::info("Volk loaded instance");
}

void Renderer::initializeVMA()
{
  VmaVulkanFunctions vmaFuncInfo{};
  VmaAllocatorCreateInfo vmaAllocInfo{
    .physicalDevice = mPhysicalDevice,
    .device = mDevice,
    .pVulkanFunctions = &vmaFuncInfo,
    .instance = mInstance,
    .vulkanApiVersion = VK_API_VERSION_1_4
  };

  vmaImportVulkanFunctionsFromVolk(&vmaAllocInfo, &vmaFuncInfo);

  if(vmaCreateAllocator(&vmaAllocInfo, &mVmaAllocator) != VK_SUCCESS)
  {
    spdlog::critical("FAILED TO CREATE VMA ALLOCATOR!");
    throw std::runtime_error("crit-err");
  }
  spdlog::info("Created VMA allocator");
}
bool checkDeviceExtensionSupport(VkPhysicalDevice device)
{
  uint32_t extensionCount = 0;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
  std::vector<VkExtensionProperties> extensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

  std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

  for(const auto& extension : extensions)
  {
    requiredExtensions.erase(extension.extensionName);
  }

  return requiredExtensions.empty();
}
void Renderer::pickPhysicalDevice()
{
  uint32_t physicalDeviceCount = 0;
  vkEnumeratePhysicalDevices(mInstance, &physicalDeviceCount, nullptr);
  std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
  vkEnumeratePhysicalDevices(mInstance, &physicalDeviceCount, physicalDevices.data());

  assert(physicalDeviceCount > 0);
  assert(physicalDevices.size() > 0);

  std::multimap<int, VkPhysicalDevice> candidates{};

  for(const auto& device : physicalDevices)
  {
    int score = 0;

    VkPhysicalDeviceFeatures2 features{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    vkGetPhysicalDeviceFeatures2(device, &features);
    VkPhysicalDeviceProperties2 properties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    vkGetPhysicalDeviceProperties2(device, &properties);

    if(!features.features.geometryShader)
      continue;

    if(!checkDeviceExtensionSupport(device))
      continue;

    RenderUtil::SwapchainSupportDetails swapchainSupport = querySwapchainSupport(device);
    if(swapchainSupport.presentModes.empty() || swapchainSupport.surfaceFormats.empty()) continue;

    constexpr uint16_t discreteBoost = 1000;
    if(properties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += discreteBoost;

    candidates.insert(std::make_pair(score,device));
  }

  assert(candidates.size() >0);

  if(candidates.rbegin()->first > 0)
    mPhysicalDevice = candidates.rbegin()->second;
  else {spdlog::error("No suitable GPU found on client!"); throw std::runtime_error("err");}
}

RenderUtil::QueueFamilyIndices Renderer::findFamilyIndices(VkPhysicalDevice device) {
  RenderUtil::QueueFamilyIndices indices;

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties2(device, &queueFamilyCount, nullptr);
  std::vector<VkQueueFamilyProperties2> queueFamilyProperties(queueFamilyCount);
  for(auto& queueFamily : queueFamilyProperties)
    queueFamily.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;

  vkGetPhysicalDeviceQueueFamilyProperties2(device, &queueFamilyCount, queueFamilyProperties.data());

  int i = 0;
  for(const auto& qfp : queueFamilyProperties)
  {
    if(qfp.queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
      indices.graphicsFamily = i;
    if(qfp.queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT)
      indices.computeFamily = i;
    if(qfp.queueFamilyProperties.queueFlags & VK_QUEUE_TRANSFER_BIT)
      indices.transferFamily = i;

    VkBool32 presentSupport = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, mWindowSurface, &presentSupport);
    if(presentSupport)
      indices.presentFamily = i;

    i++;
  }

  return indices;

}
void Renderer::createLogicalDevice()
{
  VkPhysicalDeviceVulkan14Features supportedFeatures14{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES, .pNext = nullptr};
  VkPhysicalDeviceVulkan13Features supportedFeatures13{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, .pNext = &supportedFeatures14};
  VkPhysicalDeviceVulkan12Features supportedFeatures12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .pNext  = &supportedFeatures13 };
  VkPhysicalDeviceFeatures2 supportedFeatures11{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &supportedFeatures12 };
  vkGetPhysicalDeviceFeatures2(mPhysicalDevice, &supportedFeatures11);

  VkPhysicalDeviceProperties2 props{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
  vkGetPhysicalDeviceProperties2(mPhysicalDevice, &props);

  spdlog::debug(props.properties.deviceName);
  if(!supportedFeatures13.dynamicRendering || !supportedFeatures13.synchronization2 || !supportedFeatures12.timelineSemaphore)
  {
    spdlog::error("Physical device doesn't fulfill application Vulkan 1.X feature requirements.");
    throw std::runtime_error("err");
  }
  VkPhysicalDeviceVulkan14Features features14{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES, .pNext = nullptr};
  VkPhysicalDeviceVulkan13Features features13{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES, .pNext = &features14, .synchronization2 = VK_TRUE, .dynamicRendering = VK_TRUE};
  VkPhysicalDeviceVulkan12Features features12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, .pNext = &features13, .timelineSemaphore = VK_TRUE};
  VkPhysicalDeviceFeatures2 features11{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, .pNext = &features12};

  RenderUtil::QueueFamilyIndices indices = findFamilyIndices(mPhysicalDevice);
  float queuePriorities = 1.0f;

  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueIndices = {indices.graphicsFamily.value(), indices.computeFamily.value(), indices.transferFamily.value()};

  for(const auto& idc : uniqueQueueIndices)
  {
    VkDeviceQueueCreateInfo queueCreateInfo{
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .queueFamilyIndex = idc,
      .queueCount = 1,
      .pQueuePriorities = &queuePriorities,
    };

    queueCreateInfos.push_back(queueCreateInfo);
  }
  VkDeviceCreateInfo createInfo{
    .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
    .pNext = &features11,
    .flags = 0,
    .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
    .pQueueCreateInfos = queueCreateInfos.data(),
    .enabledLayerCount = static_cast<uint32_t>(deviceLayers.size()),
    .ppEnabledLayerNames = deviceLayers.data(),
    .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
    .ppEnabledExtensionNames = deviceExtensions.data() 
  };

  if(vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice) != VK_SUCCESS)
  {
    spdlog::critical("FAILED TO CREATE LOGICAL DEVICE!");
    throw std::runtime_error("crit-err");
  }
  vkGetDeviceQueue(mDevice, indices.graphicsFamily.value(), 0, &mGraphicsQueue);
  vkGetDeviceQueue(mDevice, indices.transferFamily.value(), 0, &mTransferQueue);
  vkGetDeviceQueue(mDevice, indices.computeFamily.value(), 0, &mComputeQueue);
  vkGetDeviceQueue(mDevice, indices.presentFamily.value(), 0, &mPresentQueue);

  spdlog::info("Created logical device and acquired queues.");
}

void Renderer::createWindowSurface(GLFWwindow* window)
{
  if(glfwCreateWindowSurface(mInstance, window, nullptr, &mWindowSurface) != VK_SUCCESS)
  {
    spdlog::error("Failed to create window surface.");
    throw std::runtime_error("err");
  }
  spdlog::info("Created window surface.");
}

RenderUtil::SwapchainSupportDetails Renderer::querySwapchainSupport(VkPhysicalDevice device) const
{
  RenderUtil::SwapchainSupportDetails swapchainSupport{};

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, mWindowSurface, &presentModeCount, nullptr);
  swapchainSupport.presentModes.resize(presentModeCount);
  if(presentModeCount > 0)
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, mWindowSurface, &presentModeCount, swapchainSupport.presentModes.data());

  VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo{
    .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR,
    .surface = mWindowSurface 
  };

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, mWindowSurface, &formatCount, nullptr);
  swapchainSupport.surfaceFormats.resize(formatCount);
  if(formatCount > 0)
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, mWindowSurface, &formatCount, swapchainSupport.surfaceFormats.data());

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, mWindowSurface, &swapchainSupport.capabilities);

  return swapchainSupport;
}

void Renderer::createSwapchain(GLFWwindow* window)
{
  RenderUtil::SwapchainSupportDetails supportDetails = querySwapchainSupport(mPhysicalDevice);

  mSwapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
  for(const auto& mode : supportDetails.presentModes)
  {
    if(mode != VK_PRESENT_MODE_MAILBOX_KHR)
      continue;

    mSwapchainPresentMode = mode;
  }

  mSwapchainFormat = supportDetails.surfaceFormats[0];
  for(const auto& format : supportDetails.surfaceFormats)
  {
    if(format.format != VK_FORMAT_B8G8R8A8_SRGB || format.colorSpace != VK_COLORSPACE_SRGB_NONLINEAR_KHR)
      continue;

    mSwapchainFormat = format;
  }

  if(supportDetails.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    mSwapchainExtent = supportDetails.capabilities.currentExtent;
  else
  {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    VkExtent2D actualExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
  
    actualExtent.width = std::clamp(actualExtent.width, supportDetails.capabilities.minImageExtent.width, supportDetails.capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(actualExtent.height, supportDetails.capabilities.minImageExtent.height, supportDetails.capabilities.maxImageExtent.height);

    mSwapchainExtent = actualExtent;
  }

  uint32_t imageCount = supportDetails.capabilities.minImageCount + 1;
  if(supportDetails.capabilities.maxImageCount > 0 && imageCount > supportDetails.capabilities.maxImageCount)
    imageCount = supportDetails.capabilities.maxImageCount;

  RenderUtil::QueueFamilyIndices indices = findFamilyIndices(mPhysicalDevice);
  uint32_t qfi[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

  bool sameIndex = indices.presentFamily.value() == indices.graphicsFamily.value();

  VkSwapchainCreateInfoKHR createInfo{
    .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
    .surface = mWindowSurface, 
    .minImageCount = imageCount,
    .imageFormat = mSwapchainFormat.format,
    .imageColorSpace = mSwapchainFormat.colorSpace,
    .imageExtent = mSwapchainExtent,
    .imageArrayLayers = 1,
    .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
    .imageSharingMode = sameIndex ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT,
    .queueFamilyIndexCount = sameIndex ? (uint32_t)0 : (uint32_t)2,
    .pQueueFamilyIndices = sameIndex ? nullptr : qfi,
    .preTransform = supportDetails.capabilities.currentTransform,
    .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
    .presentMode = mSwapchainPresentMode,
    .clipped = VK_TRUE,
    .oldSwapchain = VK_NULL_HANDLE,
  };
  
  if(vkCreateSwapchainKHR(mDevice, &createInfo, nullptr, &mSwapchain) != VK_SUCCESS)
  {
    spdlog::critical("Failed to create swap chain!");
    throw std::runtime_error("crit-err");
  }

  spdlog::info("Created swap chain.");

  mSwapchainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(mDevice, mSwapchain, &imageCount, mSwapchainImages.data());

  mSwapchainImageViews.resize(mSwapchainImages.size());
  int i = 0;
  for(auto& imageView : mSwapchainImageViews)
  {
    VkImageViewCreateInfo viewCreateInfo{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = mSwapchainImages[i],
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = mSwapchainFormat.format,
      .components = {
        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .a = VK_COMPONENT_SWIZZLE_IDENTITY
      },
      .subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1
      }
    };

    if(vkCreateImageView(mDevice, &viewCreateInfo, nullptr, &imageView) != VK_SUCCESS)
    {
      spdlog::error("Failed to create swapchain image view");
      throw std::runtime_error("err");
    }
    i++;
  }

  spdlog::info("Created swapchain image views");
}

void Renderer::createSyncResources()
{
  mImageReadySemaphores.resize(MAXFRAMESINFLIGHT);
  mRenderFinishedSemaphores.resize(mSwapchainImages.size());
  mInFlightFences.resize(MAXFRAMESINFLIGHT);

  VkSemaphoreCreateInfo semaphoreInfo{
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
  };
  VkFenceCreateInfo fenceInfo{
    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    .flags = VK_FENCE_CREATE_SIGNALED_BIT 
  };

  for(int i = 0; i < MAXFRAMESINFLIGHT; i++)
  {
    if(vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mImageReadySemaphores[i]) != VK_SUCCESS ||
       vkCreateFence(mDevice, &fenceInfo, nullptr, &mInFlightFences[i]) != VK_SUCCESS)
    {
      spdlog::error("Failed to create sync resource!");
      throw std::runtime_error("err");
    }
  }

  for(int i = 0; i < mSwapchainImages.size(); i++)
  {
    if(vkCreateSemaphore(mDevice, &semaphoreInfo, nullptr, &mRenderFinishedSemaphores[i]) != VK_SUCCESS )
    {
      spdlog::error("Failed to create sync resources!");
      throw std::runtime_error("err");
    }
  }

  spdlog::info("Created sync resources!");
}

void Renderer::createCommandBuffer()
{
  RenderUtil::QueueFamilyIndices qfi = findFamilyIndices(mPhysicalDevice);

  VkCommandPoolCreateInfo poolInfo{
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
    .queueFamilyIndex = qfi.graphicsFamily.value()
  };

  if(vkCreateCommandPool(mDevice, &poolInfo, nullptr, &mCommandPool) != VK_SUCCESS)
  {
    spdlog::error("Failed to create command pool!");
    throw std::runtime_error("err");
  }

  VkCommandBufferAllocateInfo allocInfo{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = mCommandPool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = MAXFRAMESINFLIGHT,
  };

  mCommandBuffers.resize(MAXFRAMESINFLIGHT);
  if(vkAllocateCommandBuffers(mDevice, &allocInfo, mCommandBuffers.data()) != VK_SUCCESS)
  {
    spdlog::critical("Failed to allocate command buffers!");
    throw std::runtime_error("crit-err");
  }

  spdlog::info("Created command buffers!");
}

VkCommandBuffer Renderer::beginRecording()
{
  vkWaitForFences(mDevice, 1, &mInFlightFences[mCurrentFrameInFlight], VK_TRUE, UINT32_MAX);

  VkAcquireNextImageInfoKHR acquireInfo{
    .sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR,
    .swapchain = mSwapchain,
    .timeout = UINT32_MAX,
    .semaphore = mImageReadySemaphores[mCurrentFrameInFlight],
    .deviceMask = 1,
  };

  uint32_t imageIndex;
  VkResult result = vkAcquireNextImage2KHR(mDevice, &acquireInfo, &imageIndex);
  if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
  {
    // recreate the swapchain 
  }
  else if(result != VK_SUCCESS)
  {
    spdlog::critical("Failed to acquire next image from the swapchain!");
    throw std::runtime_error("crit-err");
  }
  vkResetFences(mDevice, 1, &mInFlightFences[mCurrentFrameInFlight]);

  vkResetCommandBuffer(mCommandBuffers[mCurrentFrameInFlight], 0);

  mCurrentImageIndex = imageIndex;
  VkCommandBufferBeginInfo beginInfo{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
  };

  vkBeginCommandBuffer(mCommandBuffers[mCurrentFrameInFlight], &beginInfo);

  vkCmdBindPipeline(mCommandBuffers[mCurrentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);
  VkImageMemoryBarrier2 barrier{
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
    .srcAccessMask = VK_ACCESS_2_NONE,
    .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = mSwapchainImages[mCurrentImageIndex],
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
    }
  };


  VkDependencyInfo depInfo{
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .imageMemoryBarrierCount = 1,
    .pImageMemoryBarriers = &barrier 
  };
  vkCmdPipelineBarrier2(mCommandBuffers[mCurrentFrameInFlight], &depInfo);

  VkViewport viewport{
    .x = 0,
    .y = 0,
    .width = static_cast<float>(mSwapchainExtent.width),
    .height = static_cast<float> (mSwapchainExtent.height),
    .minDepth = 0.0f,
    .maxDepth = 1.0f
  };

  vkCmdSetViewport(mCommandBuffers[mCurrentFrameInFlight], 0, 1, &viewport);

  VkRect2D scissor{
    .offset = {0,0},
    .extent = mSwapchainExtent
  };
  vkCmdSetScissor(mCommandBuffers[mCurrentFrameInFlight], 0, 1, &scissor);

  return mCommandBuffers[mCurrentFrameInFlight];

}

void Renderer::beginRendering()
{
  VkRenderingAttachmentInfo attInfo{
    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
    .imageView = mSwapchainImageViews[mCurrentImageIndex],
    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .resolveMode = VK_RESOLVE_MODE_NONE,
    .resolveImageView = VK_NULL_HANDLE,
    .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .clearValue = {
      .color{{0.0f, 0.0f, 0.0f, 1.0f}}
    }
  };

  VkRenderingInfo renderingInfo{
    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
    .pNext = nullptr,
    .flags = 0,
    .renderArea = {
      .offset = {0, 0},
      .extent = mSwapchainExtent 
    },
    .layerCount = 1,
    .viewMask = 0,
    .colorAttachmentCount = 1,
    .pColorAttachments = &attInfo,
    .pDepthAttachment = nullptr,
    .pStencilAttachment = nullptr 
  };

  vkCmdBeginRendering(mCommandBuffers[mCurrentFrameInFlight], &renderingInfo);
}

void Renderer::finishRendering()
{
  vkCmdEndRendering(mCommandBuffers[mCurrentFrameInFlight]);

  VkImageMemoryBarrier2 barrier{
    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
    .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
    .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
    .dstStageMask = VK_PIPELINE_STAGE_2_NONE,
    .dstAccessMask = VK_ACCESS_2_NONE,
    .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    .image = mSwapchainImages[mCurrentImageIndex],
    .subresourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
    }
  };
  VkDependencyInfo depInfo{
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .imageMemoryBarrierCount = 1,
    .pImageMemoryBarriers = &barrier
  };
  vkCmdPipelineBarrier2(mCommandBuffers[mCurrentFrameInFlight], &depInfo);

  if(vkEndCommandBuffer(mCommandBuffers[mCurrentFrameInFlight]) != VK_SUCCESS)
  {
    spdlog::error("Failed to end recording command buffer!");
    throw std::runtime_error("err");
  }

  VkCommandBufferSubmitInfo cbInfo{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
    .commandBuffer = mCommandBuffers[mCurrentFrameInFlight],
    .deviceMask = 0
  };
  VkSemaphoreSubmitInfo waitInfo{
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
    .semaphore = mImageReadySemaphores[mCurrentFrameInFlight],
    .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT
  };
  VkSemaphoreSubmitInfo signalInfo{
    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
    .semaphore = mRenderFinishedSemaphores[mCurrentImageIndex],
    .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT
  };

  VkSubmitInfo2 submitInfo{
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
    .waitSemaphoreInfoCount = 1,
    .pWaitSemaphoreInfos= &waitInfo,
    .commandBufferInfoCount = 1,
    .pCommandBufferInfos = &cbInfo,
    .signalSemaphoreInfoCount = 1,
    .pSignalSemaphoreInfos = &signalInfo
  };

  if(vkQueueSubmit2(mGraphicsQueue, 1, &submitInfo, mInFlightFences[mCurrentFrameInFlight]) != VK_SUCCESS)
  {
    spdlog::error("Failed to submit cb to graphics queue!");
    throw std::runtime_error("err");
  }

  VkPresentInfoKHR presentInfo{
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1, 
    .pWaitSemaphores = &mRenderFinishedSemaphores[mCurrentImageIndex],
    .swapchainCount = 1,
    .pSwapchains = &mSwapchain,
    .pImageIndices = &mCurrentImageIndex
    
  };
  VkResult result = vkQueuePresentKHR(mPresentQueue, &presentInfo);

  if(result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
  {
    // recreate swapchain 
  }
  else if(result != VK_SUCCESS)
  {
    spdlog::error("Failed to present to the screen!");
    throw std::runtime_error("err");
  }

  mCurrentFrameInFlight = (mCurrentFrameInFlight + 1) % MAXFRAMESINFLIGHT;
}
}
