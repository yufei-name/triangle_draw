#include<stdio.h>
#include<stdlib.h>
#include "buffer.h"

VkBuffer create_buffer(VkDevice device, VkDeviceSize size, VkBufferUsageFlags usage)
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VkBufferCreateInfo bci;
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.usage = usage;
    bci.size = size;
    bci.queueFamilyIndexCount = 0;
    bci.pQueueFamilyIndices = nullptr;
    bci.flags = 0;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bci.pNext = nullptr;
    VK_CHECK(vkCreateBuffer(device, &bci, nullptr, &buffer));

    return buffer;
}

void destroy_buffer(VkDevice device, VkBuffer buffer)
{
    vkDestroyBuffer(device, buffer, nullptr);
}