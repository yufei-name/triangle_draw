#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "common.h"
#include "buffer.h"
#include "memory.h"
#include "pipeline.h"
#include "descriptor.h"

struct Vertex  vertices[] = {
		{{1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}},        // Vertex 1: Red
		{{1.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},         // Vertex 2: Green
		{{-1.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}         // Vertex 3: Blue
};

struct
{
	glm::mat4 projection;
	glm::mat4 model;
	glm::vec4 view_pos;
} ubo_vs;

PFN_vkGetDeviceProcAddr pfn_vkGetDeviceProcAddr = NULL;

int findSuitableInstanceExtensions(char*** requestedExtensions, int* requestCount)
{
	int i;
	unsigned int platExtNum = 0;
	unsigned int instanceEextensionCount;
	VkExtensionProperties* availableInstanceExtensions;
	const char* requestedExtensionName[] = {
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
	};

	const char** reqPlatExtName = get_platform_extension(&platExtNum);
	unsigned int requestedExtNum = sizeof(requestedExtensionName) / sizeof(requestedExtensionName[0]);

	const char** requestInstExt = (const char**)malloc((requestedExtNum + platExtNum) * sizeof(requestedExtensionName[0]));
	if (!requestInstExt)
		return -1;

	for (i = 0; i < requestedExtNum; i++)
	{
		requestInstExt[i] = requestedExtensionName[i];
	}

	for (; i < requestedExtNum + platExtNum; i++)
	{
		requestInstExt[i] = reqPlatExtName[i - requestedExtNum];
	}

	vkEnumerateInstanceExtensionProperties(nullptr, &instanceEextensionCount, nullptr);
	if (!instanceEextensionCount)
		return -1;

	availableInstanceExtensions = (VkExtensionProperties*)malloc(sizeof(VkExtensionProperties) * instanceEextensionCount);
	if (!availableInstanceExtensions)
		return -1;

	vkEnumerateInstanceExtensionProperties(nullptr, &instanceEextensionCount, availableInstanceExtensions);
	*requestedExtensions = (char**)malloc(sizeof(requestedExtensionName[0]) * instanceEextensionCount);
	if (!requestedExtensions)
		return -1;

	for (unsigned int i = 0; i < requestedExtNum + platExtNum; i++)
	{
		int found = 0;
		for (unsigned int j = 0; j < instanceEextensionCount; j++)
		{
			if (!strcmp(requestInstExt[i], availableInstanceExtensions[j].extensionName))
			{
				found = 1;
				break;
			}
		}

		if (found)
		{
			(*requestedExtensions)[*requestCount] = (char*)requestInstExt[i];
			(*requestCount)++;
		}
	}
	if (requestInstExt)
		free(requestInstExt);

	return 0;
}

VkPhysicalDevice get_suitable_gpu(VkSurfaceKHR surface, VkPhysicalDevice* physDevice, unsigned int phys_device_num)
{
	assert(phys_device_num != 0 && "No physical devices were found on the system.");
	assert(surface && "Invalid surface handle!");

	// Find a discrete GPU
	for (int i = 0; i < phys_device_num; i++)
	{
		VkPhysicalDeviceProperties properties;
		vkGetPhysicalDeviceProperties(physDevice[i], &properties);
		if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
		{
			// See if it work with the surface
			uint32_t queue_family_properties_count = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(physDevice[i], &queue_family_properties_count, nullptr);
			VkQueueFamilyProperties *queue_family_properties = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties)* queue_family_properties_count);
			vkGetPhysicalDeviceQueueFamilyProperties(physDevice[i], &queue_family_properties_count, queue_family_properties);


			for (uint32_t queue_idx = 0; static_cast<size_t>(queue_idx) < queue_family_properties_count; queue_idx++)
			{
				VkBool32 present_supported{ VK_FALSE };
				vkGetPhysicalDeviceSurfaceSupportKHR(physDevice[i], queue_idx, surface, &present_supported);
				if (present_supported)
				{
					free(queue_family_properties);
					return physDevice[i];
				}
			}
			free(queue_family_properties);
		}
	}

	// Otherwise just pick the first one
	printf("Couldn't find a discrete physical device, picking default GPU");
	return physDevice[0];
}
static int extension_supported(VkExtensionProperties* extensions, unsigned int ext_cnt, const char* ext_name)
{
	for (int i = 0; i < ext_cnt; i++)
	{
		if (!strcmp(extensions[i].extensionName, ext_name))
		{
			return 1;
		}
	}
	return 0;
}

static const char* requestedDeviceExt[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};
static VkDevice create_device(VkPhysicalDevice physDevice)
{
	VkDevice device = VK_NULL_HANDLE;
	VkPhysicalDeviceFeatures features;
	VkPhysicalDeviceProperties physDeviceProperties;
	VkQueueFamilyProperties* queueFamilyProperties;
	unsigned int queueFamilyCount;
	int missing_device_extension = 0;
	VkDeviceQueueCreateInfo* queueCreateInfo = NULL;
	float** queueProperties = NULL;
	VkDeviceCreateInfo create_info = {  };
	VkExtensionProperties* deviceExtensions = NULL;
	unsigned int enableExtensionCount = 0;
	VkResult ret = VK_SUCCESS;
	const char** enabledExtensionName = NULL;
	const char* requestedExtensionName[] = {
		"VK_KHR_get_memory_requirements2",
		"VK_KHR_dedicated_allocation",
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
	};
	vkGetPhysicalDeviceFeatures(physDevice, &features);
	vkGetPhysicalDeviceProperties(physDevice, &physDeviceProperties);
	printf("selected gpu device %s\n", physDeviceProperties.deviceName);
	vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, NULL);
	queueFamilyProperties = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * queueFamilyCount);
	if (queueFamilyProperties == NULL)
		return device;

	vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, queueFamilyProperties);
	queueCreateInfo = (VkDeviceQueueCreateInfo*)malloc(queueFamilyCount * sizeof(VkDeviceQueueCreateInfo));
	queueProperties = (float**)malloc(queueFamilyCount * sizeof(float*));

	if (queueCreateInfo == NULL || queueProperties == NULL)
		return device;

	for (int i = 0; i < queueFamilyCount; i++)
	{
		VkQueueFamilyProperties queueFamilyProperty = queueFamilyProperties[i];
		queueProperties[i] = (float*)malloc(queueFamilyProperty.queueCount * sizeof(float));
		if (!queueProperties[i])
		{
			return device;
		}

		for (int j = 0; j < queueFamilyProperty.queueCount; j++)
		{
			queueProperties[i][j] = 0.5;
		}

		queueCreateInfo[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo[i].pNext = NULL;
		queueCreateInfo[i].queueFamilyIndex = i;
		queueCreateInfo[i].queueCount = queueFamilyProperty.queueCount;
		queueCreateInfo[i].pQueuePriorities = queueProperties[i];
		queueCreateInfo[i].flags = 0;
	}

	uint32_t device_extension_count;
	vkEnumerateDeviceExtensionProperties(physDevice, nullptr, &device_extension_count, nullptr);
	deviceExtensions = (VkExtensionProperties*)malloc(device_extension_count * sizeof(VkExtensionProperties));
	if (!deviceExtensions)
		goto failed;

	vkEnumerateDeviceExtensionProperties(physDevice, nullptr, &device_extension_count, deviceExtensions);
	enabledExtensionName = (const char**)malloc(sizeof(char*) * device_extension_count);
	if (!enabledExtensionName)
		goto failed;
	for (int i = 0; i < sizeof(requestedExtensionName) / sizeof(requestedExtensionName[0]); i++)
	{
		if (extension_supported(deviceExtensions, device_extension_count, requestedExtensionName[i]))
		{
			enabledExtensionName[enableExtensionCount] = requestedExtensionName[i];
			enableExtensionCount++;
		}
	}
	
	for (int i = 0; i < sizeof(requestedDeviceExt) / sizeof(requestedDeviceExt[0]); i++)
	{
		int match = 0;

		for (int j = 0; j < device_extension_count; j++)
		{
			if (!strcmp(requestedDeviceExt[i], deviceExtensions[j].extensionName))
			{
				match = 1;
				break;
			}
		}
		if (!match)
		{
			printf("requested extension not presented in physical device\n");
			missing_device_extension = 1;
			break;
		}
	}
	if (missing_device_extension)
		goto failed;

	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.pNext = NULL;
	create_info.pQueueCreateInfos = queueCreateInfo;
	create_info.queueCreateInfoCount = queueFamilyCount;
	create_info.enabledExtensionCount = enableExtensionCount;
	create_info.ppEnabledExtensionNames = enabledExtensionName;
	create_info.pEnabledFeatures = NULL;
	ret = vkCreateDevice(physDevice, &create_info, NULL, &device);

failed:
	if (deviceExtensions)
		free(deviceExtensions);

	if (enabledExtensionName)
		free(enabledExtensionName);

	for (int i = 0; i < queueFamilyCount; i++)
	{
		free(queueProperties[i]);
	}
	if (queueProperties)
		free(queueProperties);

	if (queueCreateInfo)
		free(queueCreateInfo);

	if (queueFamilyProperties)
		free(queueFamilyProperties);

	return device;
}

