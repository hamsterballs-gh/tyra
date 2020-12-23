/*
# ______       ____   ___
#   |     \/   ____| |___|    
#   |     |   |   \  |   |       
#-----------------------------------------------------------------------
# Copyright 2020, tyra - https://github.com/h4570/tyra
# Licenced under Apache License 2.0
# Sandro Sobczyński <sandro.sobczynski@gmail.com>
*/

#include "../include/modules/renderer.hpp"

#include <dma.h>
#include <graph.h>
#include <packet.h>
#include <draw.h>
#include <gs_psm.h>
#include "../include/utils/debug.hpp"
#include "../include/utils/math.hpp"

// ----
// Constructors/Destructors
// ----

/** Initialize DMA<->GIF channel
 * Allocate buffers
 * Initialize screen
 * Initialize drawing environment
 * Load/setup textures
 * @param screenW Half of screen width
 * @param screenH Half of screen height
 */
Renderer::Renderer(u32 t_packetSize, ScreenSettings *t_screen)
{
    PRINT_LOG("Initializing renderer");
    screen = t_screen;
    dma_channel_initialize(DMA_CHANNEL_GIF, NULL, 0); // Initialize DMA to enable data transfer
    dma_channel_fast_waits(DMA_CHANNEL_GIF);
    context = 0;
    isTextureVRAMAllocated = false;
    isVSyncEnabled = true;
    isFrameEmpty = false;
    lastTextureId = 0;
    flipPacket = packet2_create(4, P2_TYPE_UNCACHED_ACCL, P2_MODE_NORMAL, 0);
    allocateBuffers();
    initDrawingEnv();
    setPrim();
    worldColor.r = 0x10;
    worldColor.g = 0x10;
    worldColor.b = 0x10;
    areBuffersAllocated = false;
    gifSender = new GifSender(t_packetSize, t_screen, &light);
    vifSender = new VifSender(&light);
    perspective.setFrustum(*t_screen);
    renderData.perspective = &perspective;
    PRINT_LOG("Renderer initialized!");
}

Renderer::~Renderer() {}

// ----
// Methods
// ----

/** Configure and allocate vRAM for texture buffer */
void Renderer::allocateTextureBuffer(Texture *t_texture)
{
    textureBuffer.width = t_texture->getWidth();
    textureBuffer.psm = t_texture->getType();
    textureBuffer.info.components = textureBuffer.psm == TEX_TYPE_RGBA ? TEXTURE_COMPONENTS_RGBA : TEXTURE_COMPONENTS_RGB;
    textureBuffer.address = graph_vram_allocate(t_texture->getWidth(), t_texture->getHeight(), textureBuffer.psm, GRAPH_ALIGN_BLOCK);
    if (textureBuffer.address <= 1)
        PRINT_ERR("Texture buffer allocation error. No memory!");
    textureBuffer.info.width = draw_log2(t_texture->getWidth());
    textureBuffer.info.height = draw_log2(t_texture->getHeight());
    textureBuffer.info.function = TEXTURE_FUNCTION_MODULATE;
    isTextureVRAMAllocated = true;
}

/** Configure and allocate vRAM for texture buffer */
void Renderer::deallocateTextureBuffer()
{
    if (isTextureVRAMAllocated)
    {
        graph_vram_free(textureBuffer.address);
        isTextureVRAMAllocated = false;
    }
}

void Renderer::changeTexture(Texture *t_tex)
{
    if (t_tex != NULL)
    {
        if (t_tex->getId() != lastTextureId)
        {
            lastTextureId = t_tex->getId();
            deallocateTextureBuffer();
            allocateTextureBuffer(t_tex);
            GifSender::sendTexture(*t_tex, &textureBuffer);
        }
    }
    else
        PRINT_ERR("Texture was not found in texture repository!");
}

