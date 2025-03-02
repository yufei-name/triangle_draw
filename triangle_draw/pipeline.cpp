#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "pipeline.h"
#include "shader.h"

VkResult setup_graphics_pipeline(struct GraphicsContext* graphics_context)
{
	VkResult res = VK_SUCCESS;

	VkPipelineInputAssemblyStateCreateInfo input_assembly_state = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	VkPipelineRasterizationStateCreateInfo rasterization_state = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	VkPipelineColorBlendAttachmentState blend_attachment_state = {0};
	VkPipelineColorBlendStateCreateInfo color_blend_state = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
	// Note: Using reversed depth-buffer for increased precision, so Greater depth values are kept
	VkPipelineDepthStencilStateCreateInfo depth_stencil_state = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	VkPipelineViewportStateCreateInfo viewport_state = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	VkPipelineMultisampleStateCreateInfo multisample_state = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	VkDynamicState dynamic_state_enables[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dynamic_state = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	// Load shaders
	VkPipelineShaderStageCreateInfo shader_stages[2];

	// Vertex bindings and attributes
	VkVertexInputBindingDescription vertex_input_bindings[] = {
		{
		   0, //binding
	       sizeof(struct Vertex), // stride
	       VK_VERTEX_INPUT_RATE_VERTEX, //inputRate
		},
	};

	VkVertexInputAttributeDescription vertex_input_attributes[] = {
		{
		  .location = 0, // location
	      .binding = 0, // binding
	      .format = VK_FORMAT_R32G32_SFLOAT,//format
	      .offset = 0, // offset
		},// position
		{
		 .location = 1, 
		 .binding = 0, 
		 .format = VK_FORMAT_R32G32B32_SFLOAT, 
		 .offset = offsetof(Vertex, color)
		}// color

	};

	VkPipelineVertexInputStateCreateInfo vertex_input_state = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

	VkGraphicsPipelineCreateInfo pipeline_create_info = { VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

	input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly_state.flags = 0;
	input_assembly_state.primitiveRestartEnable = VK_FALSE;

	rasterization_state.polygonMode = VK_POLYGON_MODE_FILL;
	rasterization_state.cullMode = VK_CULL_MODE_NONE;
	rasterization_state.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterization_state.flags = 0;
	rasterization_state.depthClampEnable = VK_FALSE;
	rasterization_state.lineWidth = 1.0f;

	blend_attachment_state.colorWriteMask = 0xf;
	blend_attachment_state.blendEnable = VK_FALSE;

	color_blend_state.attachmentCount = 1;
	color_blend_state.pAttachments = &blend_attachment_state;

	depth_stencil_state.depthTestEnable = VK_TRUE;
	depth_stencil_state.depthWriteEnable = VK_TRUE;
	depth_stencil_state.depthCompareOp = VK_COMPARE_OP_GREATER;
	depth_stencil_state.front = depth_stencil_state.back;
	depth_stencil_state.back.compareOp = VK_COMPARE_OP_GREATER;

	viewport_state.viewportCount = 1;
	viewport_state.scissorCount = 1;
	viewport_state.flags = 0;

	multisample_state.rasterizationSamples = VK_SAMPLE_COUNT_8_BIT;
	multisample_state.flags = 0;

	dynamic_state.pDynamicStates = dynamic_state_enables;
	dynamic_state.dynamicStateCount = sizeof(dynamic_state_enables)/sizeof(dynamic_state_enables[0]);
	dynamic_state.flags = 0;

	shader_stages[0] = load_shader(graphics_context, "triangle.vert", VK_SHADER_STAGE_VERTEX_BIT);
	shader_stages[1] = load_shader(graphics_context, "triangle.frag", VK_SHADER_STAGE_FRAGMENT_BIT);

	vertex_input_state.vertexBindingDescriptionCount = sizeof(vertex_input_bindings) / sizeof(vertex_input_bindings[0]);
	vertex_input_state.pVertexBindingDescriptions = vertex_input_bindings;
	vertex_input_state.vertexAttributeDescriptionCount = sizeof(vertex_input_attributes) / sizeof(vertex_input_attributes[0]);
	vertex_input_state.pVertexAttributeDescriptions = vertex_input_attributes;

	pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_create_info.layout = graphics_context->pipeline_layout;
	pipeline_create_info.renderPass = graphics_context->render_pass;
	pipeline_create_info.flags = 0;
	pipeline_create_info.basePipelineIndex = -1;
	pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_create_info.pVertexInputState = &vertex_input_state;
	pipeline_create_info.pInputAssemblyState = &input_assembly_state;
	pipeline_create_info.pRasterizationState = &rasterization_state;
	pipeline_create_info.pColorBlendState = &color_blend_state;
	pipeline_create_info.pMultisampleState = &multisample_state;
	pipeline_create_info.pViewportState = &viewport_state;
	pipeline_create_info.pDepthStencilState = &depth_stencil_state;
	pipeline_create_info.pDynamicState = &dynamic_state;
	pipeline_create_info.stageCount = sizeof(shader_stages)/sizeof(shader_stages[0]);
	pipeline_create_info.pStages = shader_stages;

	VK_CHECK(vkCreateGraphicsPipelines(graphics_context->device, graphics_context->pipeline_cache, 1, &pipeline_create_info, nullptr, &graphics_context->graphics_pipeline));
	//VK_CHECK(vkCreateGraphicsPipelines(graphics_context->device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &graphics_context->graphics_pipeline));

	// Pipeline is baked, we can delete the shader modules now.
	vkDestroyShaderModule(graphics_context->device, shader_stages[0].module, nullptr);
	vkDestroyShaderModule(graphics_context->device, shader_stages[1].module, nullptr);

	return res;
}

VkResult destroy_graphics_pipeline(struct GraphicsContext* graphics_context)
{
	vkDestroyPipeline(graphics_context->device,graphics_context->graphics_pipeline, nullptr);
	vkDestroyPipelineLayout(graphics_context->device, graphics_context->pipeline_layout, nullptr);
	vkDestroyPipelineCache(graphics_context->device, graphics_context->pipeline_cache, nullptr);
	return VK_SUCCESS;
}