static VkFormat get_suitable_depth_format(VkPhysicalDevice physical_device)
{
	VkFormat depth_format_priority_list[] = 
	          { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM };
	VkFormat depth_format = VK_FORMAT_UNDEFINED ;
	for (uint32_t i = 0; i < sizeof(depth_format_priority_list) / sizeof(depth_format_priority_list[0]); i++)
	{
		VkFormatProperties properties;
		VkFormat format = depth_format_priority_list[i];
		vkGetPhysicalDeviceFormatProperties(physical_device, format, &properties);
		if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			depth_format = format;
			break;
		}
	}
	return depth_format;
}
static int create_cmd_pool(VkDevice device, VkQueueFamilyProperties* queueFamilyProperties, uint32_t propertyCount, VkCommandPool *pCmdPool)
{
	VkQueue queue = VK_NULL_HANDLE;
	VkCommandPoolCreateInfo createCmdPool;
	int queueFamilyIndex = -1;
	for (int i = 0; i < propertyCount; i++)
	{
        vkGetDeviceQueue(device, i, 0, &queue);
		if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)
		{
			queueFamilyIndex = i;
			break;
		}
		queue = VK_NULL_HANDLE;
	}

	if (VK_NULL_HANDLE == queue)
		return -1;

	createCmdPool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	createCmdPool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	createCmdPool.queueFamilyIndex = queueFamilyIndex;
	createCmdPool.pNext = nullptr;
	vkCreateCommandPool(device, &createCmdPool, NULL, pCmdPool);

	return 0;
}

