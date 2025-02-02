#include <glslang/Include/glslang_c_interface.h>
// Required for use of glslang_default_resource
#include <glslang/Public/resource_limits_c.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <SPIRV/GlslangToSpv.h>

#include<shaderc/shaderc.h>
#include <stdio.h>
#include <string.h>
#include "shader.h"

typedef struct SpirVBinary {
    uint32_t* words; // SPIR-V words
    size_t size; // number of words in SPIR-V binary
} SpirVBinary;
static shaderc_shader_kind vulkan_stage_to_shaderc_kind(VkShaderStageFlagBits stage)
{
    switch (stage)
    {
    case VK_SHADER_STAGE_VERTEX_BIT:
        return shaderc_vertex_shader;
    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        return shaderc_tess_control_shader;
    case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        return shaderc_tess_evaluation_shader;
    case VK_SHADER_STAGE_GEOMETRY_BIT:
        return shaderc_geometry_shader;
    case VK_SHADER_STAGE_FRAGMENT_BIT:
        return shaderc_fragment_shader;
    case VK_SHADER_STAGE_COMPUTE_BIT:
        return shaderc_compute_shader;
    case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
        return shaderc_anyhit_shader;
    case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
        return shaderc_closesthit_shader;
    case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
        return shaderc_callable_shader;
    case VK_SHADER_STAGE_MISS_BIT_KHR:
        return shaderc_miss_shader;
    case VK_SHADER_STAGE_TASK_BIT_NV:
        return shaderc_task_shader;
    case VK_SHADER_STAGE_MESH_BIT_NV:
        return shaderc_mesh_shader;
    default:
        return shaderc_glsl_infer_from_source;
    }
}
static SpirVBinary compile_glsl_to_spirv(const char* shaderSource, uint32_t length, VkShaderStageFlagBits stage, const char* fileName)
{
    SpirVBinary bin = {
       .words = NULL,
       .size = 0,
    };
    shaderc_compiler_t compiler = shaderc_compiler_initialize();
    shaderc_compilation_result_t result;
    shaderc_shader_kind shader_kind = vulkan_stage_to_shaderc_kind(stage);
    shaderc_compilation_status compilation_result = shaderc_compilation_status_success;
    shaderc_compile_options_t options = shaderc_compile_options_initialize();
    // Target Vulkan 1.0 (or higher)
    shaderc_compile_options_set_target_env(
        options,
        shaderc_target_env_vulkan,
        shaderc_env_version_vulkan_1_0
    );

    result = shaderc_compile_into_spv(compiler, shaderSource, length,
        shader_kind, fileName, "main", nullptr);

    compilation_result = shaderc_result_get_compilation_status(result);

    if (compilation_result != shaderc_compilation_status_success)
    {
        const char* err_msg = shaderc_result_get_error_message(result);
        printf("compilation status %d\n", compilation_result);
        printf("error message: \n %s\n", err_msg);
        shaderc_result_release(result);
        shaderc_compile_options_release(options);
        shaderc_compiler_release(compiler);
        return bin;
    }

    bin.size = shaderc_result_get_length(result);
    bin.words = (uint32_t*)malloc(bin.size);
    if (!bin.words)
    {
        bin.size = 0;
        goto exit;
    }
    memcpy(bin.words, shaderc_result_get_bytes(result), bin.size);
exit:
    shaderc_result_release(result);
    shaderc_compile_options_release(options);
    shaderc_compiler_release(compiler);
    return bin;
}