void Renderer::draw(Sprite &t_sprite)
{
    Texture *texture = textureRepo.getBySprite(t_sprite.getId());
    texrect_t rect;
    float sizeX, sizeY;
    if (t_sprite.getMode() == MODE_REPEAT)
    {
        sizeX = t_sprite.size.x;
        sizeY = t_sprite.size.y;
    }
    else
    {
        sizeX = (float)texture->getWidth();
        sizeY = (float)texture->getHeight();
    }

    float texS, texT;
    float texMax = texT = texS = sizeX > sizeY ? sizeX : sizeY;
    if (sizeX > sizeY)
        texT = texMax / (sizeX / sizeY);
    else if (sizeY > sizeX)
        texS = texMax / (sizeY / sizeX);
    rect.t0.s = t_sprite.isFlippedHorizontally() ? texS : 0.0F;
    rect.t0.t = t_sprite.isFlippedVertically() ? texT : 0.0F;
    rect.t1.s = t_sprite.isFlippedHorizontally() ? 0.0F : texS;
    rect.t1.t = t_sprite.isFlippedVertically() ? 0.0F : texT;
    rect.color.r = t_sprite.color.r;
    rect.color.g = t_sprite.color.g;
    rect.color.b = t_sprite.color.b;
    rect.color.a = t_sprite.color.a;
    rect.color.q = 0;
    rect.v0.x = t_sprite.position.x;
    rect.v0.y = t_sprite.position.y / 2.0F; // interlacing
    rect.v0.z = (u32)-1;
    rect.v1.x = (t_sprite.size.x * t_sprite.scale) + t_sprite.position.x;
    rect.v1.y = ((t_sprite.size.y * t_sprite.scale) + t_sprite.position.y) / 2.0F; // interlacing
    rect.v1.z = (u32)-1;
    beginFrameIfNeeded();
    changeTexture(texture);
    packet2_t *packet2 = packet2_create(12, P2_TYPE_NORMAL, P2_MODE_NORMAL, 0);
    packet2_update(packet2, draw_primitive_xyoffset(packet2->next, 0, 2048, 2048));
    packet2_utils_gif_add_set(packet2, 1);
    packet2_utils_gs_add_texbuff_clut(packet2, &textureBuffer, &t_sprite.clut);
    draw_enable_blending();
    packet2_update(packet2, draw_rect_textured(packet2->next, 0, &rect));
    float gsCenter = 4096.0F / 2.0F;
    float gsOffsetX = gsCenter - frameBuffers[0].width / 2.0F;
    float gsOffsetY = gsCenter - frameBuffers[0].height / 2.0F;
    packet2_update(packet2, draw_primitive_xyoffset(packet2->next, 0, gsOffsetX, gsOffsetY));
    draw_disable_blending();
    packet2_update(packet2, draw_finish(packet2->next));
    dma_channel_wait(DMA_CHANNEL_GIF, 0);
    dma_channel_send_packet2(packet2, DMA_CHANNEL_GIF, true);
    packet2_free(packet2);
}

/** Initializes drawing environment (1st app packet) */
void Renderer::initDrawingEnv()
{
    if (!areBuffersAllocated)
        PRINT_ERR("Buffers not initialized!");
    PRINT_LOG("Initializing drawing environment");
    float gsCenter = 4096.0F / 2.0F;
    float gsOffsetX = gsCenter - (frameBuffers[0].width / 2.0F);
    float gsOffsetY = gsCenter - (frameBuffers[0].height / 2.0F);
    // printf("----------\n");
    // printf("Offset[Y]:%f\n", gsOffsetY);
    // printf("----------\n");
    // gsOffsetY = 1836.0F;
    packet2_t *packet2 = packet2_create(20, P2_TYPE_NORMAL, P2_MODE_NORMAL, 0);
    packet2_update(packet2, draw_setup_environment(packet2->base, 0, frameBuffers, &(zBuffer))); // OK gsOffsetX.cpp:220
    packet2_update(packet2, draw_primitive_xyoffset(packet2->next, 0, gsOffsetX, gsOffsetY));    // OK gsOffsetX.cpp:159
    packet2_update(packet2, draw_finish(packet2->next));
    // packet2_update(packet2, draw_scissor_area(packet2->next, 0, 0, frameBuffers[0].width - 1, 0, frameBuffers[0].height - 1));
    dma_channel_send_packet2(packet2, DMA_CHANNEL_GIF, true);
    dma_channel_wait(DMA_CHANNEL_GIF, 0);
    packet2_free(packet2);
    PRINT_LOG("Drawing environment initialized!");
}

/** Sets drawing prim for all 3D objects */
void Renderer::setPrim()
{
    prim.type = PRIM_TRIANGLE;
    prim.shading = PRIM_SHADE_FLAT;
    prim.mapping = DRAW_ENABLE;
    prim.fogging = DRAW_DISABLE;
    prim.blending = DRAW_ENABLE;
    prim.antialiasing = DRAW_DISABLE;
    prim.mapping_type = PRIM_MAP_ST;
    prim.colorfix = PRIM_UNFIXED;
    renderData.prim = &prim;
    PRINT_LOG("Prim set!");
}

void Renderer::setWorldColor(const color_t &t_rgb)
{
    worldColor.r = t_rgb.r;
    worldColor.g = t_rgb.g;
    worldColor.b = t_rgb.b;
}

