/*=========================================*/
/*  Aster: core/pipeline.h                 */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#pragma once

#include <global.h>
#include <core/device.h>
#include <core/renderpass.h>

#include <vector>
#include <map>
#include <memory>

// Replacing the vulkan PipelineShaderStageCreateInfo due to keyword module incompatibility;
struct ShaderStage {

    static const bool allowDuplicate = false;
    static VULKAN_HPP_CONST_OR_CONSTEXPR vk::StructureType structureType = vk::StructureType::ePipelineShaderStageCreateInfo;

    ShaderStage& operator=(VkPipelineShaderStageCreateInfo const& rhs) VULKAN_HPP_NOEXCEPT
    {
        *this = *reinterpret_cast<ShaderStage const*>(&rhs);
        return *this;
    }

    ShaderStage& operator=(ShaderStage const& rhs) VULKAN_HPP_NOEXCEPT
    {
        memcpy(static_cast<void*>(this), &rhs, sizeof(ShaderStage));
        return *this;
    }

    ShaderStage& setPNext(const void* pNext_) VULKAN_HPP_NOEXCEPT
    {
        pNext = pNext_;
        return *this;
    }

    ShaderStage& setFlags(vk::PipelineShaderStageCreateFlags flags_) VULKAN_HPP_NOEXCEPT
    {
        flags = flags_;
        return *this;
    }

    ShaderStage& setStage(vk::ShaderStageFlagBits stage_) VULKAN_HPP_NOEXCEPT
    {
        stage = stage_;
        return *this;
    }

    ShaderStage& setModule(vk::ShaderModule module_) VULKAN_HPP_NOEXCEPT
    {
        shaderModule = module_;
        return *this;
    }

    ShaderStage& setPName(const char* pName_) VULKAN_HPP_NOEXCEPT
    {
        pName = pName_;
        return *this;
    }

    ShaderStage& setPSpecializationInfo(const vk::SpecializationInfo* pSpecializationInfo_) VULKAN_HPP_NOEXCEPT
    {
        pSpecializationInfo = pSpecializationInfo_;
        return *this;
    }

    operator VkPipelineShaderStageCreateInfo const& () const VULKAN_HPP_NOEXCEPT
    {
        return *reinterpret_cast<const VkPipelineShaderStageCreateInfo*>(this);
    }

    operator VkPipelineShaderStageCreateInfo& () VULKAN_HPP_NOEXCEPT
    {
        return *reinterpret_cast<VkPipelineShaderStageCreateInfo*>(this);
    }


#if defined(VULKAN_HPP_HAS_SPACESHIP_OPERATOR)
    auto operator<=>(ShaderStage const&) const = default;
#else
    bool operator==(ShaderStage const& rhs) const VULKAN_HPP_NOEXCEPT
    {
        return (sType == rhs.sType)
            && (pNext == rhs.pNext)
            && (flags == rhs.flags)
            && (stage == rhs.stage)
            && (shaderModule == rhs.shaderModule)
            && (pName == rhs.pName)
            && (pSpecializationInfo == rhs.pSpecializationInfo);
    }

    bool operator!=(ShaderStage const& rhs) const VULKAN_HPP_NOEXCEPT
    {
        return !operator==(rhs);
    }
#endif

public:
    const vk::StructureType sType = vk::StructureType::ePipelineShaderStageCreateInfo;
    const void* pNext = {};
    vk::PipelineShaderStageCreateFlags flags = {};
    vk::ShaderStageFlagBits stage = vk::ShaderStageFlagBits::eVertex;
    vk::ShaderModule shaderModule = {};
    const char* pName = {};
    const vk::SpecializationInfo* pSpecializationInfo = {};
};

struct PipelineFactory;

struct InterfaceVariableInfo {
    stl::string name;
    u32 location;
    vk::Format format;

    b8 operator!=(const InterfaceVariableInfo& _other) const {
        return this->name != _other.name || this->location != _other.location || this->format != _other.format;
    }

    b8 operator==(const InterfaceVariableInfo& _other) const {
        return !(*this != _other);
    }
};

struct DescriptorInfo {
    vk::DescriptorType type;
    u32 set = 0;
    u32 binding = 0;
    u32 array_length = 1;
    vk::ShaderStageFlags stages = {};
    u32 block_size;
    stl::string name;

    b8 operator!=(const DescriptorInfo& _other) const {
        return set != _other.set || binding != _other.binding || type != _other.type || array_length != _other.array_length || block_size != _other.block_size;
    }
};

namespace std {
    template <>
    struct hash<DescriptorInfo> {
	    usize operator()(const DescriptorInfo& _val) noexcept;
    };
}

