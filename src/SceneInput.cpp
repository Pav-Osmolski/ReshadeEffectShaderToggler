#include "SceneInput.h"
#include "PipelinePrivateData.h"
#include "SceneInputPolicy.h"
#include "SceneInputAllocation.h"
#include "StateTracking.h"
#include "WarframeCompositor.h"
#include "SceneInputSync.h"
#include <reshade.hpp>
#include <algorithm>
#include <d3d12.h>
#include <mutex>
#include <optional>
#include <wrl/client.h>

using namespace reshade::api;
using Microsoft::WRL::ComPtr;

namespace Rendering {
void RefreshSceneInputDescriptors(command_list* cmd) {
    auto& state = *cmd->get_private_data<state_tracking>();
    const auto& tracker = *cmd->get_device()->get_private_data<descriptor_tracking>();
    auto& [layout, roots] = state.root_tables[0];
    for (uint32_t i = 0; i < roots.size(); ++i) {
        auto& root = roots[i];
        if (root.type != StateTracking::root_entry_type::descriptor_table || root.descriptor_table == 0 || root.buffer_index < 0)
            continue;
        // Unsupported large/bindless tables were already captured at binding.
        // Do not rescan them on every attempted input or preview draw.
        if (i >= 64 || root.coverage.unbounded || root.coverage.clipped || state.get_root_table_entry_size_at(0, i) > 255)
            continue;
        auto snapshot = CaptureDescriptorTable<descriptor_tracking::descriptor_data>(
          tracker.get_pipeline_layout_param(layout, i), [&](const descriptor_range& range, uint32_t count, auto& descriptors) {
              descriptor_heap heap = { 0 };
              uint32_t offset = 0;
              cmd->get_device()->get_descriptor_heap_offset(root.descriptor_table, range.binding, 0, &heap, &offset);
              return tracker.set_all_descriptors(heap, offset, count, descriptors, range.binding);
          });
        state.descriptor_buffer[0][root.buffer_index] = std::move(snapshot.descriptors);
        root.coverage = snapshot.coverage;
    }
}

struct SceneInputStorage {
    device* dev = nullptr;
    resource texture = {0}, alpha = {0};
    std::shared_ptr<GlobalResourceView> views, alphaViews;
    uint32_t width = 0, height = 0;
    format pixelFormat = format::unknown;
    uint64_t bytes = 0;
    bool shaderReadable = false, alphaReadable = false;
    SceneInputOwnership lifetime;

