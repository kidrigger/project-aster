// =============================================
//  Aster: pipeline.h
//  Copyright (c) 2020-2021 Anish Bhobe
// =============================================

#pragma once

#include <stdafx.h>
#include <core/device.h>
#include <core/renderpass.h>

#include <vector>
#include <map>
#include <memory>

// Replacing the vulkan PipelineShaderStageCreateInfo due to keyword module incompatibility;
struct ShaderStage {

	static const bool allowDuplicate = false;
	static VULKAN_HPP_CONST_OR_CONSTEXPR vk::StructureType structureType = vk::StructureType::ePipelineShaderStageCreateInfo;

	ShaderStage& operator=(VkPipelineShaderStageCreateInfo const& rhs) VULKAN_HPP_NOEXCEPT {
		*this = *reinterpret_cast<ShaderStage const*>(&rhs);
		return *this;
	}

	ShaderStage& operator=(ShaderStage const& rhs) VULKAN_HPP_NOEXCEPT {
		memcpy(static_cast<void*>(this), &rhs, sizeof(ShaderStage));
		return *this;
	}

	ShaderStage& setPNext(const void* pNext_) VULKAN_HPP_NOEXCEPT {
		pNext = pNext_;
		return *this;
	}

	ShaderStage& setFlags(vk::PipelineShaderStageCreateFlags flags_) VULKAN_HPP_NOEXCEPT {
		flags = flags_;
		return *this;
	}

	ShaderStage& setStage(vk::ShaderStageFlagBits stage_) VULKAN_HPP_NOEXCEPT {
		stage = stage_;
		return *this;
	}

	ShaderStage& setModule(vk::ShaderModule module_) VULKAN_HPP_NOEXCEPT {
		shaderModule = module_;
		return *this;
	}

	ShaderStage& setPName(const char* pName_) VULKAN_HPP_NOEXCEPT {
		pName = pName_;
		return *this;
	}

	ShaderStage& setPSpecializationInfo(const vk::SpecializationInfo* pSpecializationInfo_) VULKAN_HPP_NOEXCEPT {
		pSpecializationInfo = pSpecializationInfo_;
		return *this;
	}

	operator VkPipelineShaderStageCreateInfo const&() const VULKAN_HPP_NOEXCEPT {
		return *reinterpret_cast<const VkPipelineShaderStageCreateInfo*>(this);
	}

	operator VkPipelineShaderStageCreateInfo&() VULKAN_HPP_NOEXCEPT {
		return *reinterpret_cast<VkPipelineShaderStageCreateInfo*>(this);
	}


#if defined(VULKAN_HPP_HAS_SPACESHIP_OPERATOR)
    auto operator<=>(ShaderStage const&) const = default;
#else
	bool operator==(ShaderStage const& rhs) const VULKAN_HPP_NOEXCEPT {
		return (sType == rhs.sType)
		&& (pNext == rhs.pNext)
		&& (flags == rhs.flags)
		&& (stage == rhs.stage)
		&& (shaderModule == rhs.shaderModule)
		&& (pName == rhs.pName)
		&& (pSpecializationInfo == rhs.pSpecializationInfo);
	}