inline EShLanguage FindShaderLanguage(VkShaderStageFlagBits stage)
{
    switch (stage)
    {
    case VK_SHADER_STAGE_VERTEX_BIT:
        return EShLangVertex;

    case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        return EShLangTessControl;

    case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        return EShLangTessEvaluation;

    case VK_SHADER_STAGE_GEOMETRY_BIT:
        return EShLangGeometry;

    case VK_SHADER_STAGE_FRAGMENT_BIT:
        return EShLangFragment;

    case VK_SHADER_STAGE_COMPUTE_BIT:
        return EShLangCompute;

    case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
        return EShLangRayGen;

    case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
        return EShLangAnyHit;

    case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
        return EShLangClosestHit;

    case VK_SHADER_STAGE_MISS_BIT_KHR:
        return EShLangMiss;

    case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
        return EShLangIntersect;

    case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
        return EShLangCallable;

    case VK_SHADER_STAGE_MESH_BIT_EXT:
        return EShLangMesh;

    case VK_SHADER_STAGE_TASK_BIT_EXT:
        return EShLangTask;

    default:
        return EShLangVertex;
    }
}

bool compile_to_spirv(VkShaderStageFlagBits       stage,
    const char *glsl_source,
    const char* entry_point,
    std::vector<unsigned int>& spirv,
    std::string& info_log)
{
    const char* file_name_list[1] = { "" };
    // Initialize glslang library.
    glslang::InitializeProcess();
    EShMessages messages = static_cast<EShMessages>(EShMsgDefault | EShMsgVulkanRules | EShMsgSpvRules);
    EShLanguage language = FindShaderLanguage(stage);
    glslang::TShader shader(language);

    shader.setStringsWithLengthsAndNames(&glsl_source, nullptr, file_name_list, 1);
    shader.setEntryPoint(entry_point);
    shader.setSourceEntryPoint(entry_point);
    //shader.setPreamble(shader_variant.get_preamble().c_str());
    //shader.addProcesses(shader_variant.get_processes());

    shader.setEnvInput(glslang::EShSourceGlsl, language, glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
    shader.setEnvTarget(glslang::EshTargetSpv, glslang::EShTargetSpv_1_0);

    if (!shader.parse(GetDefaultResources(), 100, false, messages))
    {
        info_log = std::string(shader.getInfoLog()) + "\n" + std::string(shader.getInfoDebugLog());
        return false;
    }

    // Add shader to new program object.
    glslang::TProgram program;
    program.addShader(&shader);

    // Link program.
    if (!program.link(messages))
    {
        info_log = std::string(program.getInfoLog()) + "\n" + std::string(program.getInfoDebugLog());
        return false;
    }

    // Save any info log that was generated.
    if (shader.getInfoLog())
    {
        info_log += std::string(shader.getInfoLog()) + "\n" + std::string(shader.getInfoDebugLog()) + "\n";
    }

    if (program.getInfoLog())
    {
        info_log += std::string(program.getInfoLog()) + "\n" + std::string(program.getInfoDebugLog());
    }

    glslang::TIntermediate* intermediate = program.getIntermediate(language);

    // Translate to SPIRV.
    if (!intermediate)
    {
        info_log += "Failed to get shared intermediate code.\n";
        return false;
    }

    spv::SpvBuildLogger logger;

    glslang::GlslangToSpv(*intermediate, spirv, &logger);

    info_log += logger.getAllMessages() + "\n";

    // Shutdown glslang library.
    glslang::FinalizeProcess();

    return true;
}
SpirVBinary compileShaderToSPIRV_Vulkan(glslang_stage_t stage, const char* shaderSource, const char* fileName) {
    glslang_input_t input;
    input.language = GLSLANG_SOURCE_GLSL;
    input.stage = stage;
    input.client = GLSLANG_CLIENT_VULKAN;
    input.client_version = GLSLANG_TARGET_VULKAN_1_0;
    input.target_language = GLSLANG_TARGET_SPV;
    input.target_language_version = GLSLANG_TARGET_SPV_1_0;
    input.code = shaderSource;
    input.default_version = 100,
    input.default_profile = GLSLANG_NO_PROFILE;
    input.force_default_version_and_profile = false;
    input.forward_compatible = false;
    input.messages = GLSLANG_MSG_DEFAULT_BIT;
    input.resource = glslang_default_resource();

    glslang_shader_t* shader = glslang_shader_create(&input);

    SpirVBinary bin = {
        NULL,
        0,
    };
    if (!glslang_shader_preprocess(shader, &input)) {
        printf("GLSL preprocessing failed %s\n", fileName);
        printf("%s\n", glslang_shader_get_info_log(shader));
        printf("%s\n", glslang_shader_get_info_debug_log(shader));
        printf("%s\n", input.code);
        glslang_shader_delete(shader);
        return bin;
    }

    if (!glslang_shader_parse(shader, &input)) {
        printf("GLSL parsing failed %s\n", fileName);
        printf("%s\n", glslang_shader_get_info_log(shader));
        printf("%s\n", glslang_shader_get_info_debug_log(shader));
        printf("%s\n", glslang_shader_get_preprocessed_code(shader));
        glslang_shader_delete(shader);
        return bin;
    }

    glslang_program_t* program = glslang_program_create();
    glslang_program_add_shader(program, shader);

    if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT)) {
        printf("GLSL linking failed %s\n", fileName);
        printf("%s\n", glslang_program_get_info_log(program));
        printf("%s\n", glslang_program_get_info_debug_log(program));
        glslang_program_delete(program);
        glslang_shader_delete(shader);
        return bin;
    }

    glslang_program_SPIRV_generate(program, stage);

    bin.size = glslang_program_SPIRV_get_size(program);
    bin.words = (uint32_t*)malloc(bin.size * sizeof(uint32_t));
    glslang_program_SPIRV_get(program, bin.words);

    const char* spirv_messages = glslang_program_SPIRV_get_messages(program);
    if (spirv_messages)
        printf("(%s) %s\b", fileName, spirv_messages);

    glslang_program_delete(program);
    glslang_shader_delete(shader);

    return bin;
}
glslang_stage_t convert_vulkan_stage_to_glslang_stage(VkShaderStageFlagBits stage)
{
#define STAGE_CONVERT(_stage)               \
        do {                                \
        case VK_SHADER_STAGE_##_stage##_BIT:\
            return GLSLANG_STAGE_##_stage;  \
        }while(0)

#define STAGE_CONVERT_KHR(_stage)               \
        do {                                \
        case VK_SHADER_STAGE_##_stage##_BIT_KHR:\
            return GLSLANG_STAGE_##_stage;  \
        }while(0)

#define STAGE_CONVERT_NV(_stage)               \
        do {                                \
        case VK_SHADER_STAGE_##_stage##_BIT_NV:\
            return GLSLANG_STAGE_##_stage;  \
        }while(0)
#define STAGE_CONVERT_KHR_2(vk_stage, glslang_stage)               \
        do {                                                       \
        case VK_SHADER_STAGE_##vk_stage##_BIT_KHR:                 \
            return GLSLANG_STAGE_##glslang_stage;                  \
        }while(0)
#define GLSLANG_STAGE_TESSELLATION_CONTROL    GLSLANG_STAGE_TESSCONTROL
#define GLSLANG_STAGE_TESSELLATION_EVALUATION GLSLANG_STAGE_TESSEVALUATION

    switch (stage)
    {
        STAGE_CONVERT(VERTEX);
        STAGE_CONVERT(TESSELLATION_CONTROL);
        STAGE_CONVERT(TESSELLATION_EVALUATION);
        STAGE_CONVERT(GEOMETRY);
        STAGE_CONVERT(FRAGMENT);
        STAGE_CONVERT(COMPUTE);
        STAGE_CONVERT_KHR(RAYGEN);
        STAGE_CONVERT_KHR_2(ANY_HIT, ANYHIT);
        STAGE_CONVERT_KHR_2(CLOSEST_HIT, CLOSESTHIT);
        STAGE_CONVERT_KHR(MISS);
        STAGE_CONVERT_KHR_2(INTERSECTION, INTERSECT);
        STAGE_CONVERT_KHR(CALLABLE);
        STAGE_CONVERT_NV(TASK);
        STAGE_CONVERT_NV(MESH);
    default:
        return GLSLANG_STAGE_COUNT;
    }
}
VkShaderModule get_shader_module(const char* file_name, VkDevice device, VkShaderStageFlagBits stage)
{
    VkShaderModule shader_module = VK_NULL_HANDLE;
    VkShaderModuleCreateInfo module_create_info{};
    uint32_t file_length = 0;
    char* shader_str = nullptr;
    SpirVBinary spirv_binary = { 0 };
    glslang_stage_t glslang_stage = convert_vulkan_stage_to_glslang_stage(stage);
    FILE* fp;
    std::string info_log;
    std::vector<uint32_t> spirv;

    errno_t err = fopen_s(&fp, file_name, "r");
    if (err)
    {
        printf("cannot open file %s\n", file_name);
        return shader_module;
    }

    fseek(fp, 0L, SEEK_END);
    file_length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    shader_str = (char*)calloc(1, file_length + 1);
    if (!shader_str)
        goto exit;
    fread(shader_str, file_length, 1, fp);
    shader_str[file_length] = '\0';

    if (!compile_to_spirv(stage, shader_str, "main", spirv, info_log))
    {
        printf("Failed to compile shader, Error: %s", info_log.c_str());
        return VK_NULL_HANDLE;
    }
    //spirv_binary = compile_glsl_to_spirv(shader_str, file_length, stage, file_name);
    module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    //module_create_info.codeSize = spirv_binary.size;
    //module_create_info.pCode = spirv_binary.words;
    module_create_info.codeSize = spirv.size() * sizeof(uint32_t);
    module_create_info.pCode = spirv.data();
    //module_create_info.codeSize = file_length + 1;
    //module_create_info.pCode = (uint32_t*)shader_str;
    VK_CHECK(vkCreateShaderModule(device, &module_create_info, NULL, &shader_module));
exit:
    if (fp)
    {
        fclose(fp);
    }
    
    if (shader_str)
    {
        free(shader_str);
    }

    return shader_module;
}
VkShaderModule get_shader_module_from_spirv(const char* file_name, VkDevice device, VkShaderStageFlagBits stage)
{
    VkShaderModule shader_module = VK_NULL_HANDLE;
    VkShaderModuleCreateInfo module_create_info{};
    uint32_t file_length = 0;
    char* spirv_str = nullptr;
    uint32_t spirv_length;

    FILE* fp;
    errno_t err = fopen_s(&fp, file_name, "r");
    if (err)
    {
        printf("cannot open file %s\n", file_name);
        return shader_module;
    }

    fseek(fp, 0L, SEEK_END);
    file_length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    spirv_length = (file_length + 4) & (uint32_t)~3;
    spirv_str = (char*)calloc(1, spirv_length);

    if (!spirv_str)
    {
        fclose(fp);
        return shader_module;
    }
    fread(spirv_str, file_length, 1, fp);
    spirv_str[file_length] = '\0';

    module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    module_create_info.codeSize = spirv_length;
    module_create_info.pCode = (uint32_t*)spirv_str;

    VK_CHECK(vkCreateShaderModule(device, &module_create_info, NULL, &shader_module));

    if (fp)
    {
        fclose(fp);
    }

    if (spirv_str)
    {
        free(spirv_str);
    }
    return shader_module;
}
VkPipelineShaderStageCreateInfo load_shader(struct GraphicsContext* graphics_context,const char* shader_filename, VkShaderStageFlagBits stage)
{
	VkPipelineShaderStageCreateInfo shader_stage = {};
	shader_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shader_stage.stage = stage;
	shader_stage.module = get_shader_module(shader_filename, graphics_context->device, stage);
	shader_stage.pName = "main";

	return shader_stage;
}