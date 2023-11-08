#pragma once
#include <string>
#include <unordered_map>
#include "rhi.h"
#include "rhi_public_resources.h"
#include "core/zec_types.h"
#include "core/array.h"
#include "core/allocators.hpp"
#include "render_pass_resources.h"

namespace zec::render_graph
{
    struct PipelineCompilationDesc
    {
        std::wstring_view name;
        rhi::ResourceLayoutHandle resource_layout;
        rhi::ShaderCompilationDesc shader_compilation_desc;
        rhi::PipelineStateObjectDesc pso_desc;
    };

    template<typename TIdentifier, typename THandle>
    class Store
    {
    public:
        using const_iterator = std::unordered_map< typename TIdentifier, typename THandle >::const_iterator;

        THandle get(const TIdentifier id) const
        {
            return handles.at(id);
        };

        void set(const TIdentifier id, const THandle handle)
        {
            handles[id] = handle;
        };

        bool contains(const TIdentifier id) const
        {
            return handles.contains(id);
        };

        const_iterator begin() const noexcept
        {
            return handles.begin();
        }

        const_iterator end() const noexcept
        {
            return handles.end();
        }
    protected:
        std::unordered_map<TIdentifier, THandle> handles;
    };

    using ResourceLayoutStore = Store<ResourceLayoutId, rhi::ResourceLayoutHandle>;

    class PipelineStore
    {
    public:
        ZecResult compile(const PipelineId id, const PipelineCompilationDesc& desc, std::string& errors);
        ZecResult recompile(const PipelineId id, std::string& errors);

        rhi::PipelineStateHandle get_pipeline(const PipelineId id) const
        {
            return pso_handles.get(id);
        };

        rhi::ResourceLayoutHandle get_resource_layout(const PipelineId id) const
        {
            return recompilation_infos.get(id).resource_layout;
        };

        bool contains(const PipelineId id) const
        {
            return pso_handles.contains(id);
        };

        Store<PipelineId, PipelineCompilationDesc>::const_iterator begin() const noexcept
        {
            return recompilation_infos.begin();
        }

        Store<PipelineId, PipelineCompilationDesc>::const_iterator end() const noexcept
        {
            return recompilation_infos.end();
        }
    private:
        rhi::Renderer* prenderer;
        Store<PipelineId, rhi::PipelineStateHandle> pso_handles;
        Store<PipelineId, PipelineCompilationDesc> recompilation_infos;
    };
}