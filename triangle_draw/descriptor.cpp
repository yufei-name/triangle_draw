#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "descriptor.h"

static inline VkDescriptorSetLayoutBinding descriptor_set_layout_binding(
	VkDescriptorType   type,
	VkShaderStageFlags flags,
	uint32_t           binding,
	uint32_t           count = 1)
{
	VkDescriptorSetLayoutBinding set_layout_binding{};
	set_layout_binding.descriptorType = type;
	set_layout_binding.stageFlags = flags;
	set_layout_binding.binding = binding;
	set_layout_binding.descriptorCount = count;
	return set_layout_binding;
}

static inline VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info(
	const VkDescriptorSetLayoutBinding* bindings,
	uint32_t                            binding_count)
{
	VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info{};
	descriptor_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptor_set_layout_create_info.pBindings = bindings;
	descriptor_set_layout_create_info.bindingCount = binding_count;
	return descriptor_set_layout_create_info;
}

static inline VkDescriptorPoolSize descriptor_pool_size(
	VkDescriptorType type,
	uint32_t         count)
{
	VkDescriptorPoolSize descriptor_pool_size{};
	descriptor_pool_size.type = type;
	descriptor_pool_size.descriptorCount = count;
	return descriptor_pool_size;
}

inline VkWriteDescriptorSet write_descriptor_set(
	VkDescriptorSet         dst_set,
	VkDescriptorType        type,
	uint32_t                binding,
	VkDescriptorBufferInfo* buffer_info,
	uint32_t                descriptor_count = 1)
{
	VkWriteDescriptorSet write_descriptor_set{};
	write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write_descriptor_set.dstSet = dst_set;
	write_descriptor_set.descriptorType = type;
	write_descriptor_set.dstBinding = binding;
	write_descriptor_set.pBufferInfo = buffer_info;
	write_descriptor_set.descriptorCount = descriptor_count;
	return write_descriptor_set;
}

int setup_descriptor_set_layout(struct GraphicsContext* graphics_context)
{
	VkDescriptorSetLayoutBinding set_layout_bindings[] =
	{
		// Binding 0 : Vertex shader uniform buffer
		descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_SHADER_STAGE_VERTEX_BIT,
			0),
			// Binding 1 : Fragment shader image sampler
	    descriptor_set_layout_binding(
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			1)
	};

	VkDescriptorSetLayoutCreateInfo descriptor_layout = descriptor_set_layout_create_info(
		set_layout_bindings, sizeof(set_layout_bindings)/sizeof(set_layout_bindings[0]));
	VkPipelineLayoutCreateInfo pipeline_layout_create_info;

	VK_CHECK(vkCreateDescriptorSetLayout(graphics_context->device, &descriptor_layout, nullptr, &graphics_context->descriptor_set_layout));

	pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_create_info.setLayoutCount = 1;
	pipeline_layout_create_info.pSetLayouts = &graphics_context->descriptor_set_layout;
	pipeline_layout_create_info.pNext = nullptr;
	pipeline_layout_create_info.flags = 0;
	pipeline_layout_create_info.pushConstantRangeCount = 0;

	VK_CHECK(vkCreatePipelineLayout(graphics_context->device, &pipeline_layout_create_info, nullptr, &graphics_context->pipeline_layout));

	return 0;
}

int setup_descriptors(struct GraphicsContext* graphics_context)
{
	VkDescriptorPoolSize pool_sizes[] =
	{
		descriptor_pool_size(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
		descriptor_pool_size(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1) };

	VkDescriptorPoolCreateInfo descriptor_pool_create_info{ };
	VkDescriptorSetAllocateInfo alloc_info{ };
	VkDescriptorBufferInfo buffer_descriptor{ };

	descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptor_pool_create_info.poolSizeCount = sizeof(pool_sizes) / sizeof(pool_sizes[0]);
	descriptor_pool_create_info.pPoolSizes = pool_sizes;
	descriptor_pool_create_info.maxSets = 2;

	VK_CHECK(vkCreateDescriptorPool(graphics_context->device, &descriptor_pool_create_info, nullptr, &graphics_context->descriptor_pool));

	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = graphics_context->descriptor_pool;
	alloc_info.pSetLayouts = &graphics_context->descriptor_set_layout;
	alloc_info.descriptorSetCount = 1;

	VK_CHECK(vkAllocateDescriptorSets(graphics_context->device, &alloc_info, &graphics_context->descriptor_set));

	buffer_descriptor.buffer = graphics_context->uniform_buffer_vs;
	buffer_descriptor.offset = 0;
	buffer_descriptor.range = VK_WHOLE_SIZE;
	// Setup a descriptor image info for the current texture to be used as a combined image sampler
	//VkDescriptorImageInfo image_descriptor;
	//image_descriptor.imageView = texture.view;                // The image's view (images are never directly accessed by the shader, but rather through views defining subresources)
	//image_descriptor.sampler = texture.sampler;             // The sampler (Telling the pipeline how to sample the texture, including repeat, border, etc.)
	//image_descriptor.imageLayout = texture.image_layout;        // The current layout of the image (Note: Should always fit the actual use, e.g. shader read)

	VkWriteDescriptorSet write_descriptor_sets[] =
	{
		// Binding 0 : Vertex shader uniform buffer
		write_descriptor_set(
			graphics_context->descriptor_set,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			0,
			&buffer_descriptor),
			// Binding 1 : Fragment shader texture sampler
			//	Fragment shader: layout (binding = 1) uniform sampler2D samplerColor;
			//  vkb::initializers::write_descriptor_set(
			//  descriptor_set,
			//  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,        // The descriptor set will use a combined image sampler (sampler and image could be split)
			//  1,                                                // Shader binding point 1
			//  &image_descriptor)                                // Pointer to the descriptor image for our texture
	};

	vkUpdateDescriptorSets(graphics_context->device, sizeof(write_descriptor_sets) / sizeof(write_descriptor_sets[0]), write_descriptor_sets, 0, NULL);
	return 0;
}

int destroy_descriptors(struct GraphicsContext* graphics_context)
{
	vkFreeDescriptorSets(graphics_context->device, graphics_context->descriptor_pool, 1, &graphics_context->descriptor_set);
	vkDestroyDescriptorSetLayout(graphics_context->device, graphics_context->descriptor_set_layout, nullptr);
	vkDestroyDescriptorPool(graphics_context->device, graphics_context->descriptor_pool, nullptr);
	return 0;
}