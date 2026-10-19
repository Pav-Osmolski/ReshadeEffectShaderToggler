#include "GlobalResourceView.h"
#include "RenderingSafety.h"

using namespace Rendering;
using namespace reshade::api;

GlobalResourceView::GlobalResourceView(reshade::api::device* d, reshade::api::resource r, reshade::api::format format) {
    device = d;
    resource_handle = r.handle;
    rtv = { 0 };
    rtv_srgb = { 0 };
    srv = { 0 };
    srv_srgb = { 0 };
    state = GlobalResourceState::RESOURCE_VALID;

    resource_desc desc = device->get_resource_desc(r);

    if ((static_cast<uint32_t>(desc.usage) & static_cast<uint32_t>(resource_usage::render_target) ||
         static_cast<uint32_t>(desc.usage) & static_cast<uint32_t>(resource_usage::shader_resource)) &&
        desc.type == resource_type::texture_2d) {
        reshade::api::format f = format == reshade::api::format::unknown ? desc.texture.format : format;

        const auto formats = GetColorViewFormats(device->get_api(), desc.texture.format, f);
        const auto format_non_srgb = formats.linear;
        const auto format_srgb = formats.srgb;

        if (static_cast<uint32_t>(desc.usage & resource_usage::render_target)) {
            device->create_resource_view(r, resource_usage::render_target, resource_view_desc(format_non_srgb), &rtv);
            if (format_srgb == format_non_srgb)
                rtv_srgb = rtv;
            else if (!device->create_resource_view(r, resource_usage::render_target, resource_view_desc(format_srgb), &rtv_srgb))
                rtv_srgb = rtv;
            // Some concretely typed sRGB images only accept their original view.
            if (rtv == 0)
                rtv = rtv_srgb;
        }

        if (static_cast<uint32_t>(desc.usage & resource_usage::shader_resource) && IsValidShaderResource(desc.texture.format)) {
            device->create_resource_view(r, resource_usage::shader_resource, resource_view_desc(format_non_srgb), &srv);
            if (format_srgb == format_non_srgb)
                srv_srgb = srv;
            else if (!device->create_resource_view(r, resource_usage::shader_resource, resource_view_desc(format_srgb), &srv_srgb))
                srv_srgb = srv;
            // Some concretely typed sRGB images only accept their original view.
            if (srv == 0)
                srv = srv_srgb;
        }
    }
}

GlobalResourceView::~GlobalResourceView() {
    Dispose();
}

inline bool GlobalResourceView::IsValidShaderResource(reshade::api::format format) {
    return format != reshade::api::format::intz;
}

void GlobalResourceView::Dispose(bool deviceValid) {
    if (device == nullptr) {
        return;
    }

    if (deviceValid) {
        if (rtv != 0) {
            device->destroy_resource_view(rtv);
        }

        if (rtv_srgb != 0 && rtv_srgb != rtv) {
            device->destroy_resource_view(rtv_srgb);
        }

        if (srv != 0) {
            device->destroy_resource_view(srv);
        }

        if (srv_srgb != 0 && srv_srgb != srv) {
            device->destroy_resource_view(srv_srgb);
        }
    }

    rtv = { 0 };
    rtv_srgb = { 0 };
    srv = { 0 };
    srv_srgb = { 0 };
}