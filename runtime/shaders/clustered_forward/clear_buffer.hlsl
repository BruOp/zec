#pragma pack_matrix( row_major )

cbuffer buffer_info : register(b0)
{
    uint buffer_idx;
    uint buffer_byte_size;
    uint clear_value;
};

RWByteAddressBuffer write_buffers_table[4096] : register(u0, space1);

[numthreads(32, 1, 1)]
void CSMain(
    uint3 group_id : SV_GROUPID,
    uint3 group_thread_id : SV_GROUPTHREADID,
    uint3 dispatch_id : SV_DISPATCHTHREADID,
    uint group_idx : SV_GROUPINDEX
)
{
    RWByteAddressBuffer buffer = write_buffers_table[buffer_idx];

    if (dispatch_id.x * 4 > buffer_byte_size) {
        return;
    }
    
    buffer.Store(dispatch_id.x, clear_value);
}