    void Dispose() {
        if (!dev) return;
        if (views) views->Dispose();
        if (alphaViews) alphaViews->Dispose();
        if (texture != 0) dev->destroy_resource(texture);
        if (alpha != 0) dev->destroy_resource(alpha);
        texture = alpha = {0};
        dev = nullptr;
    }
    ~SceneInputStorage() { Dispose(); }
};

void SceneInputLease::Dispose() {
    if (device == nullptr)
        return;
    views.reset();
    alphaViews.reset();
    for (auto table : tables)
        if (table != 0)
            device->free_descriptor_table(table);
    tables.clear();
    texture = { 0 };
    alpha = {0};
    split.reset();
    storage.reset();
    dependencies.clear();
    device = nullptr;
}

class SceneInputPool {
  public:
    struct Entry {
        std::shared_ptr<SceneInputLease> lease;
        bool retired = false, reusable = false;
        std::vector<SceneTicket> tickets;
        std::vector<command_queue*> submissions;
    };
    std::mutex mutex;
    std::vector<Entry> entries;
    std::vector<std::shared_ptr<SceneInputStorage>> images;
    std::unordered_map<command_queue*, std::shared_ptr<SceneFence>> fences;
    uint64_t bytes = 0;
    uint64_t imageReuses = 0, gpuHandoffs = 0, pressureSkips = 0;
    bool waitFailed = false;
    static bool Complete(const Entry& entry) {
        if (!entry.retired || !entry.reusable)
            return false;
        return SceneTicketsComplete(entry.tickets);
    }
};

namespace {
struct PixelBinding {
    uint32_t slot, offset;
};

std::optional<PixelBinding> FindPixelBinding(const descriptor_tracking& tracking, pipeline_layout layout,
                                            uint32_t paramCount, uint32_t shaderRegister) {
    std::optional<PixelBinding> result;
    for (uint32_t slot = 0; slot < paramCount; ++slot) {
        const auto param = tracking.get_pipeline_layout_param(layout, slot);
        const bool flagged = param.type == pipeline_layout_param_type::descriptor_table_with_flags;
        if (!flagged && param.type != pipeline_layout_param_type::descriptor_table)
            continue;
        const uint32_t count = flagged ? param.descriptor_table_with_flags.count : param.descriptor_table.count;
        for (uint32_t i = 0; i < count; ++i) {
            const descriptor_range range = flagged ? param.descriptor_table_with_flags.ranges[i] : param.descriptor_table.ranges[i];
            if (range.type != descriptor_type::shader_resource_view || range.dx_register_space != 0 ||
                range.visibility != shader_stage::pixel || range.dx_register_index > shaderRegister ||
                shaderRegister - range.dx_register_index >= range.count)
                continue;
            const uint64_t offset = uint64_t(range.binding) + shaderRegister - range.dx_register_index;
            if (result || offset >= 255)
                return {};
            result = PixelBinding{slot, static_cast<uint32_t>(offset)};
        }
    }
    return result;
}

bool SplitTargetMatches(device* dev, const state_tracking& state, uint32_t width, uint32_t height) {
    if (state.render_targets.size() != 1 || state.render_targets[0] == 0 || state.depth_stencil != 0 ||
        state.viewports.size() != 1 || state.scissor_rects.size() != 1 ||
        state.primitive_topology != primitive_topology::triangle_list)
        return false;
    const auto view = dev->get_resource_view_desc(state.render_targets[0]);
    const auto target = dev->get_resource_from_view(state.render_targets[0]);
    if (target == 0 || view.format != format::r10g10b10a2_unorm ||
        view.type != resource_view_type::texture_2d || view.texture.first_level != 0)
        return false;
    const auto desc = dev->get_resource_desc(target);
    if (!SupportsWholePreviewCopy(desc) || desc.texture.width != width || desc.texture.height != height)
        return false;
    const auto& vp = state.viewports[0];
    const auto& scissor = state.scissor_rects[0];
    return vp.x == 0 && vp.y == 0 && vp.width == static_cast<float>(width) && vp.height == static_cast<float>(height) &&
           vp.min_depth == 0 && vp.max_depth == 1 &&
           scissor.left == 0 && scissor.top == 0 && scissor.right == static_cast<int32_t>(width) &&
           scissor.bottom == static_cast<int32_t>(height);
}
}

std::shared_ptr<SceneInputLease> PrepareSceneInput(command_list* cmd, uint32_t slot, uint32_t index, resource_view& source, const char*& status) {
    status = "Scene input: incomplete descriptor tracking";
    source = { 0 };
    device* dev = cmd->get_device();
    auto& data = *dev->get_private_data<DeviceDataContainer>();
    auto& recording = *cmd->get_private_data<CommandListDataContainer>();
    auto& state = *cmd->get_private_data<state_tracking>();
    if (dev->get_api() != device_api::d3d12 || !state_tracking::descriptor_tracking_enabled())
        return {};
    if (!recording.sceneInputAllowed) {
        status = "Scene input: direct graphics draws only";
        return {};
    }
    std::shared_ptr<WarframeSplitPipeline> split;
    {
        std::lock_guard lock(data.scenePipelineMutex);
        if (WarframeSplitEnabled()) {
            const auto found = data.warframePipelines.find(state.current_pipeline[0].handle);
            if (found == data.warframePipelines.end()) {
                status = "Warframe split: compositor shader revision or pipeline is unsupported";
                return {};
            }
            split = found->second;
            if (split->scene == 0 || split->ui == 0) {
                status = split->status;
                return {};
            }
            if (!recording.warframeSplitDraw) {
                status = "Warframe split: expected one direct three-vertex draw";
                return {};
            }
        }
        if (!data.supportedSceneInputPipelines.contains(state.current_pipeline[0].handle)) {
            status = "Scene input: shader model, heap indexing or unobserved pipeline is unsupported";
            return {};
        }
    }
    if (reinterpret_cast<ID3D12GraphicsCommandList*>(cmd->get_native())->GetType() != D3D12_COMMAND_LIST_TYPE_DIRECT || recording.sceneInputTainted) {
        status = "Scene input: bundles or secondary command-list state are unsupported";
        return {};
    }
    if (recording.d3d12InsideRenderPass) {
        status = "Scene input: render passes are unsupported";
        return {};
    }
    if (recording.sceneInputPending) {
        status = "Scene input: one group per draw is supported";
        return {};
    }
    const auto& [layout, roots] = state.root_tables[0];
    const auto& tracking = *dev->get_private_data<descriptor_tracking>();
    const uint32_t paramCount = tracking.get_pipeline_layout_param_count(layout);
    if (layout == 0 || paramCount == 0 || paramCount > 64 || slot >= roots.size())
        return {};
    const auto* selected = state.get_descriptor_at(0, slot, index);
    if (!selected || selected->type != descriptor_type::shader_resource_view || selected->view == 0) {
        status = "Scene input: exact selection is not a tracked texture SRV";
        return {};
    }
    if (!selected->nativeSnapshot) {
        status = "Scene input: CPU descriptor snapshot unavailable (restart with descriptor tracking enabled)";
        return {};
    }
    source = selected->nativeSnapshot->view;
    const resource res = selected->nativeSnapshot->resource;
    if (res == 0)
        return {};
    const auto desc = dev->get_resource_desc(res);
    const auto viewDesc = selected->nativeSnapshot->description;
    if (const char* reason = SceneInputShapeRejection(desc, viewDesc)) {
        status = reason;
        return {};
    }
    const format targetFormat = split ? format::r32g32b32a32_float : viewDesc.format;
    std::optional<PixelBinding> alphaBinding;
    if (split) {
        const auto colorBinding = FindPixelBinding(tracking, layout, paramCount, 0);
        alphaBinding = FindPixelBinding(tracking, layout, paramCount, 1);
        const auto uiBinding = FindPixelBinding(tracking, layout, paramCount, 8);
        if (layout != split->layout || !colorBinding || colorBinding->slot != slot || colorBinding->offset != index ||
            !alphaBinding || !uiBinding) {
            status = "Warframe split: select the tracked pixel t0 scene and keep the original compositor layout";
            return {};
        }
        const auto* ui = state.get_descriptor_at(0, uiBinding->slot, uiBinding->offset);
        if (!ui || ui->type != descriptor_type::shader_resource_view || !ui->nativeSnapshot ||
            !ui->nativeSnapshot->resource.handle || ui->nativeSnapshot->description.format != format::r8g8b8a8_unorm) {
            status = "Warframe split: expected the original UI texture at pixel t8";
            return {};
        }
        const auto uiDesc = dev->get_resource_desc(ui->nativeSnapshot->resource);
        if (viewDesc.format != format::r11g11b10_float || !SupportsWholePreviewCopy(uiDesc) ||
            ui->nativeSnapshot->description.type != resource_view_type::texture_2d ||
            ui->nativeSnapshot->description.texture.first_level != 0 ||
            !SplitTargetMatches(dev, state, desc.texture.width, desc.texture.height) ||
            uiDesc.texture.width != desc.texture.width || uiDesc.texture.height != desc.texture.height) {
            status = "Warframe split: expected matching scene, UI and full HDR10 viewport";
            return {};
        }
    }
    constexpr auto privateUsage = resource_usage::render_target | resource_usage::shader_resource;
    if (!dev->check_format_support(targetFormat, privateUsage) ||
        (split && !dev->check_format_support(format::r32_float, privateUsage))) {
        status = "Scene input: format lacks render-target support";
        return {};
    }
    std::vector<uint32_t> sizes(paramCount);
    descriptor_heap originalResourceHeap = { 0 }, originalSamplerHeap = { 0 };
    for (uint32_t i = 0; i < paramCount; ++i) {
        const auto param = tracking.get_pipeline_layout_param(layout, i);
        if (IsStaticSamplerParameter(param))
            continue; // SDK appends static samplers as a virtual parameter.
        if (param.type != pipeline_layout_param_type::descriptor_table && param.type != pipeline_layout_param_type::descriptor_table_with_flags)
            continue;
        if (i >= roots.size() || roots[i].type != StateTracking::root_entry_type::descriptor_table || roots[i].descriptor_table == 0)
            return {};
        const auto& coverage = roots[i].coverage;
        descriptor_heap heap = { 0 };
        uint32_t offset = 0;
        dev->get_descriptor_heap_offset(roots[i].descriptor_table, 0, 0, &heap, &offset);
        auto& originalHeap = coverage.samplers == coverage.ranges ? originalSamplerHeap : originalResourceHeap;
        if (heap == 0 || (originalHeap != 0 && originalHeap != heap)) {
            status = "Scene input: descriptor tables span incompatible heaps";
            return {};
        }
        originalHeap = heap;
        if (coverage.supported && coverage.ranges != 0 && coverage.samplers == coverage.ranges && coverage.unbounded == 0 && coverage.clipped == 0 &&
            coverage.missing == 0)
            continue;
        const size_t size = state.get_root_table_entry_size_at(0, i);
        if (!CanCloneSceneTable(coverage, size)) {
            status = "Scene input: unsupported or incomplete descriptor table";
            return {};
        }
        for (uint32_t j = 0; j < size; ++j) {
            const auto* descriptor = state.get_descriptor_at(0, i, j);
            if (!descriptor || static_cast<uint32_t>(descriptor->type) == UINT32_MAX)
                continue; // Layout gap.
            if (descriptor->type == descriptor_type::constant_buffer) {
                if (descriptor->constant.buffer == 0)
                    return {};
            } else if (!descriptor->nativeSnapshot) {
                status = "Scene input: table lacks retained CPU descriptors";
                return {};
            }
        }
        sizes[i] = static_cast<uint32_t>(size);
    }
    if (slot >= sizes.size() || index >= sizes[slot] ||
        (alphaBinding && alphaBinding->offset >= sizes[alphaBinding->slot]))
        return {};
    for (const auto& stage : state.root_tables) {
        for (const auto& root : stage.second) {
            if (root.type != StateTracking::root_entry_type::descriptor_table || root.descriptor_table == 0)
                continue;
            descriptor_heap heap = { 0 };
            uint32_t offset = 0;
            dev->get_descriptor_heap_offset(root.descriptor_table, 0, 0, &heap, &offset);
            const bool sampler = root.coverage.ranges != 0 && root.coverage.samplers == root.coverage.ranges;
            const auto expected = sampler ? originalSamplerHeap : originalResourceHeap;
            if (expected == 0 || heap != expected) {
                status = "Scene input: graphics/compute descriptor heap pair cannot be restored safely";
                return {};
            }
        }
    }
    const auto selectedParam = tracking.get_pipeline_layout_param(layout, slot);
    const bool flagged = selectedParam.type == pipeline_layout_param_type::descriptor_table_with_flags;
    const uint32_t ranges = flagged ? selectedParam.descriptor_table_with_flags.count : selectedParam.descriptor_table.count;
    bool pixelOnly = false;
    for (uint32_t i = 0; i < ranges; ++i) {
        const descriptor_range range = flagged ? selectedParam.descriptor_table_with_flags.ranges[i] : selectedParam.descriptor_table.ranges[i];
        if (index < range.binding || index - range.binding >= range.count)
            continue;
        if (range.visibility != shader_stage::pixel) {
            status = "Scene input: selected descriptor must be visible only to the pixel shader";
            return {};
        }
        pixelOnly = true;
    }
    if (!pixelOnly)
        return {};
    std::shared_ptr<SceneInputPool> poolOwner;
    {
        std::lock_guard queueLock(data.queueMutex);
        if (!data.sceneInputPool)
            data.sceneInputPool = std::make_shared<SceneInputPool>();
        poolOwner = data.sceneInputPool;
    }
    auto& pool = *poolOwner;
    std::lock_guard lock(pool.mutex);
    // Only descriptor tables and pipeline references wait for CPU-observed
    // completion. Scratch images may be handed off with GPU queue waits.
    std::erase_if(pool.entries, SceneInputPool::Complete);
    if (pool.entries.size() >= 64) {
        ++pool.pressureSkips;
        status = "Scene input: waiting for immutable descriptor recordings";
        return {};
    }
    std::shared_ptr<SceneInputStorage> image;
    for (auto& candidate : pool.images) {
        if (candidate->lifetime.CanAcquire(cmd) && candidate->width == desc.texture.width &&
            candidate->height == desc.texture.height && candidate->pixelFormat == targetFormat &&
            (candidate->alpha != 0) == static_cast<bool>(split)) {
            image = candidate;
            break;
        }
    }
    if (!image) {
        const uint64_t bytes = static_cast<uint64_t>(desc.texture.width) * desc.texture.height *
                               (split ? 20 : targetFormat == format::r16g16b16a16_float ? 8 : 4);
        const uint64_t budget = (split ? 512ull : 256ull) * 1024 * 1024;
        for (auto it = pool.images.begin(); it != pool.images.end();) {
            const auto& candidate = *it;
            if (pool.bytes + bytes > budget && candidate.use_count() == 1 && candidate->lifetime.owner == nullptr &&
                SceneTicketsComplete(candidate->lifetime.dependencies)) {
                pool.bytes -= candidate->bytes;
                it = pool.images.erase(it);
            } else
                ++it;
        }
        if (bytes > budget || pool.bytes + bytes > budget) {
            ++pool.pressureSkips;
            status = "Scene input: waiting for private GPU resources";
            return {};
        }
        image = std::make_shared<SceneInputStorage>();
        image->dev = dev;
        image->width = desc.texture.width;
        image->height = desc.texture.height;
        image->pixelFormat = targetFormat;
        image->bytes = bytes;
        resource_desc target(image->width,
                             image->height,
                             1,
                             1,
                             image->pixelFormat,
                             1,
                             memory_heap::default_,
                             resource_usage::render_target | resource_usage::shader_resource | resource_usage::copy_source);
        if (!dev->create_resource(target, nullptr, resource_usage::render_target, &image->texture)) {
            status = "Scene input: private texture allocation failed";
            return {};
        }
        image->views = std::make_shared<GlobalResourceView>(dev, image->texture, image->pixelFormat);
        if (image->views->rtv == 0 || image->views->srv == 0)
            return {};
        if (split) {
            target.texture.format = format::r32_float;
            target.usage = privateUsage;
            if (!dev->create_resource(target, nullptr, resource_usage::render_target, &image->alpha)) {
                status = "Warframe split: private alpha allocation failed";
                return {};
            }
            image->alphaViews = std::make_shared<GlobalResourceView>(dev, image->alpha, format::r32_float);
            if (image->alphaViews->rtv == 0 || image->alphaViews->srv == 0) {
                status = "Warframe split: private alpha views unavailable";
                return {};
            }
        }
        pool.bytes += bytes;
        pool.images.push_back(image);
    } else {
        ++pool.imageReuses;
        if (!SceneTicketsComplete(image->lifetime.dependencies))
            ++pool.gpuHandoffs;
    }
    auto lease = std::make_shared<SceneInputLease>();
    lease->device = dev;
    lease->storage = image;
    lease->texture = image->texture;
    lease->alpha = image->alpha;
    lease->views = image->views;
    lease->alphaViews = image->alphaViews;
    lease->width = image->width;
    lease->height = image->height;
    lease->format = image->pixelFormat;
    lease->tables.resize(paramCount);
    if (image->lifetime.owner) {
        // Prior draw in the same recording. Its commands precede this draw.
        const auto* previous = static_cast<const SceneInputLease*>(image->lifetime.owner);
        lease->shaderReadable = previous->shaderReadable;
        lease->alphaReadable = previous->alphaReadable;
    } else {
        lease->shaderReadable = image->shaderReadable;
        lease->alphaReadable = image->alphaReadable;
    }
    lease->dependencies = image->lifetime.dependencies;
    if (!AllocateSceneInputTables(*dev, sizes, lease->tables)) {
        status = "Scene input: REST-owned descriptor allocation failed";
        return {};
    }
    descriptor_heap commonHeap = { 0 };
    for (uint32_t i = 0; i < paramCount; ++i) {
        if (sizes[i] == 0)
            continue;
        descriptor_heap heap = { 0 };
        uint32_t offset = 0;
        dev->get_descriptor_heap_offset(lease->tables[i], 0, 0, &heap, &offset);
        if (heap == 0 || (commonHeap != 0 && commonHeap != heap))
            return {};
        commonHeap = heap;
    }
    image->lifetime.Acquire(lease.get(), cmd);
    pool.entries.push_back({ lease, false, false, {}, {} });
    lease->split = split;
    // Keep even an unsuccessful attempt alive until recording retirement.
    recording.sceneInputLeases.push_back(lease);
    descriptor_heap privateHeap = { 0 };
    uint32_t privateOffset = 0;
    dev->get_descriptor_heap_offset(lease->tables[slot], 0, 0, &privateHeap, &privateOffset);
    if (privateHeap == originalResourceHeap) {
        status = "Scene input: game and private descriptor heaps must be distinct";
        return {};
    }
    for (uint32_t i = 0; i < paramCount; ++i) {
        for (uint32_t j = 0; j < sizes[i]; ++j) {
            const auto* descriptor = state.get_descriptor_at(0, i, j);
            if (!descriptor || descriptor->type == static_cast<descriptor_type>(UINT32_MAX))
                continue; // Layout gap.
            descriptor_table_update copy = {};
            copy.table = lease->tables[i];
            copy.binding = j;
            copy.count = 1;
            copy.type = descriptor->type;
            copy.descriptors = descriptor->type == descriptor_type::constant_buffer ? static_cast<const void*>(&descriptor->constant)
                                                                                    : static_cast<const void*>(&descriptor->nativeSnapshot->view);
            dev->update_descriptor_tables(1, &copy);
        }
    }
    descriptor_table_update update = {};
    update.table = lease->tables[slot];
    update.binding = index;
    update.count = 1;
    update.type = descriptor_type::shader_resource_view;
    update.descriptors = &lease->views->srv;
    dev->update_descriptor_tables(1, &update);
    if (alphaBinding) {
        update.table = lease->tables[alphaBinding->slot];
        update.binding = alphaBinding->offset;
        update.descriptors = &lease->alphaViews->srv;
        dev->update_descriptor_tables(1, &update);
    }
    status = "Scene input: private copy prepared";
    return lease;
}

void RecordWarframeSplitScene(command_list* cmd, SceneInputLease& lease) {
    auto& recording = *cmd->get_private_data<CommandListDataContainer>();
    auto& state = *cmd->get_private_data<state_tracking>();
    if (lease.alphaReadable)
        cmd->barrier(lease.alpha, resource_usage::shader_resource, resource_usage::render_target);
    const resource_view targets[]{lease.views->rtv, lease.alphaViews->rtv};
    const float clear[4]{};
    for (auto target : targets)
        cmd->clear_render_target_view(target, clear);
    cmd->bind_render_targets_and_depth_stencil(2, targets);
    cmd->bind_pipeline(pipeline_stage::all_graphics, lease.split->scene);
    cmd->draw(3, 1, recording.warframeFirstVertex, recording.warframeFirstInstance);
    cmd->bind_render_targets_and_depth_stencil(1, state.render_targets.data());
    cmd->bind_pipeline(pipeline_stage::all_graphics, state.current_pipeline[0]);
    cmd->barrier(lease.alpha, resource_usage::render_target, resource_usage::shader_resource);
    lease.alphaReadable = true;
}

void BindSceneInput(command_list* cmd) {
    auto& recording = *cmd->get_private_data<CommandListDataContainer>();
    if (!recording.sceneInputPending)
        return;
    const auto& lease = *recording.sceneInputPending;
    auto& state = *cmd->get_private_data<state_tracking>();
    const auto& [layout, roots] = state.root_tables[0];
    // First move BOTH heap types away from the game's pair. ReShade's zero-table
    // restore can then recover even an otherwise unobserved sampler heap from
    // its native SetDescriptorHeaps cache. No game command can interleave here.
    {
        auto& shaders = cmd->get_device()->get_private_data<DeviceDataContainer>()->customShader;
        std::lock_guard lock(shaders.pipelineMutex);
        const auto& copy = shaders.sceneFormatPipelines.at(static_cast<uint32_t>(lease.format));
        cmd->push_descriptors(
          shader_stage::pixel, copy.pipelineLayout, 0, descriptor_table_update{ {}, 0, 0, 1, descriptor_type::sampler, &copy.pipelineSampler });
        cmd->push_descriptors(
          shader_stage::pixel, copy.pipelineLayout, 1, descriptor_table_update{ {}, 0, 0, 1, descriptor_type::shader_resource_view, &lease.views->srv });
    }
    state.apply_descriptors_dx12_vulkan(cmd);
    // D3D12 has one active PSO, even when graphics and compute roots coexist.
    cmd->bind_pipeline(pipeline_stage::all_graphics, lease.split ? lease.split->ui : state.current_pipeline[0]);
    // Establish both heaps before setting root tables: switching either heap
    // invalidates all its root-table bindings. Preserve the game's sampler heap.
    for (int pass = 0; pass < 2; ++pass)
        for (uint32_t i = 0; i < roots.size(); ++i) {
            if (roots[i].type != StateTracking::root_entry_type::descriptor_table || roots[i].descriptor_table == 0)
                continue;
            const descriptor_table table = i < lease.tables.size() && lease.tables[i] != 0 ? lease.tables[i] : roots[i].descriptor_table;
            cmd->bind_descriptor_tables(shader_stage::all_graphics, layout, i, 1, &table);
        }
    recording.sceneInputPending.reset();
    recording.sceneInputPatched = true;
}
void RestoreSceneInput(command_list* cmd) {
    auto& recording = *cmd->get_private_data<CommandListDataContainer>();
    if (!recording.sceneInputPatched)
        return;
    // Called immediately after the replacement draw, before ANY game command
    // can change root state/heaps. All observed tables share the validated pair.
    auto& state = *cmd->get_private_data<state_tracking>();
    state.apply_descriptors_dx12_vulkan(cmd);
    cmd->bind_pipeline(pipeline_stage::all_graphics, state.current_pipeline[0]);
    if (recording.sceneInputGroup) {
        std::unique_lock<std::shared_mutex> lock(cmd->get_device()->get_private_data<DeviceDataContainer>()->render_mutex);
        recording.sceneInputGroup->recordSceneInputDraw(WarframeSplitEnabled());
    }
    recording.sceneInputGroup = nullptr;
    recording.sceneInputPatched = false;
}
void RetireSceneInputs(command_list* cmd) {
    auto& recording = *cmd->get_private_data<CommandListDataContainer>();
    if (recording.sceneInputLeases.empty())
        return;
    auto& data = *cmd->get_device()->get_private_data<DeviceDataContainer>();
    std::lock_guard queueLock(data.queueMutex);
    auto pool = data.sceneInputPool;
    if (!pool)
        return;
    std::lock_guard lock(pool->mutex);
    std::vector<SceneTicket> tickets;
    bool reusable = true;
    std::vector<command_queue*> submittedQueues;
    for (const auto& lease : recording.sceneInputLeases)
        for (const auto& entry : pool->entries)
            if (entry.lease == lease)
                for (auto* queue : entry.submissions)
                    if (std::find(submittedQueues.begin(), submittedQueues.end(), queue) == submittedQueues.end())
                        submittedQueues.push_back(queue);
    auto* nativeDevice = reinterpret_cast<ID3D12Device*>(cmd->get_device()->get_native());
    for (auto* queue : submittedQueues) {
        auto& fence = pool->fences[queue];
        if (!fence) {
            fence = std::make_shared<SceneFence>();
            if (FAILED(nativeDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence->fence.GetAddressOf())))) {
                fence.reset();
                reusable = false;
                continue;
            }
        }
        auto* nativeQueue = reinterpret_cast<ID3D12CommandQueue*>(queue->get_native());
        const uint64_t value = ++fence->value;
        if (FAILED(nativeQueue->Signal(fence->fence.Get(), value)))
            reusable = false;
        else
            tickets.push_back({ fence, value });
    }
    // Reset/destruction ends possible resubmission of the old recording.
    // Preserve its immutable tables until these completion tickets pass.
    for (const auto& lease : recording.sceneInputLeases)
        for (auto& entry : pool->entries)
            if (entry.lease == lease) {
                entry.retired = true;
                entry.reusable = reusable;
                entry.tickets = lease->dependencies;
                MergeSceneTickets(entry.tickets, tickets);
                auto& image = *lease->storage;
                if (image.lifetime.owner == lease.get()) {
                    image.shaderReadable = lease->shaderReadable;
                    image.alphaReadable = lease->alphaReadable;
                    image.lifetime.Retire(lease.get(), reusable, tickets);
                }
            }
    recording.sceneInputLeases.clear();
    recording.sceneInputPending.reset();
    recording.sceneInputPatched = false;
    recording.sceneInputGroup = nullptr;
}

