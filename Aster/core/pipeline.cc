/*=========================================*/
/*  Aster: core/pipeline.cc                */
/*  Copyright (c) 2020 Anish Bhobe         */
/*=========================================*/
#pragma once

#include "pipeline.h"
#include <util/files.h>
#include <spirv_reflect/spirv_reflect.h>
#include <map>

constexpr inline const char* to_cstring(SpvReflectResult _result) {
	switch (_result)
	{
	case SPV_REFLECT_RESULT_SUCCESS: return "Success";
	case SPV_REFLECT_RESULT_NOT_READY: return "NotReady";
	case SPV_REFLECT_RESULT_ERROR_PARSE_FAILED: return "ErrorParseFailed";
	case SPV_REFLECT_RESULT_ERROR_ALLOC_FAILED: return "ErrorAllocFailed";
	case SPV_REFLECT_RESULT_ERROR_RANGE_EXCEEDED: return "ErrorRangeExceeded";
	case SPV_REFLECT_RESULT_ERROR_NULL_POINTER: return "ErrorNullPointer";
	case SPV_REFLECT_RESULT_ERROR_INTERNAL_ERROR: return "ErrorInternalError";
	case SPV_REFLECT_RESULT_ERROR_COUNT_MISMATCH: return "ErrorCountMismatch";
	case SPV_REFLECT_RESULT_ERROR_ELEMENT_NOT_FOUND: return "ErrorElementNotFound";
	case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_CODE_SIZE: return "ErrorSpirvInvalidCodeSize";
	case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_MAGIC_NUMBER: return "ErrorSpirvInvalidMagicNumber";
	case SPV_REFLECT_RESULT_ERROR_SPIRV_UNEXPECTED_EOF: return "ErrorSpirvUnexpectedEof";
	case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE: return "ErrorSpirvInvalidIdReference";
	case SPV_REFLECT_RESULT_ERROR_SPIRV_SET_NUMBER_OVERFLOW: return "ErrorSpirvSetNumberOverflow";
	case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_STORAGE_CLASS: return "ErrorSpirvInvalidStorageClass";
	case SPV_REFLECT_RESULT_ERROR_SPIRV_RECURSION: return "ErrorSpirvRecursion";
	case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_INSTRUCTION: return "ErrorSpirvInvalidInstruction";
	case SPV_REFLECT_RESULT_ERROR_SPIRV_UNEXPECTED_BLOCK_DATA: return "ErrorSpirvUnexpectedBlockData";
	case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_BLOCK_MEMBER_REFERENCE: return "ErrorSpirvInvalidBlockMemberReference";
	case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ENTRY_POINT: return "ErrorSpirvInvalidEntryPoint";
	case SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_EXECUTION_MODE: return "ErrorSpirvInvalidExecutionMode";
	default: return nullptr;
	}
}

inline b8 spv_failed(SpvReflectResult _result) {
	return _result != SPV_REFLECT_RESULT_SUCCESS;
}

