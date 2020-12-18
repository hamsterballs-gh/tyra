/*
# ______       ____   ___
#   |     \/   ____| |___|    
#   |     |   |   \  |   |       
#-----------------------------------------------------------------------
# Copyright 2020, tyra - https://github.com/h4570/tyra
# Licenced under Apache License 2.0
# Sandro Sobczyński <sandro.sobczynski@gmail.com>
*/

#include "../include/modules/vif_sender.hpp"

#include <gs_gp.h>
#include <dma.h>
#include <gif_tags.h>
#include "../include/utils/math.hpp"
#include "../include/utils/debug.hpp"

const u32 VU1_PACKAGE_VERTS_PER_BUFF = 96; // Remember to modify buffer size in vu1 also
const u32 VU1_PACKAGES_PER_PACKET = 9;
const u32 VU1_PACKET_SIZE = 256; // should be 128, but 256 is more safe for future

// ----
// Constructors/Destructors
// ----

// VU1 micro program
extern u32 VU1Draw3D_CodeStart __attribute__((section(".vudata")));
extern u32 VU1Draw3D_CodeEnd __attribute__((section(".vudata")));
//

VifSender::VifSender(Light *t_light)
{
    light = t_light;
    PRINT_LOG("Initializing VifSender");
    PRINT_LOG("VifSender initialized!");
    dma_channel_initialize(DMA_CHANNEL_VIF1, NULL, 0);
    dma_channel_fast_waits(DMA_CHANNEL_VIF1);
    uploadMicroProgram();
    packets[0] = packet2_create(VU1_PACKET_SIZE, P2_TYPE_NORMAL, P2_MODE_CHAIN, true);
    packets[1] = packet2_create(VU1_PACKET_SIZE, P2_TYPE_NORMAL, P2_MODE_CHAIN, true);
    context = 0;

    gsScale.identity();
    float width = 640.0F;
    float height = 224.0F;
    s32 depthBits = 24; // or 28(fog) or 16
    int maxDepthValue = (1 << depthBits) - 1;
    gsScale.setScale(Vector3(width / 2.0F, -1 * height / 2.0F, -1 * (float)maxDepthValue / 2.0F));
    ident.identity();
    xClip = (float)2048.0F / (width * 2.0F);
    yClip = (float)2048.0F / (height * 2.0F);
    depthClipToGs = (float)((1 << depthBits) - 1) / 2.0f;
    depthClip = 2048.0F / depthClipToGs;
    setDoubleBufferAndClip();
}

VifSender::~VifSender()
{
    packet2_free(packets[0]);
    packet2_free(packets[1]);
}

// ----
// Methods
// ----

void VifSender::uploadMicroProgram()
{
    packet2_t *packet2 = packet2_create(
        packet2_utils_get_packet_size_for_program(&VU1Draw3D_CodeStart, &VU1Draw3D_CodeEnd) + 1, // + end tag
        P2_TYPE_NORMAL,
        P2_MODE_CHAIN,
        1);
    packet2_vif_add_micro_program(packet2, 0, &VU1Draw3D_CodeStart, &VU1Draw3D_CodeEnd);
    packet2_utils_vu_add_end_tag(packet2);
    dma_channel_wait(DMA_CHANNEL_VIF1, 0);
    dma_channel_send_packet2(packet2, DMA_CHANNEL_VIF1, 1);
    packet2_free(packet2);
}

void VifSender::setDoubleBufferAndClip()
{
    packet2_t *settings = packet2_create(8, P2_TYPE_NORMAL, P2_MODE_CHAIN, true);
    packet2_utils_vu_open_unpack(settings, 0, false);
    {
        packet2_add_float(settings, Math::max(xClip, 1.0F)); // x clip
        packet2_add_float(settings, Math::max(yClip, 1.0F)); // y clip
        packet2_add_float(settings, depthClip * 1.003F);     // depth clip
        packet2_add_s32(settings, 1);                        // enable clipping
    }
    packet2_utils_vu_close_unpack(settings);
    packet2_utils_vu_add_double_buffer(settings, 10, 498); // give some space for static params
    packet2_utils_vu_add_end_tag(settings);
    dma_channel_send_packet2(settings, DMA_CHANNEL_VIF1, 1);
    dma_channel_wait(DMA_CHANNEL_VIF1, 0);
    packet2_free(settings);
}

