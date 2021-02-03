#include "pch.h"
#include "upload_manager.h"

#include "D3D12MemAlloc/D3D12MemAlloc.h"
void zec::gfx::dx12::UploadContextStore::destroy()
{
    for (auto& pair : upload_contexts) {
        for (Upload& upload : pair.second) {
            upload.resource->Release();
            if (upload.allocation) {
                upload.allocation->Release();
            }
        }
    }
    upload_contexts.clear();
}