vk::ResultValue<Shader*> PipelineFactory::create_shader_module(const stl::string_view& _name) {

	usize hash_key = hash_any(_name);
	if (shader_map.contains(hash_key)) {
		auto& entry = shader_map[hash_key];
		entry.first++;
		DEBUG(stl::fmt("Using cached shader %s", _name.data()));
		return vk::ResultValue<Shader*>(vk::Result::eSuccess, &entry.second);
	}
	DEBUG(stl::fmt("Creating new shader %s", _name.data()));

	b8 found = file_exists(_name);
	ERROR_IF(!found, stl::fmt("Shader '%s' not found.", _name.data()));
	if (!found) {
		return vk::ResultValue<Shader*>(vk::Result::eIncomplete, nullptr);
	}

	auto code = load_binary32_file(_name);
	b8 empty = code.empty();
	ERROR_IF(empty, stl::fmt("Shader '%s' is empty.", _name.data()));
	if (empty) {
		return vk::ResultValue<Shader*>(vk::Result::eIncomplete, nullptr);
	}

	auto spv_ext_idx = _name.find_last_of('.');
	auto spv_ext = _name.substr(spv_ext_idx);
	WARN_IF(spv_ext != ".spv"sv, stl::fmt("Shader '%s' has extension '%s' instead of '.spv'", _name.data(), spv_ext.data()));
	auto shader_ext_idx = _name.substr(0, spv_ext_idx).find_last_of('.');
	auto shader_ext = _name.substr(shader_ext_idx, spv_ext_idx - shader_ext_idx);

	spv_reflect::ShaderModule reflector(code);
	ERROR_IF(spv_failed(reflector.GetResult()), stl::fmt("Spirv reflection failed with %s", to_cstring(reflector.GetResult())));

	u32 set_count;
	reflector.EnumerateDescriptorSets(&set_count, nullptr);
	ERROR_IF(spv_failed(reflector.GetResult()), stl::fmt("Spirv reflection failed with %s", to_cstring(reflector.GetResult())));

	stl::map<stl::string, u32> descriptor_names;
	stl::vector<DescriptorInfo> descriptors;
	if (set_count > 0) {
		stl::vector<SpvReflectDescriptorSet*> sets(set_count, nullptr);
		reflector.EnumerateDescriptorSets(&set_count, sets.data());
		ERROR_IF(spv_failed(reflector.GetResult()), stl::fmt("Spirv reflection failed with %s", to_cstring(reflector.GetResult())));

		stl::map<stl::pair<u32, u32>, DescriptorInfo> uniforms;
		for (auto set_ : sets) {
			for (u32 binding_idx = 0; binding_idx < set_->binding_count; ++binding_idx) {
				SpvReflectDescriptorBinding* binding_ = set_->bindings[binding_idx];

				u32 length = 1;
				for (u32 i_dim = 0; i_dim < binding_->array.dims_count; ++i_dim) {
					length *= binding_->array.dims[i_dim];
				}

				auto key_ = stl::make_pair( set_->set, binding_->binding );
				if (uniforms.contains(key_)) {
					b8 is_combined_sampler = false;
					is_combined_sampler |= (uniforms[key_].type == vk::DescriptorType::eSampledImage && cast<vk::DescriptorType>(binding_->descriptor_type) == vk::DescriptorType::eSampler);
					is_combined_sampler |= (uniforms[key_].type == vk::DescriptorType::eSampler && cast<vk::DescriptorType>(binding_->descriptor_type) == vk::DescriptorType::eSampledImage);
					WARN_IF(!is_combined_sampler, stl::fmt("Two bindings at (%u, %u) that are not a combined image sampler", set_->set, binding_->binding));

					uniforms[key_].type = cast<vk::DescriptorType>(vk::DescriptorType::eCombinedImageSampler);
				} else {
					uniforms[{ set_->set, binding_->binding }] = {
						.type = cast<vk::DescriptorType>(binding_->descriptor_type),
						.set = set_->set,
						.binding = binding_->binding,
						.array_length = length,
						.stages = cast<vk::ShaderStageFlags>(reflector.GetShaderStage()),
						.block_size = binding_->block.size,
						.name = binding_->name,
					};
				}
			}
		}

		u32 idx = 0;
		descriptors.reserve(uniforms.size());
		for (auto& [k, u] : uniforms) {
			VERBOSE(stl::fmt("%d, %d\ntype=%s\narray_length=%u\nstages=%s\nblock_size=%u\nname=%s", u.set, u.binding, to_string(u.type).data(), u.array_length, to_string(u.stages).data(), u.block_size, u.name.data()));
			descriptors.push_back(u);
			descriptor_names[u.name] = idx++;
		}
	}

	vk::ShaderStageFlagBits shader_stage = cast<vk::ShaderStageFlagBits>(reflector.GetShaderStage());

	stl::vector<InterfaceVariableInfo> input_variables;
	u32 input_variable_count;
	reflector.EnumerateInputVariables(&input_variable_count, nullptr);
	ERROR_IF(spv_failed(reflector.GetResult()), stl::fmt("Spirv reflection failed with %s", to_cstring(reflector.GetResult())));

	if (input_variable_count) {
		stl::vector<SpvReflectInterfaceVariable*> input_vars(input_variable_count);
		reflector.EnumerateInputVariables(&input_variable_count, input_vars.data());
		ERROR_IF(spv_failed(reflector.GetResult()), stl::fmt("Spirv reflection failed with %s", to_cstring(reflector.GetResult())));

		for (auto& iv : input_vars) {
			if (!iv->name) continue;
			VERBOSE(stl::fmt("IN %s %d %s", iv->name, iv->location, to_string(cast<vk::Format>(iv->format)).c_str()));
			auto name = stl::string_view(iv->name);
			input_variables.emplace_back() = {
				.name = stl::string(name.substr(name.find_last_of('.'))),
				.location = iv->location,
				.format = cast<vk::Format>(iv->format),
			};
		}
	}
	stl::sort(input_variables.begin(), input_variables.end(), [](InterfaceVariableInfo& a, InterfaceVariableInfo& b) { return a.name > b.name; });

	stl::vector<InterfaceVariableInfo> output_variables;
	u32 output_variable_count;
	reflector.EnumerateOutputVariables(&output_variable_count, nullptr);
	ERROR_IF(spv_failed(reflector.GetResult()), stl::fmt("Spirv reflection failed with %s", to_cstring(reflector.GetResult())));

	if (output_variable_count) {
		stl::vector<SpvReflectInterfaceVariable*> output_vars(output_variable_count);
		reflector.EnumerateOutputVariables(&output_variable_count, output_vars.data());
		ERROR_IF(spv_failed(reflector.GetResult()), stl::fmt("Spirv reflection failed with %s", to_cstring(reflector.GetResult())));

		for (auto& iv : output_vars) {
			if (!iv->name) continue;
			VERBOSE(stl::fmt("OUT %s %d %s", iv->name, iv->location, to_string(cast<vk::Format>(iv->format)).c_str()));
			auto name = stl::string_view(iv->name);
			output_variables.emplace_back() = {
				.name = stl::string(name.substr(name.find_last_of('.'))),
				.location = iv->location,
				.format = cast<vk::Format>(iv->format),
			};
		}
	}
	stl::sort(output_variables.begin(), output_variables.end(), [](InterfaceVariableInfo& a, InterfaceVariableInfo& b) { return a.name > b.name; });

	stl::vector<vk::PushConstantRange> push_constant_ranges;
	u32 push_constant_count = 0;
	reflector.EnumeratePushConstantBlocks(&push_constant_count, nullptr);

	// Push const is not perfect
	if (push_constant_count) {
		stl::vector<SpvReflectBlockVariable*> push_constants(push_constant_count);
		reflector.EnumeratePushConstantBlocks(&push_constant_count, push_constants.data());
		ERROR_IF(spv_failed(reflector.GetResult()), stl::fmt("Spirv reflection failed with %s", to_cstring(reflector.GetResult())));
		
		for (auto pc_ : push_constants) {
			push_constant_ranges.emplace_back() = {
				.stageFlags = shader_stage,
				.offset = pc_->absolute_offset,
				.size = pc_->padded_size,
			};
		}
	}

	// Module creation
	auto [result, shader] = parent_device->device.createShaderModule({
		.codeSize = sizeof(u32) * code.size(),
		.pCode = code.data(),
	});
	if (!failed(result)) {
		parent_device->set_object_name(shader, _name);
	}

	ShaderInfo shader_info = {
		.name = stl::string(_name),
		.input_vars = move(input_variables),
		.output_vars = move(output_variables),
		.descriptor_names = move(descriptor_names),
		.descriptors = move(descriptors),
		.push_ranges = move(push_constant_ranges),
	};

	auto& val = shader_map[hash_key] = {
		1u,
		{
			.stage = {
				.stage = shader_stage,
				.shaderModule = shader,
				.pName = "main",
			},
			.info = shader_info,
			.program_hash = hash_key,
			.layout_hash = hash_any(shader_info),
		}
	};

	return vk::ResultValue<Shader*>(result, &val.second);
}