/** Defines and allocates framebuffers and zbuffer */
void Renderer::allocateBuffers()
{
    frameBuffers[0].width = (s32)screen->width;   // 640
    frameBuffers[0].height = (s32)screen->height; // interlacing
    frameBuffers[0].mask = 0;
    frameBuffers[0].psm = GS_PSM_32; // 32
    frameBuffers[0].address = graph_vram_allocate(frameBuffers[0].width, frameBuffers[0].height, frameBuffers[0].psm, GRAPH_ALIGN_PAGE);

    frameBuffers[1].width = (s32)screen->width;
    frameBuffers[1].height = (s32)screen->height; // interlacing
    frameBuffers[1].mask = 0;
    frameBuffers[1].psm = GS_PSM_32; // 32
    frameBuffers[1].address = graph_vram_allocate(frameBuffers[1].width, frameBuffers[1].height, frameBuffers[1].psm, GRAPH_ALIGN_PAGE);

    zBuffer.enable = DRAW_ENABLE;
    zBuffer.mask = 0;
    zBuffer.method = ZTEST_METHOD_GREATER_EQUAL; // Greater equal
    zBuffer.zsm = GS_ZBUF_24;                    // 24
    zBuffer.address = graph_vram_allocate(frameBuffers[0].width, frameBuffers[0].height, zBuffer.zsm, GRAPH_ALIGN_PAGE);
    PRINT_LOG("Framebuffers, zBuffer set and allocated!");

    graph_set_mode(GRAPH_MODE_INTERLACED, graph_get_region(), GRAPH_MODE_FIELD, GRAPH_ENABLE);
    graph_set_screen(0, 0, (s32)screen->width, (s32)screen->height); // manual interlacing
    graph_set_bgcolor(0, 0, 0);
    graph_set_framebuffer_filtered(frameBuffers[0].address, (s32)screen->width, frameBuffers[0].psm, 0, 0);
    graph_enable_output();

    areBuffersAllocated = true;
}

/// --- Draw: PATH3

void Renderer::drawByPath3(Mesh *t_meshes, u16 t_amount, LightBulb *t_bulbs, u16 t_bulbsCount)
{
    for (u16 i = 0; i < t_amount; i++)
        drawByPath3(t_meshes[i], t_bulbs, t_bulbsCount);
}

void Renderer::drawByPath3(Mesh &t_mesh, LightBulb *t_bulbs, u16 t_bulbsCount)
{
    beginFrameIfNeeded();
    if (!t_mesh.isDataLoaded())
        PRINT_ERR("Can't draw, because no mesh data was loaded!");
    if (t_mesh.getCurrentAnimationFrame() != t_mesh.getNextAnimationFrame())
        t_mesh.animate();
    for (u32 i = 0; i < t_mesh.getMaterialsCount(); i++)
    {
        if (t_mesh.shouldBeFrustumCulled && !t_mesh.getMaterial(i).isInFrustum(renderData.frustumPlanes, t_mesh.position))
            return;
        Texture *tex = textureRepo.getByMesh(t_mesh.getId(), t_mesh.getMaterial(i).getId());
        changeTexture(tex);
        gifSender->initPacket(context);
        u32 vertCount = t_mesh.getMaterial(i).getFacesCount();
        VECTOR *vertices = new VECTOR[vertCount];
        VECTOR *normals = new VECTOR[vertCount];
        VECTOR *coordinates = new VECTOR[vertCount];
        vertCount = t_mesh.getDrawData(i, vertices, normals, coordinates, *renderData.cameraPosition);
        gifSender->addObject(&renderData, t_mesh, vertCount, vertices, normals, coordinates, t_bulbs, t_bulbsCount, &textureBuffer);
        gifSender->sendPacket();
        delete[] vertices;
        delete[] normals;
        delete[] coordinates;
    }
}

void Renderer::drawByPath3(Mesh *t_meshes, u16 t_amount) { drawByPath3(t_meshes, t_amount, NULL, 0); }

void Renderer::drawByPath3(Mesh &t_mesh) { drawByPath3(t_mesh, NULL, 0); }

/// --- Draw: PATH1

// void Renderer::draw(Mesh *t_meshes, u16 t_amount, LightBulb *t_bulbs, u16 t_bulbsCount)
// {
//     // duplicate!!!!!!!!!
//     // do the same for 1!
//     beginFrameIfNeeded();
//     if (!t_meshes[0].isDataLoaded())
//         PRINT_ERR("Can't draw, because no mesh data was loaded!");
//     if (t_amount >= 1)
//         draw(t_meshes[0], t_bulbs, t_bulbsCount);
//     if (t_amount >= 2)
//         draw(t_meshes[1], t_bulbs, t_bulbsCount);
//     if (
//         t_amount >= 3 &&
//         !t_meshes[0].shouldBeBackfaceCulled &&
//         t_meshes[0].getFramesCount() == 1 &&
//         t_meshes[0].getMaterialsCount() == 1 &&
//         t_meshes[0].getFrame(0).getVertexCount() <= 96)