void WaitForSceneInputs(command_queue* queue, command_list* cmd) {
    if (queue->get_device()->get_api() != device_api::d3d12)
        return;
    const auto* recording = cmd->get_private_data<CommandListDataContainer>();
    if (!recording || recording->sceneInputLeases.empty())
        return;
    auto& data = *queue->get_device()->get_private_data<DeviceDataContainer>();
    std::shared_ptr<SceneInputPool> pool;
    std::vector<SceneTicket> dependencies;
    {
        std::lock_guard queueLock(data.queueMutex);
        pool = data.sceneInputPool;
        if (!pool)
            return;
        std::lock_guard lock(pool->mutex);
        for (const auto& lease : recording->sceneInputLeases) {
            for (auto& entry : pool->entries)
                if (entry.lease == lease && std::find(entry.submissions.begin(), entry.submissions.end(), queue) == entry.submissions.end())
                    entry.submissions.push_back(queue);
            MergeSceneTickets(dependencies, lease->dependencies);
        }
    }
    auto* nativeQueue = reinterpret_cast<ID3D12CommandQueue*>(queue->get_native());
    if (FAILED(WaitSceneInputTickets(nativeQueue, dependencies))) {
        std::lock_guard lock(pool->mutex);
        for (auto& image : pool->images)
            image->lifetime.reusable = false;
        if (!pool->waitFailed)
            reshade::log::message(reshade::log::level::error, "REST scene-input queue wait failed; device recovery is required.");
        pool->waitFailed = true;
    }
}