void VifSender::calcModelViewProjMatrix(const RenderData &t_renderData, const Vector3 &t_position, const Vector3 &t_rotation)
{
    modelViewProj.identity();

    translate.translation(t_position);
    modelViewProj &= translate;

    rotate.rotationX(t_rotation.x);
    modelViewProj &= rotate;

    rotate.rotationY(t_rotation.y);
    modelViewProj &= rotate;

    rotate.rotationZ(t_rotation.z);
    modelViewProj &= rotate;

    modelViewProj = *t_renderData.worldView & modelViewProj;

    // NEW multiply!
    Matrix projection = ident & *t_renderData.perspective;
    modelViewProj = projection & modelViewProj; // CHECKED

    // NEW multiply!
    modelViewProj = gsScale & modelViewProj;
}

void VifSender::drawMesh(RenderData *t_renderData, Matrix t_perspective, u32 vertCount2, VECTOR *vertices, VECTOR *normals, VECTOR *coordinates, Mesh &t_mesh, LightBulb *t_bulbs, u16 t_bulbsCount, texbuffer_t *textureBuffer)
{
    // we have to split 3D object into small parts, because of small memory of VU1

    for (u32 i = 0; i < vertCount2;)
    {
        currPacket = packets[context];
        packet2_reset(currPacket, false);
        for (u8 j = 0; j < VU1_PACKAGES_PER_PACKET; j++) // how many "packages" per one packet
        {
            if (i != 0) // we have to go back to avoid the visual artifacts
                i -= 3;

            const u32 endI = i + (VU1_PACKAGE_VERTS_PER_BUFF - 1) > vertCount2 ? vertCount2 : i + (VU1_PACKAGE_VERTS_PER_BUFF - 1);
            drawVertices(t_mesh, i, endI, vertices, coordinates, t_renderData->prim, textureBuffer);
            if (endI == vertCount2) // if there are no more vertices to draw, break
            {
                i = vertCount2;
                break;
            }
            i += (VU1_PACKAGE_VERTS_PER_BUFF - 1);
            i++;
        }
        packet2_utils_vu_add_end_tag(currPacket);
        dma_channel_send_packet2(currPacket, DMA_CHANNEL_VIF1, 1);
        dma_channel_wait(DMA_CHANNEL_VIF1, 0);
        context = !context;
    }
}

/** Draw using PATH1 */
void VifSender::drawVertices(Mesh &t_mesh, u32 t_start, u32 t_end, VECTOR *t_vertices, VECTOR *t_coordinates, prim_t *t_prim, texbuffer_t *textureBuffer)
{
    const u32 vertCount = t_end - t_start;
    u32 vif_added_bytes = 0;
    packet2_utils_vu_open_unpack(currPacket, 0, true);
    {
        packet2_add_data(currPacket, modelViewProj.data, 4);
        packet2_add_s32(currPacket, 0); // not used for now
        packet2_add_s32(currPacket, vertCount);
        packet2_add_s32(currPacket, vertCount / 3);
        packet2_add_float(currPacket, depthClipToGs);
        packet2_utils_gif_add_set(currPacket, 1);
        packet2_utils_gs_add_lod(currPacket, &t_mesh.lod);
        packet2_utils_gs_add_texbuff_clut(currPacket, textureBuffer, &t_mesh.clut);
        packet2_utils_gs_add_prim_giftag(currPacket, t_prim, vertCount, DRAW_STQ2_REGLIST, 3, 0);

        packet2_add_u32(currPacket, t_mesh.color.r);
        packet2_add_u32(currPacket, t_mesh.color.g);
        packet2_add_u32(currPacket, t_mesh.color.b);
        packet2_add_u32(currPacket, t_mesh.color.a);
    }
    vif_added_bytes += packet2_utils_vu_close_unpack(currPacket);
    packet2_utils_vu_add_unpack_data(currPacket, vif_added_bytes, t_vertices + t_start, vertCount, true);
    vif_added_bytes += vertCount;
    packet2_utils_vu_add_unpack_data(currPacket, vif_added_bytes, t_coordinates + t_start, vertCount, true);
    vif_added_bytes += vertCount;
    packet2_utils_vu_add_start_program(currPacket, 0);
}

// void VifSender::drawTheSameWithOtherMatrices(const RenderData &t_renderData, Mesh *t_meshes, const u32 &t_skip, const u32 &t_count)
// {
//     // DUPLICATE!!!!!!!!!!!!
//     packet2_t *packet2 = packet2_create(300, P2_TYPE_NORMAL, P2_MODE_CHAIN, true);
//     u8 switchCounter = 0;
//     for (u32 i = t_skip; i < t_count; i++)
//     {
//         Matrix modelView = Matrix();

//         translate.translation(t_meshes[i].position);
//         modelView.identity();
//         modelView = modelView & translate;

