#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

VkDeviceMemory alloc_bind_bufer_memory(struct GraphicsContext* graphics_context, VkBuffer buffer,
    VkDeviceSize size, VkMemoryPropertyFlags required_flags, VkMemoryPropertyFlags preferred_flags)
{
	//VkBufferMemoryRequirementsInfo2KHR memReqInfo = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_REQUIREMENTS_INFO_2_KHR };
	//VkMemoryDedicatedRequirementsKHR memDedicatedReq = { VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS_KHR };
	//VkMemoryRequirements2KHR memReq2 = { VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2_KHR };
    VkMemoryRequirements memReq = { 0 };
    VkMemoryAllocateInfo mem_alloc_info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    VkBindBufferMemoryInfoKHR bindBufferMemoryInfo = { VK_STRUCTURE_TYPE_BIND_BUFFER_MEMORY_INFO_KHR };

	VkPhysicalDeviceMemoryProperties mem_properties;
    VkDeviceMemory mem_handle = VK_NULL_HANDLE;
	uint32_t memoryTypeBits;
	uint32_t min_cost = UINT32_MAX;
    uint32_t best_mem_type_index = UINT32_MAX;
    VkDeviceSize memoryOffset = { 0 };

	//memReqInfo.buffer = buffer;

	//memReq2.pNext = &memDedicatedReq;
	//memDedicatedReq.pNext = nullptr;
    memReq.size = size;
	//vkGetBufferMemoryRequirements2KHR(graphics_context->device, &memReqInfo, &memReq2);
    vkGetBufferMemoryRequirements(graphics_context->device, buffer, &memReq);
	memoryTypeBits = memReq.memoryTypeBits;

	vkGetPhysicalDeviceMemoryProperties(graphics_context->gpuDevice, &mem_properties);

    for (uint32_t mem_type_index = 0, mem_type_bit = 1; 
        mem_type_index < mem_properties.memoryTypeCount;
        mem_type_index++, mem_type_bit <<= 1)
    {
        // This memory type is acceptable according to memoryTypeBits bitmask.
        if ((mem_type_bit & memoryTypeBits) != 0)
        {
            const VkMemoryPropertyFlags currFlags = mem_properties.memoryTypes[mem_type_index].propertyFlags;
            // This memory type contains requiredFlags.
            if ((required_flags & ~currFlags) == 0)
            {
                // Calculate cost as number of bits from preferredFlags not present in this memory type.
                uint32_t currCost = count_bit_set(preferred_flags & ~currFlags);
                // Remember memory type with lowest cost.
                if (currCost < min_cost)
                {
                    best_mem_type_index = mem_type_index;
                    if (currCost == 0)
                    {
                        break;
                    }
                    min_cost = currCost;
                }
            }
        }
    }

    mem_alloc_info.allocationSize = size;
    mem_alloc_info.memoryTypeIndex = best_mem_type_index;
    mem_alloc_info.pNext = nullptr;
    
    VK_CHECK(vkAllocateMemory(graphics_context->device, &mem_alloc_info, nullptr, &mem_handle));

    bindBufferMemoryInfo.pNext = nullptr;
    bindBufferMemoryInfo.buffer = buffer;
    bindBufferMemoryInfo.memory = mem_handle;
    bindBufferMemoryInfo.memoryOffset = memoryOffset;

    //vkBindBufferMemory2KHR(graphics_context->device, 1, &bindBufferMemoryInfo);
    vkBindBufferMemory(graphics_context->device, buffer, mem_handle, 0);

    return mem_handle;
}

VkResult update_data_to_memory(struct GraphicsContext* graphics_context, VkDeviceMemory mem, uint32_t offset, void* data, uint32_t size)
{
    void* pMappedData = nullptr;
    VkResult result = VK_SUCCESS;

    result = vkMapMemory(graphics_context->device, mem, (VkDeviceSize)offset, (VkDeviceSize)size, 0, &pMappedData);
    if (result != VK_SUCCESS)
        return result;

    memcpy(pMappedData, data, size);
    vkUnmapMemory(graphics_context->device, mem);

    return result;
}

int free_memory(VkDevice device, VkDeviceMemory mem)
{
    vkFreeMemory(device, mem, nullptr);
    return 0;
}