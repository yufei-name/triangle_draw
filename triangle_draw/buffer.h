#pragma once
#include "common.h"
//extern VkBuffer create_vertex_buffer(VkDevice device, VkDeviceSize size);
extern VkBuffer create_buffer(VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage);
extern void destroy_buffer(VkDevice device, VkBuffer buffer);
//extern VkBuffer create_uniform_buffer(VkDevice device, VkDeviceSize size);
