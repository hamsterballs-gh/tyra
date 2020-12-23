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

u32 counter;

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
    float height = 480.0F;
    s32 depthBits = 24; // or 28(fog) or 16
    int maxDepthValue = (1 << depthBits) - 1;
    gsScale.setScale(Vector3(width / 2.0F, -1.0F * height / 2.0F, -1.0F * (float)maxDepthValue / 2.0F)); // Z nie ma żadnego wpływu
    ident.identity();
    xClip = (float)2048.0F / (width * .5F * 2.0F);
    yClip = (float)2048.0F / (height * .5F * 2.0F);
    // xClip = (float)2048.0F / (width * 2.0F);
    // yClip = (float)2048.0F / (height * 2.0F);
    depthClipToGs = (float)((1 << depthBits) - 1) / 2.0f;
    depthClip = 2048.0F / depthClipToGs;
    counter = 0;
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
        packet2_add_s32(settings, 0);                        // enable clipping
    }
    packet2_utils_vu_close_unpack(settings);
    packet2_utils_vu_add_double_buffer(settings, 10, 498); // give some space for static params
    packet2_utils_vu_add_end_tag(settings);
    dma_channel_send_packet2(settings, DMA_CHANNEL_VIF1, 1);
    dma_channel_wait(DMA_CHANNEL_VIF1, 0);
    packet2_free(settings);
}

float fixNear0(const float &t)
{
    float res = t;
    if (t >= -0.99F && t <= -0.00F)
        res -= 1.0F;
    else if (t >= 0.00F && t <= 0.99F)
        res += 1.0F;
    return res;
}

const Vector3 *testxd;

