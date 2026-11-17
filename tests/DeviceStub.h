#pragma once
#include <reshade_api.hpp>
#include <stdexcept>
#pragma warning(push)
#pragma warning(disable:4100)
namespace TestHost {
using namespace reshade::api;
// Unused methods throw: tests must not silently accept an unexpected API call.
struct DeviceStub : device {
    uint64_t get_native() const override { throw std::logic_error("Unexpected device API call"); }
    void get_private_data(const uint8_t guid[16], uint64_t *data) const override { throw std::logic_error("Unexpected device API call"); }
    void set_private_data(const uint8_t guid[16], const uint64_t data) override { throw std::logic_error("Unexpected device API call"); }
    device_api get_api() const override { throw std::logic_error("Unexpected device API call"); }
    bool check_capability(device_caps capability) const override { throw std::logic_error("Unexpected device API call"); }
    bool check_format_support(format format, resource_usage usage) const override { throw std::logic_error("Unexpected device API call"); }
    bool create_sampler(const sampler_desc &desc, sampler *out_sampler) override { throw std::logic_error("Unexpected device API call"); }
    void destroy_sampler(sampler sampler) override { throw std::logic_error("Unexpected device API call"); }
    bool create_resource(const resource_desc &desc, const subresource_data *initial_data, resource_usage initial_state, resource *out_resource, void **shared_handle = nullptr) override { throw std::logic_error("Unexpected device API call"); }
    void destroy_resource(resource resource) override { throw std::logic_error("Unexpected device API call"); }
    resource_desc get_resource_desc(resource resource) const override { throw std::logic_error("Unexpected device API call"); }
    bool create_resource_view(resource resource, resource_usage usage_type, const resource_view_desc &desc, resource_view *out_view) override { throw std::logic_error("Unexpected device API call"); }
    void destroy_resource_view(resource_view view) override { throw std::logic_error("Unexpected device API call"); }
    resource get_resource_from_view(resource_view view) const override { throw std::logic_error("Unexpected device API call"); }
    resource_view_desc get_resource_view_desc(resource_view view) const override { throw std::logic_error("Unexpected device API call"); }
    bool map_buffer_region(resource resource, uint64_t offset, uint64_t size, map_access access, void **out_data) override { throw std::logic_error("Unexpected device API call"); }
    void unmap_buffer_region(resource resource) override { throw std::logic_error("Unexpected device API call"); }
    bool map_texture_region(resource resource, uint32_t subresource, const subresource_box *box, map_access access, subresource_data *out_data) override { throw std::logic_error("Unexpected device API call"); }
    void unmap_texture_region(resource resource, uint32_t subresource) override { throw std::logic_error("Unexpected device API call"); }
    void update_buffer_region(const void *data, resource dest, uint64_t dest_offset, uint64_t size) override { throw std::logic_error("Unexpected device API call"); }
    void update_texture_region(const subresource_data &data, resource dest, uint32_t dest_subresource, const subresource_box *dest_box = nullptr) override { throw std::logic_error("Unexpected device API call"); }
    bool create_pipeline(pipeline_layout layout, uint32_t subobject_count, const pipeline_subobject *subobjects, pipeline *out_pipeline) override { throw std::logic_error("Unexpected device API call"); }
    void destroy_pipeline(pipeline pipeline) override { throw std::logic_error("Unexpected device API call"); }
    bool create_pipeline_layout(uint32_t param_count, const pipeline_layout_param *params, pipeline_layout *out_layout) override { throw std::logic_error("Unexpected device API call"); }
    void destroy_pipeline_layout(pipeline_layout layout) override { throw std::logic_error("Unexpected device API call"); }
    bool allocate_descriptor_tables(uint32_t count, pipeline_layout layout, uint32_t param, descriptor_table *out_tables) override { throw std::logic_error("Unexpected device API call"); }
    void free_descriptor_tables(uint32_t count, const descriptor_table *tables) override { throw std::logic_error("Unexpected device API call"); }
    void get_descriptor_heap_offset(descriptor_table table, uint32_t binding, uint32_t array_offset, descriptor_heap *out_heap, uint32_t *out_offset) const override { throw std::logic_error("Unexpected device API call"); }
    void copy_descriptor_tables(uint32_t count, const descriptor_table_copy *copies) override { throw std::logic_error("Unexpected device API call"); }
    void update_descriptor_tables(uint32_t count, const descriptor_table_update *updates) override { throw std::logic_error("Unexpected device API call"); }
    bool create_query_heap(query_type type, uint32_t count, query_heap *out_heap) override { throw std::logic_error("Unexpected device API call"); }
    void destroy_query_heap(query_heap heap) override { throw std::logic_error("Unexpected device API call"); }
    bool get_query_heap_results(query_heap heap, query_type type, uint32_t first, uint32_t count, void *results, uint32_t stride) override { throw std::logic_error("Unexpected device API call"); }
    void set_resource_name(resource resource, const char *name) override { throw std::logic_error("Unexpected device API call"); }
    void set_resource_view_name(resource_view view, const char *name) override { throw std::logic_error("Unexpected device API call"); }
    bool create_fence(uint64_t initial_value, fence_flags flags, fence *out_fence, void **shared_handle = nullptr) override { throw std::logic_error("Unexpected device API call"); }
    void destroy_fence(fence fence) override { throw std::logic_error("Unexpected device API call"); }
    uint64_t get_completed_fence_value(fence fence) const override { throw std::logic_error("Unexpected device API call"); }
    bool wait(fence fence, uint64_t value, uint64_t timeout = UINT64_MAX) override { throw std::logic_error("Unexpected device API call"); }
    bool signal(fence fence, uint64_t value) override { throw std::logic_error("Unexpected device API call"); }
    bool get_property(device_properties property, void *data) const override { throw std::logic_error("Unexpected device API call"); }
    uint64_t get_resource_view_gpu_address(resource_view view) const override { throw std::logic_error("Unexpected device API call"); }
    void get_acceleration_structure_size(acceleration_structure_type type, acceleration_structure_build_flags flags, uint32_t input_count, const acceleration_structure_build_input *inputs, uint64_t *out_size, uint64_t *out_build_scratch_size, uint64_t *out_update_scratch_size) const override { throw std::logic_error("Unexpected device API call"); }
    bool get_pipeline_shader_group_handles(pipeline pipeline, uint32_t first, uint32_t count, void *out_handles) override { throw std::logic_error("Unexpected device API call"); }
};
}
#pragma warning(pop)