void PipelineFactory::destroy_shader_module(Shader* _shader) {
	usize hash_key = hash_any(_shader->info.name);
	ERROR_IF(!shader_map.contains(hash_key), stl::fmt("Destroy called on unexisting shader %s", _shader->info.name.c_str()));

	if (0 == --shader_map[hash_key].first) {
		DEBUG(stl::fmt("Deleting cached shader %s", _shader->info.name.data()));
		parent_device->device.destroyShaderModule(_shader->stage.shaderModule);
		shader_map.erase(hash_key);
	}
}

vk::ResultValue<stl::vector<Shader*>> PipelineFactory::create_shaders(const stl::vector<stl::string_view>& _names) {
	stl::vector<Shader*> shaders;
	shaders.reserve(_names.size());

	for (auto& name : _names) {
		vk::Result res;
		tie(res, shaders.emplace_back()) = create_shader_module(name);
		ERROR_IF(failed(res), stl::fmt("Shader %s creation failed with %s", name.data(), to_cstring(res)));
		if (failed(res)) {
			for (auto& shader_ : shaders) {
				parent_device->device.destroyShaderModule(shader_->stage.shaderModule);
			}
			return { res, {} };
		}
	}

	/*
	 * TODO:
	 * Change supported to support the rest
	 */

	// Check if given shader combos are supported
	constexpr auto supported = [](const vk::ShaderStageFlags _flags) {
		if (_flags & (vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment)) {
			return true;
		}
		return false;
	};

	Shader* vertex_shader = nullptr;
	Shader* fragment_shader = nullptr;

	vk::ShaderStageFlags used_stages;
	for (auto& stage_ : shaders) {
		if (stage_->stage.stage == vk::ShaderStageFlagBits::eVertex) vertex_shader = stage_;
		else if (stage_->stage.stage == vk::ShaderStageFlagBits::eFragment) fragment_shader = stage_;
		used_stages |= stage_->stage.stage;
		WARN_IF(!supported(stage_->stage.stage), stl::fmt("%s Shader unsupported", to_string(stage_->stage.stage).c_str()));
	}

	ERROR_IF((used_stages & vk::ShaderStageFlagBits::eAllGraphics) && (used_stages & vk::ShaderStageFlagBits::eCompute), "Compute and Graphics stages can't be used in same pipeline");

	if (vertex_shader != nullptr && fragment_shader != nullptr) {

		WARN_IF(vertex_shader->info.output_vars.size() != fragment_shader->info.input_vars.size(), stl::fmt("%s outputs don't map to %s Inputs 1:1 (%lu vs %lu)", vertex_shader->info.name.c_str(), fragment_shader->info.name.c_str()));
		auto vertex_out = vertex_shader->info.output_vars.begin();
		const auto vertex_out_end = vertex_shader->info.output_vars.end();
		auto fragment_input = fragment_shader->info.input_vars.begin();

		for (; vertex_out != vertex_out_end; ++vertex_out, ++fragment_input) {
			ERROR_IF(*fragment_input != *vertex_out, stl::fmt("%s output does not match %s inputs", vertex_shader->info.name.c_str(), fragment_shader->info.name.c_str()));
		}
	}
	else if (vertex_shader != nullptr || fragment_shader != nullptr) {
		// TODO: Add compute shader logic
		ERROR("Vertex shader and Fragment shader must both exist");
	}

	return { vk::Result::eSuccess, shaders };
}