	bool operator!=(ShaderStage const& rhs) const VULKAN_HPP_NOEXCEPT {
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

class PipelineFactory;

struct InterfaceVariableInfo {
	std::string name;
	u32 location{};
	vk::Format format{};

	b8 operator!=(const InterfaceVariableInfo& _other) const {
		return this->name != _other.name || this->location != _other.location || this->format != _other.format;
	}

	b8 operator==(const InterfaceVariableInfo& _other) const {
		return !(*this != _other);
	}
};

struct DescriptorInfo {
	vk::DescriptorType type{};
	u32 set{ 0 };
	u32 binding{ 0 };
	u32 array_length{ 1 };
	vk::ShaderStageFlags stages = {};
	u32 block_size{};
	std::string name;

	b8 operator!=(const DescriptorInfo& _other) const {
		return set != _other.set || binding != _other.binding || type != _other.type || array_length != _other.array_length || block_size != _other.block_size;
	}
};

namespace std {
	template <>
	struct hash<DescriptorInfo> {
		[[nodiscard]]
		usize operator()(const DescriptorInfo& _val) noexcept;
	};
}

struct ShaderInfo {
	std::string name;
	std::vector<InterfaceVariableInfo> input_vars;
	std::vector<InterfaceVariableInfo> output_vars;
	std::map<std::string, u32> descriptor_names;
	std::vector<DescriptorInfo> descriptors;
	std::vector<vk::PushConstantRange> push_ranges;
};

namespace std {
	template <>
	struct hash<ShaderInfo> {
		[[nodiscard]]
		usize operator()(const ShaderInfo& _val) noexcept;
	};
}

struct Shader {
	ShaderStage stage;
	ShaderInfo info;
	usize program_hash{};
	usize layout_hash{};
};

struct PipelineCreateInfo {

	RenderPass renderpass;

	struct InputAttribute {
		std::string_view attr_name;
		u32 binding{ 0 };
		u32 offset{ 0 };
		vk::Format format{};
	};

	struct {
		std::vector<vk::VertexInputBindingDescription> bindings;
		std::vector<InputAttribute> attributes;
	} vertex_input;

	struct {
		vk::PrimitiveTopology topology{ vk::PrimitiveTopology::eTriangleList };
		b32 primitive_restart_enable{ false };
	} input_assembly;

	struct {
		b32 enable_dynamic{ true };
		std::vector<vk::Viewport> viewports{ 1 };
		std::vector<vk::Rect2D> scissors{ 1 };
	} viewport_state;

	struct {
		b32 raster_discard_enabled{ false };
		vk::PolygonMode polygon_mode{ vk::PolygonMode::eFill };
		vk::CullModeFlags cull_mode{ vk::CullModeFlagBits::eBack };
		vk::FrontFace front_face{ vk::FrontFace::eClockwise };
		b32 depth_clamp_enabled{ false };
		f32 depth_clamp{ 0.0f };

		struct {
			b32 enable{ false };
			f32 constant_factor{ 0.25f };
			f32 slope_factor{ 0.75f };
		} depth_bias;

		f32 line_width{ 1.0f };
	} raster_state;

	struct {
		vk::SampleCountFlagBits sample_count{ vk::SampleCountFlagBits::e1 };
	} multisample_state;

	std::vector<std::string_view> shader_files;

	struct {
		std::vector<vk::PipelineColorBlendAttachmentState> attachments {
			{
				.blendEnable = false,
				.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
			}
		};
		b32 logic_op_enable{ false };
		vk::LogicOp logic_op{ vk::LogicOp::eClear };
		vec4 blend_constants{};
	} color_blend;

	std::vector<vk::DynamicState> dynamic_states;
	std::string name;
};

template <>
struct std::hash<PipelineCreateInfo> {
	[[nodiscard]]
	usize operator()(const PipelineCreateInfo& _value) noexcept;
};

struct Layout {
	usize hash{};
	ShaderInfo layout_info;
	vk::PipelineLayout layout;
	std::vector<vk::DescriptorSetLayout> descriptor_set_layouts;
};

struct Pipeline {
	std::vector<Shader*> shaders;
	Layout* layout{};
	vk::Pipeline pipeline;
	std::string name;
	usize hash{};

	PipelineFactory* parent_factory{};

	void destroy();
};

class PipelineFactory {
public:
	Device* parent_device;

	explicit PipelineFactory(Device* _device) : parent_device{ _device } {}

	PipelineFactory(const PipelineFactory& _other) = delete;
	PipelineFactory(PipelineFactory&& _other) noexcept;
	PipelineFactory& operator=(const PipelineFactory& _other) = delete;
	PipelineFactory& operator=(PipelineFactory&& _other) noexcept;

	~PipelineFactory();

	vk::ResultValue<Pipeline*> create_pipeline(const PipelineCreateInfo& _create_info);

private:
	void destroy_pipeline(Pipeline* _pipeline) noexcept;

	vk::ResultValue<Shader*> create_shader_module(const std::string_view& _name);
	vk::ResultValue<std::vector<Shader*>> create_shaders(const std::vector<std::string_view>& _names);
	void destroy_shader_module(Shader* _shader) noexcept;

	vk::ResultValue<std::vector<vk::DescriptorSetLayout>> create_descriptor_layouts(const ShaderInfo& _shader_info);
	vk::ResultValue<Layout*> create_pipeline_layout(const std::vector<Shader*>& _shaders);
	void destroy_pipeline_layout(Layout* _layout) noexcept;

	// Fields
	std::unordered_map<usize, std::pair<u32, Shader>> shader_map_;
	std::unordered_map<usize, std::pair<u32, Layout>> layout_map_;
	std::unordered_map<usize, std::pair<u32, Pipeline>> pipeline_map_;

	friend Pipeline;
};
