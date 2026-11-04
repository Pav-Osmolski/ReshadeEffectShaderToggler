#include "SceneInput.h"
#include "PipelinePrivateData.h"
#include "SceneInputPolicy.h"
#include "StateTracking.h"
#include <algorithm>
#include <d3d12.h>
#include <mutex>
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

void SceneInputLease::Dispose() {
    if (device == nullptr)
        return;
    if (views)
        views->Dispose();
    views.reset();
    for (auto table : tables)
        if (table != 0)
            device->free_descriptor_table(table);
    tables.clear();
    if (texture != 0)
        device->destroy_resource(texture);
    texture = { 0 };
    device = nullptr;
}

struct SceneFence {
    ComPtr<ID3D12Fence> fence;
    uint64_t value = 0;
};
struct SceneTicket {
    std::shared_ptr<SceneFence> fence;
    uint64_t value;
};
class SceneInputPool {
  public:
    struct Entry {
        std::shared_ptr<SceneInputLease> lease;
        bool retired = false, reusable = false;
        std::vector<SceneTicket> tickets;
    };
    std::mutex mutex;
    std::vector<Entry> entries;
    std::unordered_map<command_queue*, std::shared_ptr<SceneFence>> fences;
    uint64_t bytes = 0;
    static bool Complete(const Entry& entry) {
        if (!entry.retired || !entry.reusable)
            return false;
        for (const auto& ticket : entry.tickets) {
            const uint64_t value = ticket.fence->fence->GetCompletedValue();
            if (value == UINT64_MAX || value < ticket.value)
                return false;
        }
        return true;
    }
};

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
    {
        std::lock_guard lock(data.scenePipelineMutex);
        if (!data.classicGraphicsPipelines.contains(state.current_pipeline[0].handle)) {
            status = "Scene input: DXIL or unobserved graphics pipeline is unsupported";
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
    if (!dev->check_format_support(viewDesc.format, resource_usage::render_target | resource_usage::shader_resource)) {
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
    if (slot >= sizes.size() || index >= sizes[slot])
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
    if (!data.sceneInputPool)
        data.sceneInputPool = std::make_shared<SceneInputPool>();
    auto& pool = *data.sceneInputPool;
    std::lock_guard lock(pool.mutex);
    std::shared_ptr<SceneInputLease> lease;
    for (auto& entry : pool.entries) {
        auto& item = *entry.lease;
        if (SceneInputPool::Complete(entry) && item.width == desc.texture.width && item.height == desc.texture.height && item.format == viewDesc.format &&
            item.sizes == sizes) {
            lease = entry.lease;
            entry.retired = entry.reusable = false;
            entry.tickets.clear();
            break;
        }
    }
    if (!lease) {
        // Allow a bounded number of recordings in flight. Never stall a draw
        // waiting for a fence or recycle a texture referenced by a GPU command.
        const uint64_t bytes = static_cast<uint64_t>(desc.texture.width) * desc.texture.height * (viewDesc.format == format::r16g16b16a16_float ? 8 : 4);
        constexpr uint64_t budget = 256ull * 1024 * 1024;
        for (auto it = pool.entries.begin(); it != pool.entries.end();) {
            if ((pool.bytes + bytes > budget || pool.entries.size() >= 64) && SceneInputPool::Complete(*it)) {
                const auto& old = *it->lease;
                pool.bytes -= static_cast<uint64_t>(old.width) * old.height * (old.format == format::r16g16b16a16_float ? 8 : 4);
                it = pool.entries.erase(it);
            } else
                ++it;
        }
        if (bytes > budget || pool.bytes + bytes > budget || pool.entries.size() >= 64) {
            status = "Scene input: waiting for private GPU resources";
            return {};
        }
        lease = std::make_shared<SceneInputLease>();
        lease->device = dev;
        lease->width = desc.texture.width;
        lease->height = desc.texture.height;
        lease->format = viewDesc.format;
        lease->sizes = sizes;
        lease->tables.resize(paramCount);
        resource_desc target(lease->width,
                             lease->height,
                             1,
                             1,
                             lease->format,
                             1,
                             memory_heap::default_,
                             resource_usage::render_target | resource_usage::shader_resource | resource_usage::copy_source);
        if (!dev->create_resource(target, nullptr, resource_usage::render_target, &lease->texture)) {
            status = "Scene input: private texture allocation failed";
            return {};
        }
        lease->views = std::make_shared<GlobalResourceView>(dev, lease->texture, lease->format);
        if (lease->views->rtv == 0 || lease->views->srv == 0)
            return {};
        descriptor_heap commonHeap = { 0 };
        for (uint32_t i = 0; i < paramCount; ++i) {
            if (sizes[i] == 0)
                continue;
            if (!dev->allocate_descriptor_table(layout, i, &lease->tables[i])) {
                status = "Scene input: private descriptor allocation failed";
                return {};
            }
            descriptor_heap heap = { 0 };
            uint32_t offset = 0;
            dev->get_descriptor_heap_offset(lease->tables[i], 0, 0, &heap, &offset);
            if (heap == 0 || (commonHeap != 0 && commonHeap != heap))
                return {};
            commonHeap = heap;
        }
        pool.bytes += bytes;
        pool.entries.push_back({ lease, false, false, {} });
    }
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
    status = "Scene input: private copy prepared";
    return lease;
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
    cmd->bind_pipeline(pipeline_stage::all_graphics, state.current_pipeline[0]);
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
    cmd->get_private_data<state_tracking>()->apply_descriptors_dx12_vulkan(cmd);
    recording.sceneInputPatched = false;
}
void RetireSceneInputs(command_list* cmd) {
    auto& recording = *cmd->get_private_data<CommandListDataContainer>();
    if (recording.sceneInputLeases.empty())
        return;
    auto& data = *cmd->get_device()->get_private_data<DeviceDataContainer>();
    auto pool = data.sceneInputPool;
    if (!pool)
        return;
    std::lock_guard queueLock(data.queueMutex);
    std::lock_guard lock(pool->mutex);
    std::vector<SceneTicket> tickets;
    bool reusable = !data.queues.empty();
    auto* nativeDevice = reinterpret_cast<ID3D12Device*>(cmd->get_device()->get_native());
    for (auto* queue : data.queues) {
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
    // Called at reset/destruction, when the old recording cannot be submitted
    // again. execute_command_list is BEFORE submission and is unsafe for this.
    for (const auto& lease : recording.sceneInputLeases)
        for (auto& entry : pool->entries)
            if (entry.lease == lease) {
                entry.retired = true;
                entry.reusable = reusable;
                entry.tickets = tickets;
            }
    recording.sceneInputLeases.clear();
    recording.sceneInputPending.reset();
    recording.sceneInputPatched = false;
}
void DisposeSceneInputs(device* dev) {
    auto& data = *dev->get_private_data<DeviceDataContainer>();
    if (!data.sceneInputPool)
        return;
    std::lock_guard queueLock(data.queueMutex);
    for (auto* queue : data.queues)
        queue->wait_idle();
    // Invalidate leases also held
    // by command-list private data so their eventual destructor is idempotent.
    for (auto& entry : data.sceneInputPool->entries)
        entry.lease->Dispose();
    data.sceneInputPool.reset();
}
void SceneInputSecondary(command_list* cmd, command_list*) {
    RestoreSceneInput(cmd);
    if (cmd->get_device()->get_api() == device_api::d3d12)
        cmd->get_private_data<CommandListDataContainer>()->sceneInputTainted = true;
}
}
