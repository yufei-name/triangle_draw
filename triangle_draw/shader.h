#pragma once
#include "common.h"
struct GraphicsContext;
extern VkPipelineShaderStageCreateInfo load_shader(struct GraphicsContext* graphics_context, const char* shader_filename, VkShaderStageFlagBits stage);