static inline VkSurfaceFormatKHR choose_surface_format(VkSurfaceFormatKHR* pSurfaceFormats, uint32_t surface_format_num)
{
	VkSurfaceFormatKHR surface_foramt_priorities[] = { {VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
												   {VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR} };

	for (uint32_t i = 0; i < sizeof(surface_foramt_priorities) / sizeof(surface_foramt_priorities[0]); i++)
	{
		for (uint32_t format_index = 0; format_index < surface_format_num; format_index++)
		{
			if (surface_foramt_priorities[i].colorSpace == pSurfaceFormats[format_index].colorSpace &&
				surface_foramt_priorities[i].format == pSurfaceFormats[format_index].format)
			{
				return pSurfaceFormats[format_index];
			}
		}
	}
	return pSurfaceFormats[0];
}
static int create_render_context(VkPhysicalDevice physDevice, VkDevice device, VkSurfaceKHR surface, VkPresentModeKHR present_mode,
	VkSurfaceFormatKHR* pSurfaceFormat, VkSwapchainKHR* pSwapachain, VkImage** ppSwapchainImages, VkImageView** ppSwapchainImageViews, uint32_t* pImageNum)
{
	int ret = 0;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkImage *pSwapchainImages = NULL;
	VkImageView* pSwapchainImageViews = NULL;
	uint32_t imageNum = 0;
	VkResult result = VK_SUCCESS;

	VkSurfaceCapabilitiesKHR surface_properties;
	VkExtent2D surface_extent;

	VkSurfaceFormatKHR* pSurfaceFormats = NULL;
	uint32_t surface_format_num;

	uint32_t swapchainNum;
	uint32_t array_layers;
	VkFormatProperties formatProperties;
	VkSurfaceFormatKHR surface_format;


	VkImageUsageFlags imageUsageFlags = 0;
	VkImageUsageFlags requestedImageUsageFlags[] = { VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT };

	VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_FLAG_BITS_MAX_ENUM_KHR;

	VkPresentModeKHR present_mode_priority_list[] = {
		VK_PRESENT_MODE_MAILBOX_KHR, 
		VK_PRESENT_MODE_FIFO_KHR, 
		VK_PRESENT_MODE_IMMEDIATE_KHR,
	};

	//VkPresentModeKHR  present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
	VkPresentModeKHR* availabelPresenModes = NULL;
	uint32_t availablePresentModeNum = 0;
	uint32_t matchPresentMode = 0;

	VkSwapchainCreateInfoKHR swapchainCreateInfo{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDevice,surface, &surface_properties);
	if (surface_properties.currentExtent.width != 0xFFFFFFFFF)
	{
		surface_extent.height = surface_properties.currentExtent.height;
		surface_extent.width = surface_properties.currentExtent.width;
	}
	else
	{
		surface_extent.width = WINDOW_WIDTH;
		surface_extent.height = WINDOW_HEIGHT;
	}

	vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice, surface, &surface_format_num, nullptr);
	pSurfaceFormats = (VkSurfaceFormatKHR*)malloc(sizeof(VkSurfaceFormatKHR) * surface_format_num);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice, surface, &surface_format_num, pSurfaceFormats);

	surface_format = choose_surface_format(pSurfaceFormats, surface_format_num);

	swapchainNum = surface_properties.maxImageCount > 3 ? 3 : surface_properties.maxImageCount;
	swapchainNum = swapchainNum > surface_properties.minImageCount ? swapchainNum : surface_properties.minImageCount;

	array_layers = 1;

	vkGetPhysicalDeviceFormatProperties(physDevice, surface_format.format, &formatProperties);
	for (int i = 0; i < sizeof(requestedImageUsageFlags) / sizeof(requestedImageUsageFlags[0]); i++)
	{
		if (surface_properties.supportedUsageFlags & requestedImageUsageFlags[i])
		{
			imageUsageFlags |= requestedImageUsageFlags[i];
		}
	}

	if (imageUsageFlags == 0)
	{
		printf("cannot find suitable image flags\n");
		return -1;
	}

	if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
	{
		compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
	}
	else
	{
		VkCompositeAlphaFlagBitsKHR requestedCompsiteAlphaFlags[] = {
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
		};
		for (int i = 0; i < (sizeof(requestedCompsiteAlphaFlags) / sizeof(requestedCompsiteAlphaFlags[0])); i++)
		{
			if (surface_properties.supportedCompositeAlpha & requestedCompsiteAlphaFlags[i])
			{
				compositeAlpha = requestedCompsiteAlphaFlags[i];
				break;
			}
		}
	}

	if (compositeAlpha == VK_COMPOSITE_ALPHA_FLAG_BITS_MAX_ENUM_KHR)
	{
		printf("cannot find requested composite alpha flags! \n");
		return -1;
	}

	vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice, surface, &availablePresentModeNum, nullptr);
	availabelPresenModes = (VkPresentModeKHR*)malloc(sizeof(VkPresentModeKHR)* availablePresentModeNum);
	if (!availabelPresenModes)
	{
		goto failed;
	}

	vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice, surface, &availablePresentModeNum, availabelPresenModes);

	for (int i = 0; i < availablePresentModeNum; i++)
	{
		if (present_mode == availabelPresenModes[i])
		{
			matchPresentMode = 1;
			break;
		}
	}
	if (!matchPresentMode)
	{
		for (int i = 0; i < availablePresentModeNum; i++)
		{
			for (int j = 0; j < sizeof(present_mode_priority_list) / sizeof(present_mode_priority_list[0]); j++)
			{
				if (availabelPresenModes[i] == present_mode_priority_list[j])
				{
					present_mode = availabelPresenModes[i];
					matchPresentMode = 1;
					break;
				}
			}
		}
	}

	if (!matchPresentMode)
	{
		printf("cannot find the present mode %d in available present mode\n", present_mode);
		return -1;
	}
	swapchainCreateInfo.minImageCount = swapchainNum;
	swapchainCreateInfo.imageExtent = surface_extent;
	swapchainCreateInfo.presentMode = present_mode;
	swapchainCreateInfo.imageFormat = surface_format.format;
	swapchainCreateInfo.imageColorSpace = surface_format.colorSpace;
	swapchainCreateInfo.imageArrayLayers = array_layers;
	swapchainCreateInfo.imageUsage = imageUsageFlags;
	swapchainCreateInfo.preTransform = surface_properties.currentTransform;
	swapchainCreateInfo.compositeAlpha = compositeAlpha;
	swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
	swapchainCreateInfo.surface = surface;
	{
		PFN_vkCreateSwapchainKHR pfn_vkCreateSwapchainKHR = (PFN_vkCreateSwapchainKHR)pfn_vkGetDeviceProcAddr(device, "vkCreateSwapchainKHR");
		if(pfn_vkCreateSwapchainKHR)
		   result = pfn_vkCreateSwapchainKHR(device, &swapchainCreateInfo, nullptr, &swapchain);

	}
	if (result != VK_SUCCESS)
	{
		ret = -1;
		goto failed;
	}

	vkGetSwapchainImagesKHR(device, swapchain, &imageNum, nullptr);

	pSwapchainImages = (VkImage*)malloc(sizeof(VkImage) * imageNum);

	vkGetSwapchainImagesKHR(device, swapchain, &imageNum, pSwapchainImages);
	pSwapchainImageViews = (VkImageView*)malloc(sizeof(VkImageView) * imageNum);

	for (uint32_t i = 0; i < imageNum; i++)
	{
		VkImageViewCreateInfo view_info{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view_info.format = surface_format.format;
		view_info.image = pSwapchainImages[i];
		view_info.subresourceRange.levelCount = 1;
		view_info.subresourceRange.layerCount = 1;
		view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view_info.components.r = VK_COMPONENT_SWIZZLE_R;
		view_info.components.g = VK_COMPONENT_SWIZZLE_G;
		view_info.components.b = VK_COMPONENT_SWIZZLE_B;
		view_info.components.a = VK_COMPONENT_SWIZZLE_A;

		VkImageView image_view;
		vkCreateImageView(device, &view_info, nullptr, &image_view);

		pSwapchainImageViews[i] = image_view;
	}
	*pSurfaceFormat = surface_format;
	*pSwapachain = swapchain;
	*ppSwapchainImages = pSwapchainImages;
	*ppSwapchainImageViews = pSwapchainImageViews;
	*pImageNum = imageNum;

failed:
	if (pSurfaceFormats)
		free(pSurfaceFormats);

	if (availabelPresenModes)
		free(availabelPresenModes);

	return ret;
}