void ForgetSceneInputQueue(command_queue* queue) {
    auto& data = *queue->get_device()->get_private_data<DeviceDataContainer>();
    auto pool = data.sceneInputPool;
    if (!pool)
        return;
    // The caller has waited for this queue and holds queueMutex. Its completed
    // submissions no longer need a queue object when their recording retires.
    std::lock_guard lock(pool->mutex);
    for (auto& entry : pool->entries)
        std::erase(entry.submissions, queue);
    pool->fences.erase(queue);
}

std::string SceneInputPoolDiagnostics(device* dev) {
    auto& data = *dev->get_private_data<DeviceDataContainer>();
    std::lock_guard queueLock(data.queueMutex);
    auto pool = data.sceneInputPool;
    if (!pool)
        return "Scene-input pool: not allocated\n";
    std::lock_guard lock(pool->mutex);
    size_t active = 0, retained = 0;
    for (const auto& image : pool->images)
        active += image->lifetime.owner != nullptr;
    for (const auto& entry : pool->entries)
        retained += entry.retired;
    return "Scene-input pool: " + std::to_string(pool->images.size()) + " image pairs | " +
           std::to_string(pool->bytes / (1024 * 1024)) + " MiB | active recordings " + std::to_string(active) +
           " | retained descriptor sets " + std::to_string(pool->entries.size()) + " | retired sets " +
           std::to_string(retained) + "\nImage reuses: " + std::to_string(pool->imageReuses) +
           " | GPU handoffs: " + std::to_string(pool->gpuHandoffs) +
           " | pool pressure skips: " + std::to_string(pool->pressureSkips) + "\n";
}

void DisposeSceneInputs(device* dev) {
    auto& data = *dev->get_private_data<DeviceDataContainer>();
    std::lock_guard queueLock(data.queueMutex);
    if (!data.sceneInputPool)
        return;
    for (auto* queue : data.queues)
        queue->wait_idle();
    // Invalidate leases also held
    // by command-list private data so their eventual destructor is idempotent.
    for (auto& entry : data.sceneInputPool->entries)
        entry.lease->Dispose();
    for (auto& image : data.sceneInputPool->images)
        image->Dispose();
    data.sceneInputPool.reset();
}
void SceneInputSecondary(command_list* cmd, command_list*) {
    RestoreSceneInput(cmd);
    if (cmd->get_device()->get_api() == device_api::d3d12)
        cmd->get_private_data<CommandListDataContainer>()->sceneInputTainted = true;
}
}