void VifSender::calcModelViewProjMatrix(const RenderData &t_renderData, const Vector3 &t_position, const Vector3 &t_rotation)
{
    model.identity();
    testxd = &t_position;
    // Very important... this prevents values to be near 0.0F, without it we will have visual artifacts
    // due to 0 division (infinite numbers)
    // Vector3 fixpos = Vector3(t_position);
    // fixpos.z = -fixpos.z;
    // translate.translation(fixpos);
    translate.translation(t_position);
    // translate.translation(Vector3(t_position.x + 0.1F, t_position.y + 0.1F, t_position.z + 0.1F));
    // translate.translation(Vector3(t_position.x + 0.9F, t_position.y + 0.9F, t_position.z + 0.9F));
    // translate.translation(Vector3(fixNear0(t_position.x), fixNear0(t_position.y), fixNear0(t_position.z)));
    model &= translate;

    // rotate.rotationX(t_rotation.x);
    // model &= rotate;

    // rotate.rotationY(t_rotation.y);
    // model &= rotate;

    // rotate.rotationZ(t_rotation.z);
    // model &= rotate;
    modelView = *t_renderData.worldView & model; // Bez kamery ten sam problem

    // Matrix projection = Matrix(1.923234, 0.000000, 0.000000, 0.000000,
    //                            0.000000, 2.747477, 0.000000, 0.000000,
    //                            0.000000, 0.000000, -1.000500, -1.000000,
    //                            0.000000, 0.000000, -2.000500, 0.000000);

    // NEW multiply!
    // Matrix projection = ident; // Bez identa to samo
    Matrix projection = Matrix(ident);
    projection = *t_renderData.perspective; // Bez identa ato samo
    // projection.data[0] = 2.0F;
    // projection.data[5] = 2.0F;
    // projection.data[10] = 0.499800F;
    // projection.data[11] = -1.0F;
    // projection.data[14] = -.200020F;
    // if (counter > 200)
    //     projection.print();

    // projection.data[0] = 2.747478F;
    // projection.data[5] = 2.747478F;
    // projection.data[10] = -1.002002F;
    // projection.data[11] = -1.0F;
    // projection.data[14] = -2.002002F;
    // projection.data[15] = 0.0F;

    // model = Matrix(0.707107, -0.408248, 0.577350, 0.000000,
    //                0.000000, 0.816497, 0.577350, 0.000000,
    //                -0.707107, -0.408248, 0.577350, 0.000000,
    //                0.000000, 0.000000, -86.602539, 1.000000);

    // Matrix 1:1 z playlua
    // Sprawdzić model jaki ma matrix
    // Spróbować znaleźć model * matrix?

    // modelViewProj = projection; // bedzie modelView
    modelViewProj = projection & model; // bedzie modelView

    // modelViewProj.print();
    // for (;;)
    // {
    // }
    // projection.print();
    // model.print();
    // (projection & model).print();
    // (model * projection).print();
    // for (;;)
    // {
    // }
    //modelViewProj = projection * modelView; // bedzie modelView

    // if (counter > 500)
    // {
    //     printf("modelViewProj3----------------\n");
    //     modelViewProj.print();
    //     printf("----------------\n");
    // }

    // GS Scale----------------
    // X:10.000000 Y:0.000000 Z:10.000000
    // MODEL: Vector3(8.500004, 3.499997, 40.099998)
    // PROJE: Vector3(-0.425205, -0.233445, 1.523993)
    // GS   : Vector3(1914.634399, 2107.793457, -4395570.500000)

    // X:-10.000000 Y:0.000000 Z:-10.000000
    // MODEL: Vector3(-11.499995, 3.499997, 20.099998)
    // PROJE: Vector3(1.147691, -0.465730, 2.025277)
    // GS   : Vector3(2417.961182, 2163.541504, -8600650.000000)

    // X:-10.000000 Y:0.000000 Z:10.000000
    // MODEL: Vector3(-11.499995, 3.499997, 40.099998)
    // PROJE: Vector3(0.575277, -0.233445, 1.523993)
    // GS   : Vector3(2234.788330, 2107.793457, -4395570.500000)

    // X:10.000000 Y:0.000000 Z:10.000000
    // MODEL: Vector3(8.500004, 3.499997, 40.099998)
    // PROJE: Vector3(-0.425205, -0.233445, 1.523993)
    // GS   : Vector3(1914.634399, 2107.793457, -4395570.500000)

    // X:10.000000 Y:0.000000 Z:-10.000000
    // MODEL: Vector3(8.500004, 3.499997, 20.099998)
    // PROJE: Vector3(-0.848294, -0.465730, 2.025277)
    // GS   : Vector3(1779.245728, 2163.541504, -8600650.000000)

    // X:-10.000000 Y:0.000000 Z:-10.000000
    // MODEL: Vector3(-11.499995, 3.499997, 20.099998)
    // PROJE: Vector3(1.147691, -0.465730, 2.025277)
    // GS   : Vector3(2417.961182, 2163.541504, -8600650.000000)

    // ----------------
    // projection.print(); // TODO 1
    // printf("---\n");
    // modelViewProj = Matrix(1.923228, 0.000000, 0.002619, 0.002618,
    //                        0.000000, 2.747477, 0.000000, 0.000000,
    //                        0.005035, 0.000000, -1.000497, -0.999997,
    //                        0.000000, 0.000000, 3.002001, 5.000000);
    // modelViewProj.print();
    // NEW multiply!
    // modelViewProj.print();
    gsModelViewProj = gsScale & modelViewProj; // Do sprawdzenia 4
    // Vector3 test = Vector3(t_position.x, t_position.y, t_position.z);
    // test.print();
    // modelViewProj.print(); // TODO 1
    // printf("---\n");
    // gsModelViewProj.print(); // TODO 2
    // for (;;)
    // {
    // }

    //    -----
    // Projection:
    // (1.923234 0.000000 0.000000 0.000000)
    // (0.000000 2.747478 0.000000 0.000000)
    // (0.000000 0.000000 -1.000500 -1.000000)
    // (0.000000 0.000000 -2.000500 0.000000)
    // ModelView:
    // (1.000000 0.000000 0.000000 0.000000)
    // (0.000000 1.000000 0.000000 0.000000)
    // (0.000000 0.000000 1.000000 0.000000)
    // (20.000000 10.000000 40.000000 1.000000)
    // Scale:
    // (320.000000 0.000000 0.000000 0.000000)
    // (0.000000 -112.000000 0.000000 0.000000)
    // (0.000000 0.000000 -8388607.500000 0.000000)
    // (0.000000 0.000000 0.000000 1.000000)
    // XFORM:
    // (1.923234 0.000000 0.000000 0.000000)
    // (0.000000 2.747478 0.000000 0.000000)
    // (0.000000 0.000000 -1.000500 -1.000000)
    // (38.464684 27.474775 -42.020500 -40.000000)
    // Result:
    // (615.434937 0.000000 0.000000 0.000000)
    // (0.000000 -307.717468 0.000000 0.000000)
    // (0.000000 0.000000 8392802.000000 -1.000000)
    // (12308.698242 -3077.174805 352493472.000000 -40.000000)
    // -----

    // if (counter > 500)
    // {
    //     printf("modelViewProj4----------------\n");
    //     gsModelViewProj.print();
    //     printf("----------------\n");
    // }
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

// Xform + perspective divide from VU1
void testmul(VECTOR output, VECTOR vertex, Matrix modelViewProjx)
{
    asm __volatile__(
        "lqc2		vf1, 0x00(%2)	\n"
        "lqc2		vf2, 0x10(%2)	\n"
        "lqc2		vf3, 0x20(%2)	\n"
        "lqc2		vf4, 0x30(%2)	\n"
        "1:					\n"
        "lqc2		vf9, 0x00(%1)	\n"
        "vmulax		ACC,  vf1, vf9x	\n"
        "vmadday	ACC,  vf2, vf9y	\n"
        "vmaddaz	ACC,  vf3, vf9z	\n"
        "vmaddw		vf12, vf4, vf9w	\n"
        //"vclipw.xyz		vf12,vf12	\n"

        "sqc2		vf12, 0x00(%0)	\n"
        :
        : "r"(output), "r"(vertex), "r"(modelViewProjx.data)
        : "memory");
}

// Xform + perspective divide from VU1
void test(VECTOR output, VECTOR vertex, Matrix modelViewProjx)
{
    asm __volatile__(
        "lqc2		vf1, 0x00(%2)	\n"
        "lqc2		vf2, 0x10(%2)	\n"
        "lqc2		vf3, 0x20(%2)	\n"
        "lqc2		vf4, 0x30(%2)	\n"
        "1:					\n"
        "lqc2		vf9, 0x00(%1)	\n"
        "vmulax		ACC,  vf1, vf9x	\n"
        "vmadday	ACC,  vf2, vf9y	\n"
        "vmaddaz	ACC,  vf3, vf9z	\n"
        "vmaddw		vf12, vf4, vf9w	\n"
        //"vclipw.xyz		vf12,vf12	\n"

        "vdiv		Q,   vf0w, vf12w	\n"
        "vwaitq				\n"
        "vmulq.xyz		vf8, vf12, Q	\n"

        "sqc2		vf8, 0x00(%0)	\n"
        :
        : "r"(output), "r"(vertex), "r"(modelViewProjx.data)
        : "memory");

    // asm __volatile__(
    //     "lqc2		vf1, 0x00(%2)	\n"
    //     "lqc2		vf2, 0x10(%2)	\n"
    //     "lqc2		vf3, 0x20(%2)	\n"
    //     "lqc2		vf4, 0x30(%2)	\n"
    //     "1:					\n"
    //     "lqc2		vf6, 0x00(%1)	\n"
    //     "vmulaw		ACC, vf4, vf0	\n"
    //     "vmaddax		ACC, vf1, vf6	\n"
    //     "vmadday		ACC, vf2, vf6	\n"
    //     "vmaddz		vf7, vf3, vf6	\n"

    //     "vdiv		Q, vf0w, vf7w	\n"
    //     "vwaitq				\n"
    //     "vmulq.xyz		vf7, vf7, Q	\n"

    //     "sqc2		vf7, 0x00(%0)	\n"
    //     :
    //     : "r"(output), "r"(vertex), "r"(modelViewProjx.data)
    //     : "memory");
}

/** Draw using PATH1 */
void VifSender::drawVertices(Mesh &t_mesh, u32 t_start, u32 t_end, VECTOR *t_vertices, VECTOR *t_coordinates, prim_t *t_prim, texbuffer_t *textureBuffer)
{
    const u32 vertCount = t_end - t_start;
    u32 vif_added_bytes = 0;
    packet2_utils_vu_open_unpack(currPacket, 0, true);
    {
        packet2_add_data(currPacket, gsModelViewProj.data, 4);
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

    // 1. Sprawdzić czy minus też jest na matrycach w OpenGL
    // 2. Sprawdzić różnicę między mnożeniami

    if (counter++ > 200)
    {
        counter = 0;
        printf("\n\n6 VERTS (2 TRIS)----------------\n");
        // translate.print();
        // printf("---\n");6
        for (size_t i = 0; i < vertCount; i++)
        {
            VECTOR output;

            // 1 - Vec*Model
            // 2 - Vec*(Proj*Model)

            // printf("X:%f Y:%f Z:%f\n", t_vertices[i][0], t_vertices[i][1], t_vertices[i][2]);
            test(output, t_vertices[i], model);
            printf("1(X:%f Y:%f, Z:%f)\n", output[0], output[1], output[2]);
            test(output, t_vertices[i], modelViewProj);
            printf("2(X:%f Y:%f)\n", output[0], output[1]);
            test(output, t_vertices[i], gsModelViewProj);
            Vector3 xd3 = Vector3(output[0], output[1], output[2]);
            // testmul(output, t_vertices[i], gsModelViewProj);
            // printf("MUL(X:%f Y:%f Z:%f W:%f)\n", output[0], output[1], output[2], output[3]);

            Vector3 gsOffsets = Vector3(Math::max(xClip, 1.0F), Math::max(yClip, 1.0F), depthClip * 1.003F);
            gsOffsets.x += 2047.5F;
            gsOffsets.y += 2047.5F;
            gsOffsets.z = depthClipToGs;

            // printf("GSMVP: ");
            Vector3 beb = xd3 + gsOffsets;
            printf("3(X:%f Y:%f)\n", beb.x, beb.y);
            // (xd3 + gsOffsets).print(); // 1:1 with VU1
            printf("\n");
        }
        printf("----------------\n");
    }

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
//         Matrix projection = ident & *t_renderData.perspective;
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
//         Matrix projection = ident & *t_renderData.perspective;
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
