#pragma once
#include "common.h"

extern VkDeviceMemory alloc_bind_bufer_memory(struct GraphicsContext* graphics_context, VkBuffer buffer, 
	VkDeviceSize size, VkMemoryPropertyFlags required_flags,
	VkMemoryPropertyFlags preferred_flags);
extern VkResult update_data_to_memory(struct GraphicsContext *graphics_context, VkDeviceMemory mem, 
	uint32_t offset, void* data, uint32_t size);
extern int free_memory(VkDevice device, VkDeviceMemory mem);