#include "SceneInputAllocation.h"
#include "SceneTechniqueObservation.h"
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <unordered_set>

using namespace reshade::api;
static void require(bool value) {
    if (!value) { std::fputs("Scene-input processing contract regression\n", stderr); std::exit(EXIT_FAILURE); }
}
struct Host {
    bool failCreate = false, layoutLive = false;
    unsigned failAllocation = 0, allocations = 0, created = 0, destroyed = 0, freed = 0;
    std::unordered_set<uint64_t> liveTables;
    bool create_pipeline_layout(uint32_t count, const pipeline_layout_param* params, pipeline_layout* out) {
        ++created;
        require(count == 1 && params->type == pipeline_layout_param_type::descriptor_table);
        require(params->descriptor_table.count == 1 && params->descriptor_table.ranges[0].count == 16);
        if (failCreate) return false;
        *out = {2}; layoutLive = true; return true;
    }
    void destroy_pipeline_layout(pipeline_layout layout) { require(layout == 2 && layoutLive); layoutLive = false; ++destroyed; }
    bool allocate_descriptor_table(pipeline_layout layout, uint32_t param, descriptor_table* out) {
        // The native game layout (99) has no SDK allocation metadata. Tables
        // allocated through the API-owned layout (2) survive its destruction.
        if (layout != 2 || !layoutLive) return false;
        require(param == 0);
        ++allocations;
        if (allocations == failAllocation) return false;
        *out = {1000 + allocations}; liveTables.insert(out->handle); return true;
    }
    void free_descriptor_table(descriptor_table table) { require(liveTables.erase(table.handle) == 1); ++freed; }
};
int main() {
    const std::vector<uint32_t> sizes{2,0,16,0,0,0};
    Host host;
    descriptor_table original = {0};
    require(!host.allocate_descriptor_table({99}, 2, &original));
    std::puts("Host contract: native game layout without SDK metadata rejects table allocation.");
    std::vector<descriptor_table> tables;
    require(Rendering::AllocateSceneInputTables(host, sizes, tables));
    require(host.created == 1 && host.destroyed == 1 && host.allocations == 2 && host.freed == 0);
    require(!host.layoutLive && host.liveTables.size() == 2);
    std::puts("Host contract: REST-owned allocation layout succeeds; allocated tables survive its destruction.");
    require(tables[0] != 0 && tables[1] == 0 && tables[2] != 0 && tables[3] == 0);
    for (auto table : tables) if (table != 0) host.free_descriptor_table(table);
    require(host.liveTables.empty());
    Host failure; failure.failAllocation = 2; tables.clear();
    require(!Rendering::AllocateSceneInputTables(failure, sizes, tables));
    require(failure.destroyed == 1 && failure.freed == 1 && failure.liveTables.empty());
    for (auto table : tables) require(table == 0);
    Host createFailure; createFailure.failCreate = true;
    require(!Rendering::AllocateSceneInputTables(createFailure, sizes, tables));
    require(createFailure.allocations == 0 && createFailure.destroyed == 0);
    Host invalid; tables.clear();
    require(!Rendering::AllocateSceneInputTables(invalid, {256}, tables));
    require(!Rendering::AllocateSceneInputTables(invalid, {0}, tables));
    tables = {{77}};
    require(!Rendering::AllocateSceneInputTables(invalid, sizes, tables));
    require(tables[0] == 77 && invalid.created == 0);

    auto* runtime = reinterpret_cast<effect_runtime*>(uintptr_t{1});
    auto* command = reinterpret_cast<command_list*>(uintptr_t{2});
    using Observation = Rendering::SceneTechniqueObservation;
    {
        Observation outer(runtime, {5}, command, {7});
        Observation::OnRendered(runtime, {6}, command, {7}, {7}); require(!outer.recorded);
        Observation::OnRendered(runtime, {5}, command, {8}, {8}); require(!outer.recorded);
        {
            Observation inner(runtime, {6}, command, {8});
            Observation::OnRendered(runtime, {6}, command, {8}, {8});
            require(inner.recorded && !outer.recorded);
        }
        std::thread other([&] { Observation::OnRendered(runtime, {5}, command, {7}, {7}); }); other.join();
        require(!outer.recorded);
        Observation::OnRendered(runtime, {5}, command, {7}, {7}); require(outer.recorded);
    }
    require(Observation::active == nullptr);
    { Observation queuedOnly(runtime, {5}, command, {7}); require(!queuedOnly.recorded); }
    std::puts("Scene-input allocation and technique-observation contract tests passed.");
}
