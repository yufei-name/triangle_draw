#pragma once
#define WINDOW_WIDTH 500
#define WINDOW_HEIGHT 500
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
extern int platform_initialization(VkInstance vkInst, VkSurfaceKHR* surface);
const char** get_platform_extension(unsigned int* platform_extension_num);

struct GraphicsContext
{
	VkPhysicalDevice gpuDevice; 
	VkDevice device;
	VkQueue graphics_queue;
	VkSurfaceKHR display_surface;
	VkSurfaceFormatKHR surface_format;
	VkExtent2D surface_extent;
	VkSwapchainKHR swapchain;
	uint32_t image_num;
	VkImage* swapchain_images;
	VkImageView* swapchain_image_views;

	// Swap chain image presentation
	VkSemaphore acquired_image_ready_sema;
	// Command buffer submission and execution
	VkSemaphore render_complete_sema;

	VkDeviceMemory depth_stencil_mem;
	VkImage depth_stencil_image;
	VkImageView depth_stencil_view;
	// Global render pass for frame buffer writes
	VkRenderPass render_pass;
	VkFramebuffer* framebuffers;
	// Command buffer pool
	VkCommandPool cmd_pool;
	VkCommandBuffer* command_buffers;

	VkBuffer vertex_buffer;
	VkDeviceMemory vertex_mem;
	
	VkBuffer uniform_buffer_vs;
	VkDeviceMemory uniform_memory_vs;

	VkPipeline            graphics_pipeline;
	VkPipelineLayout      pipeline_layout;

	VkDescriptorPool      descriptor_pool;
	VkDescriptorSet       descriptor_set;
	VkDescriptorSetLayout descriptor_set_layout;

	// List of shader modules created (stored for cleanup)
	VkShaderModule shader_module_vs;
	VkShaderModule shader_module_ps;

	// Pipeline cache object
	VkPipelineCache pipeline_cache;
};
struct Vertex
{
	glm::vec2 position;
	glm::vec3 color;
};

typedef struct event_param{
	int type;
	int value;
}event_param_t;

extern int platform_process_event(event_param_t* event_para);
extern const char* vk_result_to_string(VkResult result);

/// @brief Helper macro to test the result of Vulkan calls which can return an error.
#define VK_CHECK(x)                                                                    \
	do                                                                                 \
	{                                                                                  \
		VkResult err = x;                                                              \
		if (err)                                                                       \
		{                                                                              \
			printf("Detected Vulkan error: %s\n", vk_result_to_string(err));           \
            exit(0);                                                                   \
		}                                                                              \
	} while (0)

static inline uint32_t count_bit_set(uint32_t v)
{
	uint32_t c = v - ((v >> 1) & 0x55555555);
	c = ((c >> 2) & 0x33333333) + (c & 0x33333333);
	c = ((c >> 4) + c) & 0x0F0F0F0F;
	c = ((c >> 8) + c) & 0x00FF00FF;
	c = ((c >> 16) + c) & 0x0000FFFF;
	return c;
}