vk::ResultValue<RenderPass> PipelineFactory::create_renderpass(const stl::string& _name, const vk::RenderPassCreateInfo& _create_info) {
	ERROR_IF(_create_info.subpassCount != 1, stl::fmt("Renderpass %s has more than 1 subpass. Currently unsupported"));

	const auto attachments = stl::span(_create_info.pAttachments, cast<usize>(_create_info.attachmentCount));
	const auto color_refs = stl::span(_create_info.pSubpasses->pColorAttachments, cast<usize>(_create_info.pSubpasses->colorAttachmentCount));
	const auto preserve_refs = stl::span(_create_info.pSubpasses->pPreserveAttachments, cast<usize>(_create_info.pSubpasses->preserveAttachmentCount));

	usize hash_value = 0;
	
	for (const auto& attachment_ : attachments) {
		hash_value = hash_combine(hash_value, hash_any(attachment_.format));
		hash_value = hash_combine(hash_value, hash_any(attachment_.samples));
	}

	usize color_ref_map = 0;
	for (const auto& ref : color_refs) {
		color_ref_map |= (cast<usize>(1) << ref.attachment);
	}
	hash_value = hash_combine(hash_value, hash_any(color_ref_map));

	usize preserve_ref_map = 0;
	for (const auto& ref : preserve_refs) {
		preserve_ref_map |= (cast<usize>(1) << ref);
	}
	hash_value = hash_combine(hash_value, hash_any(preserve_ref_map));

	if (_create_info.pSubpasses->pDepthStencilAttachment != nullptr) {
		hash_value = hash_combine(hash_value, hash_any(_create_info.pSubpasses->pDepthStencilAttachment->attachment));
	}

	auto [result, rp] = parent_device->device.createRenderPass(_create_info);
	if (!failed(result)) {
		auto renderpass = RenderPass{
			.renderpass = rp,
			.name = _name,
			.attachment_format = hash_value,
			.parent_factory = this,
		};
		parent_device->set_object_name(renderpass.renderpass, _name);
		return { result, renderpass };
	}
	return { result, {} };
}

void PipelineFactory::destroy_pipeline_layout(Layout* _layout) {
	const auto hash_key = _layout->hash;
	ERROR_IF(!layout_map.contains(hash_key), stl::fmt("Destroy called on unexisting layout %s", _layout->layout_info.name.c_str()));

	if (0 == --layout_map[hash_key].first) {
		DEBUG(stl::fmt("Deleting cached layout %s", _layout->layout_info.name.data()));
		parent_device->device.destroyPipelineLayout(_layout->layout);
		for (auto& dsl_ : _layout->descriptor_set_layouts) {
			parent_device->device.destroyDescriptorSetLayout(dsl_);
		}
		layout_map.erase(hash_key);
	}
}

void PipelineFactory::destroy_pipeline(Pipeline* _pipeline) {
	const auto hash_key = _pipeline->hash;
	ERROR_IF(!pipeline_map.contains(hash_key), stl::fmt("Destroy called on unexisting pipeline %s", _pipeline->name.c_str()));

	if (0 >= --pipeline_map[hash_key].first) {
		DEBUG(stl::fmt("Deleting cached pipeline %s", _pipeline->name.data()));
		for (auto& shader_ : _pipeline->shaders) {
			this->destroy_shader_module(shader_);
		}
		this->destroy_pipeline_layout(_pipeline->layout);
		parent_device->device.destroyPipeline(_pipeline->pipeline);
		layout_map.erase(hash_key);
	}
}