//         rotate.rotationX(t_meshes[i].rotation.x);
//         modelView &= rotate;

//         rotate.rotationY(t_meshes[i].rotation.y);
//         modelView &= rotate;

//         rotate.rotationZ(t_meshes[i].rotation.z);
//         modelView &= rotate;

//         modelView = *t_renderData.worldView & modelView;

//         // NEW multiply!
//         Matrix projection = ident & frustum;
//         Matrix viewProj = projection & modelView;

//         // NEW multiply!
//         viewProj = gsScale & viewProj;
//         packet2_utils_vu_open_unpack(packet2, 0, true);
//         {
//             packet2_add_data(packet2, viewProj.data, 4);
//         }
//         packet2_utils_vu_close_unpack(packet2);
//         packet2_utils_vu_open_unpack(packet2, 9, true);
//         {
//             packet2_add_u32(packet2, t_meshes[i].color.r);
//             packet2_add_u32(packet2, t_meshes[i].color.g);
//             packet2_add_u32(packet2, t_meshes[i].color.b);
//             packet2_add_u32(packet2, t_meshes[i].color.a);
//         }
//         packet2_utils_vu_close_unpack(packet2);
//         packet2_utils_vu_add_start_program(packet2, 0);

//         if (switchCounter++ >= 32)
//         {
//             switchCounter = 0;
//             packet2_utils_vu_add_end_tag(packet2);
//             dma_channel_wait(DMA_CHANNEL_VIF1, 0);
//             dma_channel_send_packet2(packet2, DMA_CHANNEL_VIF1, 1);
//             packet2_reset(packet2, false);
//         }
//     }
//     if (switchCounter != 0)
//     {
//         packet2_utils_vu_add_end_tag(packet2);
//         dma_channel_wait(DMA_CHANNEL_VIF1, 0);
//         dma_channel_send_packet2(packet2, DMA_CHANNEL_VIF1, 1);
//     }
//     packet2_free(packet2);
// }

// void VifSender::drawTheSameWithOtherMatrices(const RenderData &t_renderData, Mesh **t_meshes, const u32 &t_skip, const u32 &t_count)
// {
//     // DUPLICATE!!!!!!!!!!!!
//     packet2_t *packet2 = packet2_create(300, P2_TYPE_NORMAL, P2_MODE_CHAIN, true);
//     u8 switchCounter = 0;
//     for (u32 i = t_skip; i < t_count; i++)
//     {
//         Matrix modelView = Matrix();

//         translate.translation(t_meshes[i]->position);
//         modelView.identity();
//         modelView = modelView & translate;

//         rotate.rotationX(t_meshes[i]->rotation.x);
//         modelView &= rotate;

//         rotate.rotationY(t_meshes[i]->rotation.y);
//         modelView &= rotate;

//         rotate.rotationZ(t_meshes[i]->rotation.z);
//         modelView &= rotate;

//         modelView = *t_renderData.worldView & modelView;

//         // NEW multiply!
//         Matrix projection = ident & frustum;
//         Matrix viewProj = projection & modelView;

//         // NEW multiply!
//         viewProj = gsScale & viewProj;
//         packet2_utils_vu_open_unpack(packet2, 0, true);
//         {
//             packet2_add_data(packet2, viewProj.data, 4);
//         }
//         packet2_utils_vu_close_unpack(packet2);
//         packet2_utils_vu_open_unpack(packet2, 9, true);
//         {
//             packet2_add_u32(packet2, t_meshes[i]->color.r);
//             packet2_add_u32(packet2, t_meshes[i]->color.g);
//             packet2_add_u32(packet2, t_meshes[i]->color.b);
//             packet2_add_u32(packet2, t_meshes[i]->color.a);
//         }
//         packet2_utils_vu_close_unpack(packet2);
//         packet2_utils_vu_add_start_program(packet2, 0);

//         if (switchCounter++ >= 32)
//         {
//             switchCounter = 0;
//             packet2_utils_vu_add_end_tag(packet2);
//             dma_channel_wait(DMA_CHANNEL_VIF1, 0);
//             dma_channel_send_packet2(packet2, DMA_CHANNEL_VIF1, 1);
//             packet2_reset(packet2, false);
//         }
//     }
//     if (switchCounter != 0)
//     {
//         packet2_utils_vu_add_end_tag(packet2);
//         dma_channel_wait(DMA_CHANNEL_VIF1, 0);
//         dma_channel_send_packet2(packet2, DMA_CHANNEL_VIF1, 1);
//     }
//     packet2_free(packet2);
// }