//         vifSender->drawTheSameWithOtherMatrices(renderData, t_meshes, 2, t_amount);
//     else
//         for (u16 i = 2; i < t_amount; i++)
//             draw(t_meshes[i], t_bulbs, t_bulbsCount);
// }

// void Renderer::draw(Mesh **t_meshes, u16 t_amount, LightBulb *t_bulbs, u16 t_bulbsCount)
// {
//     beginFrameIfNeeded();
//     if (!t_meshes[0]->isDataLoaded())
//         PRINT_ERR("Can't draw, because no mesh data was loaded!");
//     if (t_amount >= 1)
//         draw(*t_meshes[0], t_bulbs, t_bulbsCount);
//     if (t_amount >= 2)
//         draw(*t_meshes[1], t_bulbs, t_bulbsCount);
//     if (
//         t_amount >= 3 &&
//         !t_meshes[0]->shouldBeBackfaceCulled &&
//         t_meshes[0]->getFramesCount() == 1 &&
//         t_meshes[0]->getMaterialsCount() == 1 &&
//         t_meshes[0]->getFrame(0).getVertexCount() <= 96)

//         vifSender->drawTheSameWithOtherMatrices(renderData, t_meshes, 2, t_amount);
//     else
//         for (u16 i = 2; i < t_amount; i++)
//             draw(*t_meshes[i], t_bulbs, t_bulbsCount);
// }

void Renderer::draw(Mesh &t_mesh, LightBulb *t_bulbs, u16 t_bulbsCount)
{
    beginFrameIfNeeded();
    if (!t_mesh.isDataLoaded())
        PRINT_ERR("Can't draw, because no mesh data was loaded!");
    vifSender->calcModelViewProjMatrix(renderData, t_mesh.position, t_mesh.rotation);

    Vector3 rotatedCamera = Vector3(*renderData.cameraPosition);
    rotatedCamera.rotate(t_mesh.rotation, true);

    if (t_mesh.getCurrentAnimationFrame() != t_mesh.getNextAnimationFrame())
        t_mesh.animate();
    for (u32 i = 0; i < t_mesh.getMaterialsCount(); i++)
    {
        if (t_mesh.shouldBeFrustumCulled && !t_mesh.getMaterial(i).isInFrustum(renderData.frustumPlanes, t_mesh.position))
            return;
        u32 vertCount = t_mesh.getMaterial(i).getFacesCount();
        VECTOR __attribute__((aligned(16))) vertices[vertCount];
        VECTOR __attribute__((aligned(16))) normals[vertCount];
        VECTOR __attribute__((aligned(16))) coordinates[vertCount];
        Texture *tex = textureRepo.getByMesh(t_mesh.getId(), t_mesh.getMaterial(i).getId());
        changeTexture(tex);
        vertCount = t_mesh.getDrawData(i, vertices, normals, coordinates, rotatedCamera);

        vifSender->drawMesh(&renderData, perspective, vertCount, vertices, normals, coordinates, t_mesh, t_bulbs, t_bulbsCount, &textureBuffer);
    }
}

// void Renderer::draw(Mesh *t_meshes, u16 t_amount) { draw(t_meshes, t_amount, NULL, 0); }
// void Renderer::draw(Mesh **t_meshes, u16 t_amount) { draw(t_meshes, t_amount, NULL, 0); }

void Renderer::draw(Mesh &t_mesh) { draw(t_mesh, NULL, 0); }

/// ---

void Renderer::setCameraDefinitions(Matrix *t_worldView, Vector3 *t_cameraPos, Plane *t_planes)
{
    renderData.worldView = t_worldView;
    renderData.cameraPosition = t_cameraPos;
    renderData.frustumPlanes = t_planes;
}

void Renderer::beginFrameIfNeeded()
{
    if (isFrameEmpty)
    {
        isFrameEmpty = false;
        gifSender->sendClear(&zBuffer, &worldColor);
    }
}

void Renderer::endFrame(float fps)
{
    if (!isFrameEmpty)
    {
        if (fps > 49.0F && isVSyncEnabled)
            graph_wait_vsync();
        flipBuffers();
    }
}

/** We need to flip buffers outside of the chain, for some reason,
 * so we use a separate small packet
 * Do not use this method. This is called via packetManager
 */
void Renderer::flipBuffers()
{
    graph_set_framebuffer_filtered(
        frameBuffers[context].address,
        frameBuffers[context].width,
        frameBuffers[context].psm,
        0,
        0);
    context ^= 1;
    isFrameEmpty = 1;
    packet2_update(flipPacket, draw_framebuffer(flipPacket->base, 0, &frameBuffers[context]));
    packet2_update(flipPacket, draw_finish(flipPacket->next));
    dma_channel_send_packet2(flipPacket, DMA_CHANNEL_GIF, true);
    draw_wait_finish();
}