vk::ResultValue<stl::vector<vk::DescriptorSetLayout>> PipelineFactory::create_descriptor_layouts(const ShaderInfo& _shader_info) {
	auto result = vk::Result::eSuccess;
	stl::vector<vk::DescriptorSetLayout> descriptor_set_layout;

	if (_shader_info.descriptors.empty()) {
		return vk::ResultValue(result, descriptor_set_layout);
	}

	stl::vector<vk::DescriptorSetLayoutBinding> bindings;
	u32 current_set = _shader_info.descriptors.front().set;

	for (auto& dsi_ : _shader_info.descriptors) {
		bindings.push_back({
			.binding = dsi_.binding,
			.descriptorType = dsi_.type,
			.descriptorCount = dsi_.array_length,
			.stageFlags = dsi_.stages,
		});

		if (current_set != dsi_.set) {
			tie(result, descriptor_set_layout.emplace_back()) = parent_device->device.createDescriptorSetLayout({
				.bindingCount = cast<u32>(bindings.size()),
				.pBindings = bindings.data(),
			});
			bindings.clear();
			ERROR_IF(failed(result), stl::fmt("Set %d creation failed with %s", current_set, to_cstring(result)));
			if (failed(result)) {
				for (auto& dsl_ : descriptor_set_layout) {
					parent_device->device.destroyDescriptorSetLayout(dsl_);
				}
				descriptor_set_layout.clear();
				return vk::ResultValue(result, descriptor_set_layout);
			}
		}
		current_set = dsi_.set;
	}
	if (!bindings.empty()) {
		tie(result, descriptor_set_layout.emplace_back()) = parent_device->device.createDescriptorSetLayout({
			.bindingCount = cast<u32>(bindings.size()),
			.pBindings = bindings.data(),
		});
		bindings.clear();
		ERROR_IF(failed(result), stl::fmt("Set %d creation failed with %s", current_set, to_cstring(result)));
		if (failed(result)) {
			for (auto& dsl_ : descriptor_set_layout) {
				parent_device->device.destroyDescriptorSetLayout(dsl_);
			}
			descriptor_set_layout.clear();
			return vk::ResultValue(result, descriptor_set_layout);
		}
	}

	return vk::ResultValue(result, descriptor_set_layout);
}

void merge_acc_descriptor(DescriptorInfo* _acc, DescriptorInfo& _info) {
	stl::map<u32, DescriptorInfo> binding_map;
	ERROR_IF(_info.set != _acc->set, "Descriptor Set mismatch");
	ERROR_IF(_acc->binding != _info.binding, stl::fmt("Bindings %s and %s don't match.", _acc->name != _info.name));
	_acc->name = move(_info.name);
	_acc->stages |= _info.stages;
}

vk::ResultValue<Layout*> PipelineFactory::create_pipeline_layout(const stl::vector<Shader*>& _shaders) {

	auto result = vk::Result::eSuccess;
	vk::PipelineLayout layout;

	usize layout_key = 0;
	for (const auto& shader_ : _shaders) {
		layout_key = hash_combine(layout_key, shader_->layout_hash);
	}
	if (layout_map.contains(layout_key)) {
		auto& [ref_count_, layout_] = layout_map[layout_key];
		++ref_count_;
		return vk::ResultValue(result, &layout_);
	}

	Shader* vertex_shader = nullptr;
	Shader* fragment_shader = nullptr;
	for (auto& shader_ : _shaders) {
		if (shader_->stage.stage == vk::ShaderStageFlagBits::eVertex) {
			vertex_shader = shader_;
		}
		if (shader_->stage.stage == vk::ShaderStageFlagBits::eFragment) {
			fragment_shader = shader_;
		}
	}

	constexpr auto descriptor_info_lt = [](const DescriptorInfo& _a, const DescriptorInfo& _b) -> b8 { return _a.set != _b.set ? _a.set < _b.set : _a.binding < _b.binding; };

	stl::vector<DescriptorInfo> descriptors;
	for (auto& shader_ : _shaders) {
		for (auto& descriptor_ : shader_->info.descriptors) {
			auto find_d = stl::lower_bound(descriptors.begin(), descriptors.end(), descriptor_, descriptor_info_lt);
			if (find_d != descriptors.end()) {
				merge_acc_descriptor(&*find_d, descriptor_);
			}
			else {
				descriptors.push_back(descriptor_);
			}
		}
	}
	stl::sort(descriptors.begin(), descriptors.end(), descriptor_info_lt);

	stl::map<stl::string, u32> descriptor_names;
	u32 i_ = 0;
	for (auto& di_ : descriptors) {
		descriptor_names[di_.name] = i_++;
	}

	stl::vector<InterfaceVariableInfo> input_vars;
	stl::vector<InterfaceVariableInfo> output_vars;
	if (vertex_shader != nullptr && fragment_shader != nullptr) {
		input_vars = vertex_shader->info.input_vars;
		output_vars = fragment_shader->info.output_vars;
	}

	stl::vector<vk::PushConstantRange> push_ranges;
	u32 offset = max_value<u32>;
	u32 end_offset = min_value<u32>;
	vk::ShaderStageFlags stage = {};
	for (auto& shader_ : _shaders) {
		for (auto& pcr_ : shader_->info.push_ranges) {
			offset = stl::min(pcr_.offset, offset);
			end_offset = stl::max(pcr_.offset + pcr_.size, end_offset);
			stage |= pcr_.stageFlags;
		}
	}
	if (end_offset > offset) {
		push_ranges.emplace_back() = {
			.stageFlags = stage,
			.offset = offset,
			.size = end_offset - offset,
		};
	}

	ShaderInfo pipeline_info = {
		.name = "pipeline_info",
		.input_vars = move(input_vars),
		.output_vars = move(output_vars),
		.descriptor_names = move(descriptor_names),
		.descriptors = move(descriptors),
		.push_ranges = move(push_ranges),
	};

	stl::vector<vk::DescriptorSetLayout> descriptor_layouts;
	tie(result, descriptor_layouts) = create_descriptor_layouts(pipeline_info);
	ERROR_IF(failed(result), stl::fmt("Descriptor layouts creation for %s failed with %s", pipeline_info.name.c_str(), to_cstring(result))) THEN_CRASH(result);

	tie(result, layout) = parent_device->device.createPipelineLayout({
		.setLayoutCount = cast<u32>(descriptor_layouts.size()),
		.pSetLayouts = descriptor_layouts.data(),
		.pushConstantRangeCount = cast<u32>(pipeline_info.push_ranges.size()),
		.pPushConstantRanges = pipeline_info.push_ranges.data(),
	});
	ERROR_IF(failed(result), stl::fmt("Pipeline layout creation for %s failed with %s", pipeline_info.name.c_str(), to_cstring(result))) THEN_CRASH(result);
	parent_device->set_object_name(layout, pipeline_info.name);

	auto& val = layout_map[layout_key] = {
		1u,
		{
			.hash = layout_key,
			.layout_info = pipeline_info,
			.layout = layout,
			.descriptor_set_layouts = move(descriptor_layouts),
		}
	};

	return vk::ResultValue<Layout*>(result, &val.second);
}

