#include<stdio.h>
#include<stdlib.h>
#include "buffer.h"

VkBuffer create_vertex_buffer(VkDevice device, VkDeviceSize size)
{
    VkBuffer vertex_buffer = VK_NULL_HANDLE;
    VkBufferCreateInfo bci;
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bci.size = size;
    bci.queueFamilyIndexCount = 0;
    bci.pQueueFamilyIndices = nullptr;
    bci.flags = 0;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    bci.pNext = nullptr;
    VK_CHECK(vkCreateBuffer(device, &bci, nullptr, &vertex_buffer));
 
    return vertex_buffer;
}

VkBuffer create_uniform_buffer(VkDevice device, VkDeviceSize size)
{
    VkBuffer uniform_buffer = VK_NULL_HANDLE;
    VkBufferCreateInfo bci{ };
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bci.size = size;
    bci.queueFamilyIndexCount = 0;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(device, &bci, nullptr, &uniform_buffer));

    return uniform_buffer;
}
void destroy_buffer(VkDevice device, VkBuffer buffer)
{
    vkDestroyBuffer(device, buffer, nullptr);
}