static void destroy_render_context(VkDevice device, VkSwapchainKHR swapchain, VkImage* pImages, uint32_t image_num)
{
	if (pImages)
	{
		for (uint32_t i = 0; i < image_num; i++)
		{
			vkDestroyImage(device, pImages[i],NULL);
		}
	}

	if (swapchain)
	{
		vkDestroySwapchainKHR(device, swapchain, NULL);
	}
}
static int prepare_render_context(VkPhysicalDevice physDevice, VkDevice device, 
	VkSurfaceKHR surface, VkImage* pSwapchainImages,uint32_t image_num)
{
	VkSurfaceCapabilitiesKHR surface_properties;
	VkExtent2D surface_extent;

	vkDeviceWaitIdle(device);
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDevice, surface, &surface_properties);
	if (surface_properties.currentExtent.width != 0xFFFFFFFFF)
	{
		surface_extent.height = surface_properties.currentExtent.height;
		surface_extent.width = surface_properties.currentExtent.width;
	}
	else
	{
		surface_extent.width = WINDOW_WIDTH;
		surface_extent.height = WINDOW_HEIGHT;
	}
	VkExtent3D extent{ surface_extent.width, surface_extent.height, 1 };

	for (uint32_t i = 0; i < image_num; i++)
	{

	}
	return 0;
}
static uint32_t get_memory_type(uint32_t bits, VkMemoryPropertyFlags properties, VkPhysicalDeviceMemoryProperties memory_properties, VkBool32* memory_type_found)
{
	for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++)
	{
		if ((bits & 1) == 1)
		{
			if ((memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				if (memory_type_found)
				{
					*memory_type_found = true;
				}
				return i;
			}
		}
		bits >>= 1;
	}

	if (memory_type_found)
	{
		*memory_type_found = false;
		return 0;
	}
	else
	{
		printf("Could not find a matching memory type\n");
	}
	return -1;
}
static int setup_depth_stencil(VkPhysicalDevice gpuDevice, VkDevice device, VkExtent2D surface_extent,
	VkImage* depth_stencil_image, VkDeviceMemory* depth_stencil_mem, VkImageView* depth_stencil_view)
{
	VkBool32 mem_type_found;
	VkImageCreateInfo image_create_info{};
	VkMemoryAllocateInfo memory_allocation{};
	// The GPU memory properties
	VkPhysicalDeviceMemoryProperties memory_properties;
	VkFormat depth_format = get_suitable_depth_format(gpuDevice);

	image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_create_info.imageType = VK_IMAGE_TYPE_2D;
	image_create_info.format = depth_format;
	image_create_info.extent = { surface_extent.width, surface_extent.height, 1 };
	image_create_info.mipLevels = 1;
	image_create_info.arrayLayers = 1;
	image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
	image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	vkCreateImage(device, &image_create_info, nullptr, depth_stencil_image);
	VkMemoryRequirements memReqs{};
	vkGetImageMemoryRequirements(device, *depth_stencil_image, &memReqs);

	vkGetPhysicalDeviceMemoryProperties(gpuDevice, &memory_properties);
	memory_allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memory_allocation.allocationSize = memReqs.size;
	memory_allocation.memoryTypeIndex = get_memory_type(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, memory_properties, &mem_type_found);
	vkAllocateMemory(device, &memory_allocation, nullptr, depth_stencil_mem);
	vkBindImageMemory(device, *depth_stencil_image, *depth_stencil_mem, 0);

	VkImageViewCreateInfo image_view_create_info{};
	image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	image_view_create_info.image = *depth_stencil_image;
	image_view_create_info.format = depth_format;
	image_view_create_info.subresourceRange.baseMipLevel = 0;
	image_view_create_info.subresourceRange.levelCount = 1;
	image_view_create_info.subresourceRange.baseArrayLayer = 0;
	image_view_create_info.subresourceRange.layerCount = 1;
	image_view_create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	// Stencil aspect should only be set on depth + stencil formats (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT
	if (depth_format >= VK_FORMAT_D16_UNORM_S8_UINT)
	{
		image_view_create_info.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	vkCreateImageView(device, &image_view_create_info, nullptr, depth_stencil_view);
	return 0;
}

static int setup_render_pass(VkDevice device, VkFormat color_format, VkFormat depth_format, VkRenderPass* render_pass)
{
	VkAttachmentDescription attachments[2] = { };
	VkRenderPassCreateInfo render_pass_create_info = {};
	VkAttachmentReference color_reference = {};
	VkAttachmentReference depth_reference = {};
	// Subpass dependencies for layout transitions
	VkSubpassDependency dependencies[2];
	VkSubpassDescription subpass_description = {};

	attachments[0].format = color_format;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	// Depth attachment
	attachments[1].format = depth_format;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	
	color_reference.attachment = 0;
	color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	depth_reference.attachment = 1;
	depth_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass_description.colorAttachmentCount = 1;
	subpass_description.pColorAttachments = &color_reference;
	subpass_description.pDepthStencilAttachment = &depth_reference;
	subpass_description.inputAttachmentCount = 0;
	subpass_description.pInputAttachments = nullptr;
	subpass_description.preserveAttachmentCount = 0;
	subpass_description.pPreserveAttachments = nullptr;
	subpass_description.pResolveAttachments = nullptr;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_NONE_KHR;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_create_info.attachmentCount = sizeof(attachments)/sizeof(attachments[0]);
	render_pass_create_info.pAttachments = attachments;
	render_pass_create_info.subpassCount = 1;
	render_pass_create_info.pSubpasses = &subpass_description;
	render_pass_create_info.dependencyCount = sizeof(dependencies)/sizeof(dependencies[0]);
	render_pass_create_info.pDependencies = dependencies;

	vkCreateRenderPass(device, &render_pass_create_info, nullptr, render_pass);

	return 0;
}
inline bool validate_format_feature(VkImageUsageFlagBits image_usage, VkFormatFeatureFlags supported_features)
{
	switch (image_usage)
	{
	case VK_IMAGE_USAGE_STORAGE_BIT:
		return VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT & supported_features;
	default:
		return true;
	}
}
inline VkImageUsageFlagBits choose_image_usage(VkImageUsageFlags supported_image_usage, VkFormatFeatureFlags supported_features)
{
	VkImageUsageFlagBits  validated_image_usage_flags = (VkImageUsageFlagBits)0;
	// Pick the first format from list of defaults, if supported
	static const VkImageUsageFlagBits image_usage_flags[] = {
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
		VK_IMAGE_USAGE_STORAGE_BIT,
		VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT };

	for (uint32_t i = 0; i < sizeof(image_usage_flags)/sizeof(image_usage_flags[0]); i++)
	{
		VkImageUsageFlagBits image_usage = image_usage_flags[i];
		if ((image_usage & supported_image_usage) && validate_format_feature(image_usage, supported_features))
		{
			validated_image_usage_flags = image_usage;
			break;
		}
	}
	return validated_image_usage_flags;
}

inline VkPresentModeKHR choose_present_mode(VkPresentModeKHR* available_present_modes, uint32_t present_mode_num)
{
	const VkPresentModeKHR present_mode_priority_list[] = { VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_MAILBOX_KHR };
	// If nothing found, always default to FIFO
	VkPresentModeKHR chosen_present_mode = VK_PRESENT_MODE_FIFO_KHR;

	for (uint32_t i = 0; i < sizeof(present_mode_priority_list)/sizeof(present_mode_priority_list[0]); i++)
	{
		for (uint32_t present_mode_index = 0; present_mode_index < present_mode_num; present_mode_index++)
		{
			if (present_mode_priority_list[i] == available_present_modes[present_mode_index])
			{
				chosen_present_mode = available_present_modes[present_mode_index];
				break;
			}
		}
	}
	return chosen_present_mode;
}

inline uint32_t choose_image_array_layers(uint32_t request_image_array_layers, uint32_t max_image_array_layers)
{
	request_image_array_layers = request_image_array_layers < max_image_array_layers ? request_image_array_layers : max_image_array_layers;
	request_image_array_layers = request_image_array_layers > 1u ? request_image_array_layers : 1u;

	return request_image_array_layers;
}

inline VkCompositeAlphaFlagBitsKHR choose_composite_alpha(VkCompositeAlphaFlagBitsKHR request_composite_alpha, VkCompositeAlphaFlagsKHR supported_composite_alpha)
{
	VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	if (request_composite_alpha & supported_composite_alpha)
	{
		return request_composite_alpha;
	}

	static const VkCompositeAlphaFlagBitsKHR composite_alpha_flags[] = {
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR };

	for (uint32_t i = 0; i < sizeof(composite_alpha_flags)/sizeof(composite_alpha_flags[0]); i++)
	{
		composite_alpha = composite_alpha_flags[i];
		if (composite_alpha & supported_composite_alpha)
		{
			return composite_alpha;
		}
	}

	return composite_alpha;
}

static void build_command_buffers(struct GraphicsContext* graphics_context)
{
	VkCommandBufferBeginInfo command_buffer_begin_info{ };
	VkClearColorValue default_clear_color = { {0.01f, 0.01f, 0.033f, 1.0f} };
	VkClearValue clear_values[2];
	VkRenderPassBeginInfo render_pass_begin_info{ };
	VkViewport viewport{ };
	VkRect2D scissor{ };
	VkDeviceSize offsets[1] = { 0 };

	command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	clear_values[0].color = default_clear_color;
	clear_values[1].depthStencil = { 0.0f, 0 };

	render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	render_pass_begin_info.renderPass = graphics_context->render_pass;
	render_pass_begin_info.renderArea.offset.x = 0;
	render_pass_begin_info.renderArea.offset.y = 0;
	render_pass_begin_info.renderArea.extent.width = graphics_context->surface_extent.width;
	render_pass_begin_info.renderArea.extent.height = graphics_context->surface_extent.height;
	render_pass_begin_info.clearValueCount = 2;
	render_pass_begin_info.pClearValues = clear_values;

	viewport.width = graphics_context->surface_extent.width;
	viewport.height = graphics_context->surface_extent.height;
	viewport.minDepth = 0;
	viewport.maxDepth = 1.0f;

	scissor.extent.width = graphics_context->surface_extent.width;
	scissor.extent.height = graphics_context->surface_extent.height;
	scissor.offset.x = 0;
	scissor.offset.y = 0;

	for (int32_t i = 0; i < graphics_context->image_num; i++)
	{
		// Set target frame buffer
		render_pass_begin_info.framebuffer = graphics_context->framebuffers[i];

		VK_CHECK(vkBeginCommandBuffer(graphics_context->command_buffers[i], &command_buffer_begin_info));

		vkCmdBeginRenderPass(graphics_context->command_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdSetViewport(graphics_context->command_buffers[i], 0, 1, &viewport);
		vkCmdSetScissor(graphics_context->command_buffers[i], 0, 1, &scissor);

		vkCmdBindDescriptorSets(graphics_context->command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_context->pipeline_layout, 0, 1, &graphics_context->descriptor_set, 0, NULL);
		vkCmdBindPipeline(graphics_context->command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_context->graphics_pipeline);

		vkCmdBindVertexBuffers(graphics_context->command_buffers[i], 0, 1, &graphics_context->vertex_buffer, offsets);
		//vkCmdBindIndexBuffer(graphics_context->command_buffers[i], index_buffer->get_handle(), 0, VK_INDEX_TYPE_UINT32);

		//vkCmdDrawIndexed(draw_cmd_buffers[i], index_count, 1, 0, 0, 0);
		vkCmdDraw(graphics_context->command_buffers[i], 3, 1, 0, 0);
		//draw_ui(draw_cmd_buffers[i]);

		vkCmdEndRenderPass(graphics_context->command_buffers[i]);

		VK_CHECK(vkEndCommandBuffer(graphics_context->command_buffers[i]));
	}
}

bool resize(struct GraphicsContext* graphics_context,const uint32_t width, const uint32_t height)
{
	VkResult result;
	VkSwapchainCreateInfoKHR create_info{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	VkSurfaceCapabilitiesKHR surface_capabilities{};
	VkExtent2D surface_extent;
	uint32_t surface_format_count{ 0U };
	VkSurfaceFormatKHR *surface_formats;

	uint32_t present_mode_count{ 0U };
	VkPresentModeKHR* present_modes;
	uint32_t desired_swapchain_images;

	VkFormatProperties format_properties;
	VkImageUsageFlagBits image_usage_flags;
	VkSwapchainKHR swapchain_handle;
	uint32_t image_available = 0;

	VkImage depth_stencil_image = VK_NULL_HANDLE;
	VkImageView depth_stencil_view = VK_NULL_HANDLE;
	VkDeviceMemory depth_stencil_mem = VK_NULL_HANDLE;

	VkImageView attachments[2];

	// Don't recreate the swapchain if the dimensions haven't changed
	if (width == graphics_context->surface_extent.width && height == graphics_context->surface_extent.height)
	{
		return false;
	}
	surface_extent.width = width;
	surface_extent.height = height;
	graphics_context->surface_extent = surface_extent;

	vkDeviceWaitIdle(graphics_context->device);

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(graphics_context->gpuDevice, graphics_context->display_surface, &surface_capabilities);
	vkGetPhysicalDeviceSurfaceFormatsKHR(graphics_context->gpuDevice, graphics_context->display_surface, &surface_format_count, nullptr);
	surface_formats = (VkSurfaceFormatKHR*)malloc(surface_format_count * sizeof(VkSurfaceFormatKHR));
	vkGetPhysicalDeviceSurfaceFormatsKHR(graphics_context->gpuDevice, graphics_context->display_surface, &surface_format_count, surface_formats);

	vkGetPhysicalDeviceSurfacePresentModesKHR(graphics_context->gpuDevice, graphics_context->display_surface, &present_mode_count, nullptr);
	present_modes = (VkPresentModeKHR*)malloc(present_mode_count * sizeof(VkPresentModeKHR));
	vkGetPhysicalDeviceSurfacePresentModesKHR(graphics_context->gpuDevice, graphics_context->display_surface, &present_mode_count, present_modes);

	vkGetPhysicalDeviceFormatProperties(graphics_context->gpuDevice, graphics_context->surface_format.format, &format_properties);

	image_usage_flags = choose_image_usage(surface_capabilities.supportedUsageFlags, format_properties.optimalTilingFeatures);

	// Determine the number of VkImage's to use in the swapchain.
    // Ideally, we desire to own 1 image at a time, the rest of the images can
    // either be rendered to and/or being queued up for display.
	desired_swapchain_images  = surface_capabilities.minImageCount + 1;
	if ((surface_capabilities.maxImageCount > 0) && (desired_swapchain_images > surface_capabilities.maxImageCount))
	{
		// Application must settle for fewer images than desired.
		desired_swapchain_images = surface_capabilities.maxImageCount;
	}

	create_info.minImageCount = desired_swapchain_images;
	create_info.imageExtent = surface_extent;
	create_info.presentMode = choose_present_mode(present_modes, present_mode_count);
	create_info.imageFormat = graphics_context->surface_format.format;
	create_info.imageColorSpace = graphics_context->surface_format.colorSpace;
	create_info.imageArrayLayers = choose_image_array_layers(1U, surface_capabilities.maxImageArrayLayers);
	create_info.imageUsage = image_usage_flags;
	create_info.preTransform = surface_capabilities.currentTransform;
	create_info.compositeAlpha = choose_composite_alpha(VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR, surface_capabilities.supportedCompositeAlpha);
	create_info.oldSwapchain = graphics_context->swapchain;
	create_info.surface = graphics_context->display_surface;

	result = vkCreateSwapchainKHR(graphics_context->device, &create_info, nullptr, &swapchain_handle);

	if (result != VK_SUCCESS)
	{
		printf("Cannot create Swapchain, err %d\n", result);
	}

	for (uint32_t i = 0; i < graphics_context->image_num; i++)
	{
		vkDestroyImageView(graphics_context->device, graphics_context->swapchain_image_views[i], nullptr);
		graphics_context->swapchain_image_views[i] = VK_NULL_HANDLE;
	}
	vkDestroySwapchainKHR(graphics_context->device, graphics_context->swapchain, nullptr);

	for (uint32_t i = 0; i < graphics_context->image_num; i++)
	{
		vkDestroyFramebuffer(graphics_context->device, graphics_context->framebuffers[i], nullptr);
		graphics_context->framebuffers[i] = VK_NULL_HANDLE;
	}

	free(graphics_context->swapchain_images);
	free(graphics_context->swapchain_image_views);
	free(graphics_context->framebuffers);

	vkGetSwapchainImagesKHR(graphics_context->device, swapchain_handle, &image_available, nullptr);
	graphics_context->swapchain_images = (VkImage*)malloc(image_available * sizeof(VkImage));
	graphics_context->swapchain_image_views = (VkImageView*)malloc(image_available * sizeof(VkImageView));
	graphics_context->framebuffers = (VkFramebuffer*)malloc(image_available * sizeof(VkFramebuffer));
	graphics_context->image_num = image_available;
	graphics_context->swapchain = swapchain_handle;
	vkGetSwapchainImagesKHR(graphics_context->device, swapchain_handle, &image_available, graphics_context->swapchain_images);

	for (uint32_t i = 0; i < graphics_context->image_num; i++)
	{
		VkImageViewCreateInfo color_attachment_view = {};
		color_attachment_view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		color_attachment_view.pNext = NULL;
		color_attachment_view.format = graphics_context->surface_format.format;
		color_attachment_view.components = {
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A };
		color_attachment_view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		color_attachment_view.subresourceRange.baseMipLevel = 0;
		color_attachment_view.subresourceRange.levelCount = 1;
		color_attachment_view.subresourceRange.baseArrayLayer = 0;
		color_attachment_view.subresourceRange.layerCount = 1;
		color_attachment_view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		color_attachment_view.flags = 0;

		//graphics_context->swapchain_images[i] = images[i];

		color_attachment_view.image = graphics_context->swapchain_images[i];

		vkCreateImageView(graphics_context->device, &color_attachment_view, nullptr, &graphics_context->swapchain_image_views[i]);
	}

	// Recreate the frame buffers
	vkDestroyImageView(graphics_context->device, graphics_context->depth_stencil_view, nullptr);
	vkDestroyImage(graphics_context->device, graphics_context->depth_stencil_image, nullptr);
	vkFreeMemory(graphics_context->device, graphics_context->depth_stencil_mem, nullptr);
	setup_depth_stencil(graphics_context->gpuDevice, graphics_context->device, graphics_context->surface_extent, &depth_stencil_image, &depth_stencil_mem, &depth_stencil_view);

	graphics_context->depth_stencil_view = depth_stencil_view;
	graphics_context->depth_stencil_image = depth_stencil_image;
	graphics_context->depth_stencil_mem = depth_stencil_mem;

	// Depth/Stencil attachment is the same for all frame buffers
	attachments[1] = depth_stencil_view;

	VkFramebufferCreateInfo framebuffer_create_info = {};
	framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebuffer_create_info.pNext = NULL;
	framebuffer_create_info.renderPass = graphics_context->render_pass;
	framebuffer_create_info.attachmentCount = 2;
	framebuffer_create_info.pAttachments = attachments;
	framebuffer_create_info.width = graphics_context->surface_extent.width;
	framebuffer_create_info.height = graphics_context->surface_extent.height;
	framebuffer_create_info.layers = 1;

	for (uint32_t i = 0; i < graphics_context->image_num; i++)
	{
        attachments[0] = graphics_context->swapchain_image_views[i];
		vkCreateFramebuffer(graphics_context->device, &framebuffer_create_info, nullptr, &graphics_context->framebuffers[i]);
	}

	vkResetCommandPool(graphics_context->device, graphics_context->cmd_pool, 0);

	build_command_buffers(graphics_context);

	if (surface_formats)
	{
		free(surface_formats);
	}

	if (present_modes)
	{
		free(present_modes);
	}

	return true;
}

static void present_frame(struct GraphicsContext* graphics_context, uint32_t current_buffer)
{
	VkPresentInfoKHR present_info = {};
	VkResult present_result = VK_SUCCESS;
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.pNext = NULL;
	present_info.swapchainCount = 1;
	present_info.pSwapchains = &graphics_context->swapchain;
	present_info.pImageIndices = &current_buffer;
	present_info.pWaitSemaphores = &graphics_context->render_complete_sema;
	present_info.waitSemaphoreCount = 1;

	present_result = vkQueuePresentKHR(graphics_context->graphics_queue, &present_info);

	if (!((present_result == VK_SUCCESS) || (present_result == VK_SUBOPTIMAL_KHR)))
	{
		if (present_result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			// Swap chain is no longer compatible with the surface and needs to be recreated
			resize(graphics_context, graphics_context->surface_extent.width, graphics_context->surface_extent.height);
			return;
		}
	}
}

static int update(struct GraphicsContext* graphics_context)
{
	VkSurfaceCapabilitiesKHR surface_properties;
	// Contains command buffers and semaphores to be presented to the queue
	VkSubmitInfo submit_info{ };
	/** @brief Pipeline stages used to wait at for graphics queue submissions */
	VkPipelineStageFlags submit_pipeline_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	uint32_t image_index = 0;
	VkResult result;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(graphics_context->gpuDevice, graphics_context->display_surface,&surface_properties);

	if (surface_properties.currentExtent.width != graphics_context->surface_extent.width ||
		surface_properties.currentExtent.height != graphics_context->surface_extent.height)
	{
		resize(graphics_context, surface_properties.currentExtent.width, surface_properties.currentExtent.height);
	}

	vkDeviceWaitIdle(graphics_context->device);

	result = vkAcquireNextImageKHR(graphics_context->device, graphics_context->swapchain, UINT64_MAX, 
		graphics_context->acquired_image_ready_sema, VK_NULL_HANDLE, &image_index);

	if (VK_ERROR_OUT_OF_DATE_KHR == result)
	{
		resize(graphics_context, graphics_context->surface_extent.width, graphics_context->surface_extent.height);
	}

	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = &graphics_context->acquired_image_ready_sema;
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = &graphics_context->render_complete_sema;
	submit_info.pWaitDstStageMask = &submit_pipeline_stages;

	// Command buffer to be submitted to the queue
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &graphics_context->command_buffers[image_index];

	// Submit to queue
	VK_CHECK(vkQueueSubmit(graphics_context->graphics_queue, 1, &submit_info, VK_NULL_HANDLE));

	present_frame(graphics_context, image_index);
	return 0;
}

static int application_handler(struct GraphicsContext* graphics_context, struct Window* win)
{
	event_param_t param = { 0 };
	while (win->close != 1)
	{
		update(graphics_context);
		platform_process_event(&param);
	}
	return 0;
}

static int setup_vertex_buffer(struct GraphicsContext* graphics_context)
{
	graphics_context->vertex_buffer = create_vertex_buffer(graphics_context->device, sizeof(vertices));
	graphics_context->vertex_mem = alloc_bind_bufer_memory(graphics_context, graphics_context->vertex_buffer,
		sizeof(vertices), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	update_data_to_memory(graphics_context, graphics_context->vertex_mem, 0, vertices, sizeof(vertices));

	return 0;
}

static int setup_uniform_buffer(struct GraphicsContext* graphics_context)
{
	float zoom = -2.5f;
	uint32_t width = graphics_context->surface_extent.width;
	uint32_t height = graphics_context->surface_extent.height;
	glm::vec3 camera_pos = glm::vec3();
	glm::vec3 rotation = glm::vec3(0.0, 0.0, 180.0);

	graphics_context->uniform_buffer_vs = create_uniform_buffer(graphics_context->device, sizeof(ubo_vs));
	graphics_context->uniform_memory_vs = alloc_bind_bufer_memory(graphics_context, graphics_context->uniform_buffer_vs,
		sizeof(ubo_vs), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	glm::mat4 view_matrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.5f, 0.5f, zoom));

	ubo_vs.model = view_matrix * glm::translate(glm::mat4(1.0f), camera_pos);
	ubo_vs.model = glm::rotate(ubo_vs.model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	ubo_vs.model = glm::rotate(ubo_vs.model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	ubo_vs.model = glm::rotate(ubo_vs.model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	ubo_vs.view_pos = glm::vec4(0.0f, 0.0f, -zoom, 0.0f);
	ubo_vs.projection = glm::perspective(glm::radians(60.0f), static_cast<float>(width) / static_cast<float>(height), 0.001f, 256.0f);

	update_data_to_memory(graphics_context, graphics_context->uniform_memory_vs, 0, &ubo_vs, sizeof(ubo_vs));

	return 0;
}

int main()
{
	uint32_t api_version;
	VkInstanceCreateInfo createInstanceInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
	VkInstance hInstance{ VK_NULL_HANDLE };
	struct Window *window;

	unsigned int physicalDeviceCount = 0;
	VkPhysicalDevice* physicalDevices = NULL;
	char** requestedExtensions = NULL;
	int extensionCount = 0;
	VkSurfaceKHR display_surface;
	VkPhysicalDevice curPhysDevice;
	VkSurfaceFormatKHR surface_format;
	VkSurfaceCapabilitiesKHR surface_properties;
	VkExtent2D surface_extent;

	VkDevice device;
	VkQueueFamilyProperties* pQueueFamilyProperties;
	uint32_t queueFamilyCount;

	VkFormat depth_format = VK_FORMAT_UNDEFINED;
	// Handle to the device graphics queue that command buffers are submitted to
	VkQueue queue = VK_NULL_HANDLE;

	VkCommandPool cmdPool = VK_NULL_HANDLE;

	VkCommandBufferAllocateInfo cmd_buf_alloc_info;
	VkCommandBuffer *draw_cmd_buffers = NULL;

	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	VkImage* pSwapchainImages = NULL;
	VkImageView* pSwapchainImageViews = NULL;
	uint32_t image_num = 0;

	VkSemaphoreCreateInfo sema_create_info;
	// Swap chain image presentation
	VkSemaphore acquired_image_ready_sema = VK_NULL_HANDLE;

	// Command buffer submission and execution
	VkSemaphore render_complete_sema = VK_NULL_HANDLE;

	VkFenceCreateInfo fence_create_info;
	VkFence* wait_fences = nullptr;

	VkImage depth_stencil_image;
	VkDeviceMemory depth_stencil_mem;
	VkImageView depth_stencil_view;

	VkRenderPass render_pass = VK_NULL_HANDLE;
	// Pipeline cache object
	VkPipelineCacheCreateInfo pipeline_cache_create_info = {};
	VkPipelineCache pipeline_cache = VK_NULL_HANDLE;

	VkFramebuffer* framebuffers;
	VkImageView attachments[2];
	VkFramebufferCreateInfo framebuffer_create_info = {};

	struct GraphicsContext* graphics_context = (struct GraphicsContext*)calloc(1, sizeof(struct GraphicsContext));

	vkEnumerateInstanceVersion(&api_version);

	appInfo.pApplicationName = "Simple Triangle";
	appInfo.applicationVersion = 0;
	appInfo.pEngineName = "Vulkan";
	appInfo.engineVersion = 0;
	appInfo.apiVersion = VK_MAKE_VERSION(1, 0, 0);

	findSuitableInstanceExtensions(&requestedExtensions, &extensionCount);
	createInstanceInfo.pApplicationInfo = &appInfo;
	createInstanceInfo.enabledExtensionCount = extensionCount;
	createInstanceInfo.ppEnabledExtensionNames = requestedExtensions;

	VkResult result = vkCreateInstance(&createInstanceInfo, nullptr, &hInstance);

	vkEnumeratePhysicalDevices(hInstance, &physicalDeviceCount, NULL);
	if (!physicalDeviceCount)
		return -1;
	pfn_vkGetDeviceProcAddr = (PFN_vkGetDeviceProcAddr)vkGetInstanceProcAddr(hInstance, "vkGetDeviceProcAddr");

	physicalDevices = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * physicalDeviceCount);
	vkEnumeratePhysicalDevices(hInstance, &physicalDeviceCount, physicalDevices);
	window = (struct Window*)malloc(sizeof(struct Window));
	window->handle = nullptr;
	window->close = 0;
	platform_initialization(hInstance, &display_surface, window);
	curPhysDevice = get_suitable_gpu(display_surface, physicalDevices, physicalDeviceCount);

	depth_format = get_suitable_depth_format(curPhysDevice);
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(curPhysDevice, display_surface, &surface_properties);
	if (surface_properties.currentExtent.width != 0xFFFFFFFFF)
	{
		surface_extent.height = surface_properties.currentExtent.height;
		surface_extent.width = surface_properties.currentExtent.width;
	}
	else
	{
		surface_extent.width = WINDOW_WIDTH;
		surface_extent.height = WINDOW_HEIGHT;
	}

	device = create_device(curPhysDevice);

	vkGetPhysicalDeviceQueueFamilyProperties(curPhysDevice, &queueFamilyCount, NULL);
	pQueueFamilyProperties = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * queueFamilyCount);
	if (pQueueFamilyProperties == NULL)
		goto failed;

	vkGetPhysicalDeviceQueueFamilyProperties(curPhysDevice, &queueFamilyCount, pQueueFamilyProperties);
	create_cmd_pool(device, pQueueFamilyProperties, queueFamilyCount, &cmdPool);

	create_render_context(curPhysDevice, device, display_surface, VK_PRESENT_MODE_MAILBOX_KHR,
		&surface_format, &swapchain, &pSwapchainImages, &pSwapchainImageViews , &image_num);

	sema_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	sema_create_info.pNext = nullptr;
	sema_create_info.flags = 0;

	// Create a semaphore used to synchronize image presentation
	// Ensures that the current swapchain render target has completed presentation and has been released by the presentation engine, ready for rendering
	vkCreateSemaphore(device, &sema_create_info, nullptr, &acquired_image_ready_sema);
	// Create a semaphore used to synchronize command submission
	// Ensures that the image is not presented until all commands have been sumbitted and executed
	vkCreateSemaphore(device, &sema_create_info, nullptr, &render_complete_sema);

	for (uint32_t queue_family_index = 0; queue_family_index < queueFamilyCount; queue_family_index++)
	{
		VkBool32 present_supported = VK_FALSE;
		vkGetPhysicalDeviceSurfaceSupportKHR(curPhysDevice, queue_family_index, display_surface, &present_supported);
		if (present_supported)
		{
			vkGetDeviceQueue(device, queue_family_index, 0, &queue);
			break;
		}
	}

	if (queue == VK_NULL_HANDLE)
	{
		for (uint32_t queue_family_index = 0; queue_family_index < queueFamilyCount; queue_family_index++)
		{
			if (pQueueFamilyProperties[queue_family_index].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				vkGetDeviceQueue(device, queue_family_index, 0, &queue);
				break;
			}
		}
	}
	draw_cmd_buffers = (VkCommandBuffer*)malloc(image_num * sizeof(VkCommandBuffer));

	cmd_buf_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmd_buf_alloc_info.commandPool = cmdPool;
	cmd_buf_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmd_buf_alloc_info.commandBufferCount = image_num;
	cmd_buf_alloc_info.pNext = nullptr;

	vkAllocateCommandBuffers(device, &cmd_buf_alloc_info, draw_cmd_buffers);

	wait_fences = (VkFence*)malloc(image_num * sizeof(VkFence));

	fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	fence_create_info.pNext = nullptr;

	for (uint32_t i = 0; i < image_num; i++)
	{
		vkCreateFence(device, &fence_create_info, nullptr, &wait_fences[i]);
	}

	setup_depth_stencil(curPhysDevice, device, surface_extent, 
		&depth_stencil_image, &depth_stencil_mem, &depth_stencil_view);

	setup_render_pass(device, surface_format.format, depth_format, &render_pass);
	pipeline_cache_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	vkCreatePipelineCache(device, &pipeline_cache_create_info, nullptr, &pipeline_cache);

	// Create frame buffers for every swap chain image
	framebuffers = (VkFramebuffer*)malloc(image_num * sizeof(VkFramebuffer));

	framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebuffer_create_info.pNext = NULL;
	framebuffer_create_info.renderPass = render_pass;
	framebuffer_create_info.attachmentCount = 2;
	framebuffer_create_info.pAttachments = attachments;
	framebuffer_create_info.width = surface_extent.width;
	framebuffer_create_info.height = surface_extent.height;
	framebuffer_create_info.layers = 1;

	// Depth/Stencil attachment is the same for all frame buffers
	attachments[1] = depth_stencil_view;
	for (uint32_t i = 0; i < image_num; i++)
	{
		attachments[0] = pSwapchainImageViews[i];
		vkCreateFramebuffer(device, &framebuffer_create_info, nullptr, &framebuffers[i]);
	}

	graphics_context->gpuDevice = curPhysDevice;
	graphics_context->device = device;
	graphics_context->graphics_queue = queue;
	graphics_context->display_surface = display_surface;
	graphics_context->surface_format = surface_format;
	graphics_context->surface_extent = surface_extent;
	graphics_context->swapchain = swapchain;
	graphics_context->image_num = image_num;
	graphics_context->swapchain_images = pSwapchainImages;
	graphics_context->swapchain_image_views = pSwapchainImageViews;
	graphics_context->acquired_image_ready_sema = acquired_image_ready_sema;
	graphics_context->render_complete_sema = render_complete_sema;
	graphics_context->depth_stencil_mem = depth_stencil_mem;
	graphics_context->depth_stencil_image = depth_stencil_image;
	graphics_context->depth_stencil_view = depth_stencil_view;
	graphics_context->render_pass = render_pass;
	graphics_context->framebuffers = framebuffers;
	graphics_context->cmd_pool = cmdPool;
	graphics_context->command_buffers = draw_cmd_buffers;
	graphics_context->pipeline_cache = pipeline_cache;

	setup_vertex_buffer(graphics_context);
	setup_uniform_buffer(graphics_context);
	setup_descriptor_set_layout(graphics_context);
	setup_graphics_pipeline(graphics_context);
	setup_descriptors(graphics_context);
	build_command_buffers(graphics_context);

	application_handler(graphics_context, window);
failed:
	if (requestedExtensions)
		free(requestedExtensions);

	if (pQueueFamilyProperties)
		free(pQueueFamilyProperties);
	if (graphics_context->swapchain_image_views)
	{
		for (uint32_t i = 0; i < image_num; i++)
		{
			vkDestroyImageView(device, graphics_context->swapchain_image_views[i], nullptr);
		}
		free(graphics_context->swapchain_image_views);
	}

	destroy_render_context(device, graphics_context->swapchain, graphics_context->swapchain_images, graphics_context->image_num);

	if (graphics_context->swapchain_images)
		free(graphics_context->swapchain_images);

	destroy_buffer(device, graphics_context->vertex_buffer);
	free_memory(device, graphics_context->vertex_mem);

	destroy_buffer(device, graphics_context->uniform_buffer_vs);
	free_memory(device, graphics_context->uniform_memory_vs);

	if (graphics_context->command_buffers)
	{
		vkFreeCommandBuffers(device, cmdPool, graphics_context->image_num, graphics_context->command_buffers);
	}

	if (cmdPool)
	{
		vkDestroyCommandPool(device, cmdPool, NULL);
		cmdPool = NULL;
	}

	if (graphics_context->depth_stencil_view)
	{
		vkDestroyImageView(device, graphics_context->depth_stencil_view, nullptr);
	}
	
	if (graphics_context->depth_stencil_image)
	{
		vkDestroyImage(device, graphics_context->depth_stencil_image, nullptr);
	}

	if (graphics_context->depth_stencil_mem)
	{
		vkFreeMemory(device, graphics_context->depth_stencil_mem, nullptr);
	}

	if (graphics_context->framebuffers)
	{
		for (uint32_t i = 0; i < graphics_context->image_num; i++)
		{
			vkDestroyFramebuffer(device, graphics_context->framebuffers[i], nullptr);
		}
	}
	if (graphics_context->render_pass)
	{
		vkDestroyRenderPass(device, graphics_context->render_pass, nullptr);
	}

	destroy_graphics_pipeline(graphics_context);

	if (acquired_image_ready_sema)
	{
		vkDestroySemaphore(device, acquired_image_ready_sema, nullptr);
	}

	if (render_complete_sema)
	{
		vkDestroySemaphore(device, render_complete_sema, nullptr);
	}

	if (wait_fences)
	{
		for (uint32_t i = 0; i < graphics_context->image_num; i++)
		{
			vkDestroyFence(device, wait_fences[i], nullptr);
		}
	}

	if (device)
	{
		vkDestroyDevice(device, NULL);
		device = NULL;
	}

	if (display_surface)
	{
		vkDestroySurfaceKHR(hInstance, display_surface, NULL);
		display_surface = NULL;
	}

	platform_deinitialization(window->handle);

	if (window)
	{
		free(window);
	}

	if (hInstance)
	{
		vkDestroyInstance(hInstance, NULL);
		hInstance = NULL;
	}

	if (physicalDevices)
	{
		free(physicalDevices);
	}

	return 0;
}