vk::ResultValue<Pipeline*> PipelineFactory::create_pipeline(const PipelineCreateInfo& _create_info) {

	vk::Pipeline pipeline;

	usize pipeline_key = hash_any(_create_info);
	if (pipeline_map.contains(pipeline_key)) {
		auto& entry_ = pipeline_map[pipeline_key];
		++entry_.first;
		return vk::ResultValue(vk::Result::eSuccess, &entry_.second);
	}

	auto [result, shaders] = create_shaders(_create_info.shader_files);
	ERROR_IF(failed(result), "Shader creation failed");

	Layout* pipeline_layout;
	tie(result, pipeline_layout) = create_pipeline_layout(shaders);
	ERROR_IF(failed(result), stl::fmt("Pipeline layout creation for %s failed with %s", _create_info.name.c_str(), to_cstring(result)));

	stl::vector<ShaderStage> shader_stages(shaders.size());
	stl::transform(shaders.begin(), shaders.end(), shader_stages.begin(), [](Shader* s) { return s->stage; });
	
	stl::vector<vk::VertexInputAttributeDescription> input_attributes(pipeline_layout->layout_info.input_vars.size());
	for (auto& ivs : pipeline_layout->layout_info.input_vars) {
		auto& in_attr = _create_info.vertex_input.attributes;
		auto match_iv = stl::find_if(in_attr.begin(), in_attr.end(), [&_name = ivs.name](auto& _ia) { return _ia.attr_name == _name; });
		ERROR_IF(match_iv == in_attr.end(), stl::fmt("Attribute %s required by shader, not found", ivs.name.c_str()))
			ELSE_IF_ERROR(match_iv->format != ivs.format, stl::fmt("Attribute %s has mismatching formats (exp: %s, found: %s)", ivs.name.c_str(), to_cstring(ivs.format), to_cstring(match_iv->format)));

		input_attributes[ivs.location] = {
			.location = ivs.location,
			.binding = match_iv->binding,
			.format = ivs.format,
			.offset = 0,
		};
	}

	vk::PipelineVertexInputStateCreateInfo visci = {
		.vertexBindingDescriptionCount = cast<u32>(_create_info.vertex_input.bindings.size()),
		.pVertexBindingDescriptions = _create_info.vertex_input.bindings.data(),
		.vertexAttributeDescriptionCount = cast<u32>(input_attributes.size()),
		.pVertexAttributeDescriptions = input_attributes.data(),
	};

	vk::PipelineInputAssemblyStateCreateInfo iasci = {
		.topology = _create_info.input_assembly.topology,
		.primitiveRestartEnable = _create_info.input_assembly.primitive_restart_enable,
	};

	b32 enable_dynamic = _create_info.viewport_state.enable_dynamic;
	vk::PipelineViewportStateCreateInfo vsci = {
		.viewportCount = cast<u32>(_create_info.viewport_state.viewports.size()),
		.pViewports = enable_dynamic ? nullptr : _create_info.viewport_state.viewports.data(),
		.scissorCount = cast<u32>(_create_info.viewport_state.scissors.size()),
		.pScissors = enable_dynamic ? nullptr : _create_info.viewport_state.scissors.data(),
	};

	vk::PipelineRasterizationStateCreateInfo rsci = {
		.depthClampEnable = _create_info.raster_state.depth_clamp_enabled,
		.rasterizerDiscardEnable = _create_info.raster_state.raster_discard_enabled,
		.polygonMode = _create_info.raster_state.polygon_mode,
		.cullMode = _create_info.raster_state.cull_mode,
		.frontFace = _create_info.raster_state.front_face,
		.depthBiasEnable = _create_info.raster_state.depth_bias.enable,
		.depthBiasConstantFactor = _create_info.raster_state.depth_bias.constant_factor,
		.depthBiasClamp = _create_info.raster_state.depth_clamp,
		.depthBiasSlopeFactor = _create_info.raster_state.depth_bias.slope_factor,
		.lineWidth = _create_info.raster_state.line_width,
	};

	vk::PipelineMultisampleStateCreateInfo msci = {
		.rasterizationSamples = _create_info.multisample_state.sample_count,
		.sampleShadingEnable = false, // TODO
	};

	auto& ssci = shader_stages;

	vk::PipelineColorBlendStateCreateInfo cbsci = {
		.logicOpEnable = _create_info.color_blend.logic_op_enable,
		.logicOp = _create_info.color_blend.logic_op,
		.attachmentCount = cast<u32>(_create_info.color_blend.attachments.size()),
		.pAttachments = _create_info.color_blend.attachments.data(),
		.blendConstants = stl::array{ 
			_create_info.color_blend.blend_constants.x,
			_create_info.color_blend.blend_constants.y,
			_create_info.color_blend.blend_constants.z,
			_create_info.color_blend.blend_constants.w, 
		},
	};

	vk::PipelineDynamicStateCreateInfo dsci = {
		.dynamicStateCount = cast<u32>(_create_info.dynamic_states.size()),
		.pDynamicStates = _create_info.dynamic_states.data(),
	};

	tie(result, pipeline) = parent_device->device.createGraphicsPipeline({/*Cache*/ }, {
		.stageCount = cast<u32>(ssci.size()),
		.pStages = recast<const vk::PipelineShaderStageCreateInfo*>(ssci.data()),
		.pVertexInputState = &visci,
		.pInputAssemblyState = &iasci,
		.pViewportState = &vsci,
		.pRasterizationState = &rsci,
		.pMultisampleState = &msci,
		.pColorBlendState = &cbsci,
		.pDynamicState = &dsci,
		.layout = pipeline_layout->layout,
		.renderPass = _create_info.renderpass.renderpass,
	});
	ERROR_IF(failed(result), stl::fmt("Pipeline %s creation failed with %s", _create_info.name.c_str(), to_cstring(result)));
	parent_device->set_object_name(pipeline, _create_info.name);

	auto& val = pipeline_map[pipeline_key] = {
		1u,
		Pipeline{
			.shaders = move(shaders),
			.layout = pipeline_layout,
			.pipeline = pipeline,
			.name = move(_create_info.name),
			.hash = pipeline_key,
			.parent_factory = this,
		}
	};

	return vk::ResultValue(result, &val.second);
};