struct ShaderInfo {
    stl::string name;
    stl::vector<InterfaceVariableInfo> input_vars;
    stl::vector<InterfaceVariableInfo> output_vars;
    stl::map<stl::string, u32> descriptor_names;
    stl::vector<DescriptorInfo> descriptors;
    stl::vector<vk::PushConstantRange> push_ranges;
};

namespace std {
    template <>
    struct hash<ShaderInfo> {
	    usize operator()(const ShaderInfo& _val) noexcept;
    };
}

struct Shader {
    ShaderStage stage;
    ShaderInfo info;
    usize program_hash;
    usize layout_hash;
};

struct PipelineCreateInfo {

    RenderPass renderpass;

    struct InputAttribute {
        stl::string_view attr_name;
        u32 binding = 0;
        u32 offset = 0;
        vk::Format format;
    };

    struct {
        stl::vector<vk::VertexInputBindingDescription> bindings;
        stl::vector<InputAttribute> attributes;
    } vertex_input;
    struct {
        vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;
        b32 primitive_restart_enable = false;
    } input_assembly;
    struct {
        b32 enable_dynamic = true;
        stl::vector<vk::Viewport> viewports{ 1 };
        stl::vector<vk::Rect2D> scissors{ 1 };
    } viewport_state;
    struct {
        b32 raster_discard_enabled = false;
        vk::PolygonMode polygon_mode = vk::PolygonMode::eFill;
        vk::CullModeFlags cull_mode = vk::CullModeFlagBits::eBack;
        vk::FrontFace front_face = vk::FrontFace::eClockwise;
        b32 depth_clamp_enabled = false;
        f32 depth_clamp = 0.0f;
        struct {
            b32 enable = false;
            f32 constant_factor = 0.25f;
            f32 slope_factor = 0.75f;
        } depth_bias;
        f32 line_width = 1.0f;
    } raster_state;
    struct {
        vk::SampleCountFlagBits sample_count = vk::SampleCountFlagBits::e1;
    } multisample_state;
    stl::vector<stl::string_view> shader_files;

    struct {
        stl::vector<vk::PipelineColorBlendAttachmentState> attachments = { {
            .blendEnable = false,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
        } };
        b32 logic_op_enable = false;
        vk::LogicOp logic_op = vk::LogicOp::eClear;
        vec4 blend_constants = {};
    } color_blend;
    stl::vector<vk::DynamicState> dynamic_states;
    stl::string name;
};

template <>
struct std::hash<PipelineCreateInfo> {
    usize operator()(const PipelineCreateInfo& _value) noexcept;
};

struct Layout {
    usize hash;
    ShaderInfo layout_info;
    vk::PipelineLayout layout;
    stl::vector<vk::DescriptorSetLayout> descriptor_set_layouts;
};

struct Pipeline {
    stl::vector<Shader*> shaders;
    Layout* layout;
    vk::Pipeline pipeline;
    stl::string name;
    usize hash;

    PipelineFactory* parent_factory;

    void destroy();
};

struct PipelineFactory {

    Device* parent_device;

    void init(Device* _device) {
        parent_device = _device;
    }

    void destroy() {
        for (auto& [k, v] : pipeline_map) {
            destroy_pipeline(&v.second);
        }
        for (auto& [k, v] : layout_map) {
            WARN(stl::fmt("Pipeline layout %s not released by pipeline!", v.second.layout_info.name.c_str()));
            destroy_pipeline_layout(&v.second);
        }
        for (auto& [k, v] : shader_map) {
            WARN(stl::fmt("Shader Module %s not released by pipeline!", v.second.info.name.c_str()));
            destroy_shader_module(&v.second);
        }
    }

    vk::ResultValue<Shader*> create_shader_module(const stl::string_view& _name);
    vk::ResultValue<stl::vector<Shader*>> create_shaders(const stl::vector<stl::string_view>& _names);
    void destroy_shader_module(Shader* _shader);

    vk::ResultValue<stl::vector<vk::DescriptorSetLayout>> create_descriptor_layouts(const ShaderInfo& _shader_info);
    vk::ResultValue<Layout*> create_pipeline_layout(const stl::vector<Shader*>& _shaders);
    void destroy_pipeline_layout(Layout* _layout);

    vk::ResultValue<Pipeline*> create_pipeline(const PipelineCreateInfo& _create_info);
    void destroy_pipeline(Pipeline* _pipeline);

    // Fields
    stl::unordered_map<usize, stl::pair<u32, Shader>> shader_map;
    stl::unordered_map<usize, stl::pair<u32, Layout>> layout_map;
    stl::unordered_map<usize, stl::pair<u32, Pipeline>> pipeline_map;
};