usize std::hash<ShaderInfo>::operator()(const ShaderInfo& _val) noexcept {
	usize hash_ = 0;
	for (auto& ds_ : _val.descriptors) {
		hash_ = hash_combine(hash_, hash_any(ds_));
	}
	for (auto& pcr_ : _val.push_ranges) {
		hash_ = hash_combine(hash_, hash_any(pcr_.size));
		hash_ = hash_combine(hash_, hash_any(pcr_.offset));
		hash_ = hash_combine(hash_, hash_any(pcr_.stageFlags));
	}
	return hash_;
}

usize std::hash<DescriptorInfo>::operator()(const DescriptorInfo& _val) noexcept {
	usize hash_ = hash_any(_val.type);
	hash_ = hash_combine(hash_, hash_any(_val.set));
	hash_ = hash_combine(hash_, hash_any(_val.binding));
	hash_ = hash_combine(hash_, hash_any(_val.array_length));
	hash_ = hash_combine(hash_, hash_any(_val.stages));
	hash_ = hash_combine(hash_, hash_any(_val.block_size));
	return hash_;
}

void RenderPass::destroy() {
	parent_factory->parent_device->device.destroyRenderPass(renderpass);
}

usize std::hash<PipelineCreateInfo>::operator()(const PipelineCreateInfo& _value) noexcept {
	auto hash_val = hash_any(_value.renderpass.attachment_format);
	{ // vertex input
		for (const auto& binding_ : _value.vertex_input.bindings) {
			hash_val = hash_combine(hash_val, hash_any(binding_.binding));
			hash_val = hash_combine(hash_val, hash_any(binding_.inputRate));
		}
		for (const auto& attr_ : _value.vertex_input.attributes) {
			hash_val = hash_combine(hash_val, hash_any(attr_.binding));
			hash_val = hash_combine(hash_val, hash_any(attr_.attr_name));
			hash_val = hash_combine(hash_val, hash_any(attr_.format));
			hash_val = hash_combine(hash_val, hash_any(attr_.offset));
		}
	}
	{ // input assembly
		hash_val = hash_combine(hash_val, hash_any(_value.input_assembly.topology));
		hash_val = hash_combine(hash_val, hash_any(_value.input_assembly.primitive_restart_enable));
	}
	{ // viewport state
		hash_val = hash_combine(hash_val, hash_any(_value.viewport_state.enable_dynamic));
		for (const auto& viewport_ : _value.viewport_state.viewports) {
			hash_val = hash_combine(hash_val, hash_any(viewport_.x));
			hash_val = hash_combine(hash_val, hash_any(viewport_.y));
			hash_val = hash_combine(hash_val, hash_any(viewport_.width));
			hash_val = hash_combine(hash_val, hash_any(viewport_.height));
			hash_val = hash_combine(hash_val, hash_any(viewport_.minDepth));
			hash_val = hash_combine(hash_val, hash_any(viewport_.maxDepth));
		}
		for (const auto& scissor_ : _value.viewport_state.scissors) {
			hash_val = hash_combine(hash_val, hash_any(scissor_.extent.width));
			hash_val = hash_combine(hash_val, hash_any(scissor_.extent.height));
			hash_val = hash_combine(hash_val, hash_any(scissor_.offset.x));
			hash_val = hash_combine(hash_val, hash_any(scissor_.offset.y));
		}
	}
	{ // raster state
		hash_val = hash_combine(hash_val, hash_any(_value.raster_state.raster_discard_enabled));
		hash_val = hash_combine(hash_val, hash_any(_value.raster_state.polygon_mode));
		hash_val = hash_combine(hash_val, hash_any(_value.raster_state.cull_mode));
		hash_val = hash_combine(hash_val, hash_any(_value.raster_state.front_face));
		hash_val = hash_combine(hash_val, hash_any(_value.raster_state.depth_clamp_enabled));
		hash_val = hash_combine(hash_val, hash_any(_value.raster_state.depth_clamp));
		hash_val = hash_combine(hash_val, hash_any(_value.raster_state.depth_bias.enable));
		hash_val = hash_combine(hash_val, hash_any(_value.raster_state.depth_bias.constant_factor));
		hash_val = hash_combine(hash_val, hash_any(_value.raster_state.depth_bias.slope_factor));
		hash_val = hash_combine(hash_val, hash_any(_value.raster_state.line_width));
	}
	{ // multisample
		hash_val = hash_combine(hash_val, hash_any(_value.multisample_state.sample_count));
	}
	{ // shaders
		for (const auto& shader_name_ : _value.shader_files) {
			hash_val = hash_combine(hash_val, hash_any(shader_name_));
		}
	}
	{ // color blend info
		for (const auto& attachment_ : _value.color_blend.attachments) {
			hash_val = hash_combine(hash_val, hash_any(attachment_.alphaBlendOp));
			hash_val = hash_combine(hash_val, hash_any(attachment_.blendEnable));
			hash_val = hash_combine(hash_val, hash_any(attachment_.colorBlendOp));
			hash_val = hash_combine(hash_val, hash_any(attachment_.colorWriteMask));
			hash_val = hash_combine(hash_val, hash_any(attachment_.dstAlphaBlendFactor));
			hash_val = hash_combine(hash_val, hash_any(attachment_.dstColorBlendFactor));
			hash_val = hash_combine(hash_val, hash_any(attachment_.srcAlphaBlendFactor));
			hash_val = hash_combine(hash_val, hash_any(attachment_.srcColorBlendFactor));
		}
		hash_val = hash_combine(hash_val, hash_any(_value.color_blend.logic_op_enable));
		hash_val = hash_combine(hash_val, hash_any(_value.color_blend.logic_op));
		hash_val = hash_combine(hash_val, hash_any(_value.color_blend.blend_constants[0]));
		hash_val = hash_combine(hash_val, hash_any(_value.color_blend.blend_constants[1]));
		hash_val = hash_combine(hash_val, hash_any(_value.color_blend.blend_constants[2]));
		hash_val = hash_combine(hash_val, hash_any(_value.color_blend.blend_constants[3]));
	}
	{ // dynamic states
		for (const auto& dyn_state_ : _value.dynamic_states) {
			hash_val = hash_combine(hash_val, hash_any(dyn_state_));
		}
	}
	return hash_val;
}

void Pipeline::destroy() {
	parent_factory->destroy_pipeline(this);
}
