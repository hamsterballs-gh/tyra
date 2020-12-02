/*
# ______       ____   ___
#   |     \/   ____| |___|    
#   |     |   |   \  |   |       
#-----------------------------------------------------------------------
# Copyright 2020, tyra - https://github.com/h4570/tyra
# Licenced under Apache License 2.0
# Sandro Sobczyński <sandro.sobczynski@gmail.com>
*/

#include "ari.hpp"

#include "utils/math.hpp"

// ----
// Constructors/Destructors
// ----

const u8 WATER_TILES_COUNT = 64;

Ari::Ari(Engine *t_engine) : engine(t_engine), camera(&engine->screen)
{
    waterFloors = new Mesh[WATER_TILES_COUNT];
    spirals = new Point[WATER_TILES_COUNT];
}

Ari::~Ari() {}

// ----
// Methods
// ----

void Ari::onInit()
{
    engine->renderer->setCameraDefinitions(&camera.worldView, &camera.position, camera.planes);
    engine->audio.setSongVolume(40);
    // engine->audio.loadSong("MOV-CIRC.WAV");
    engine->audio.loadSong("nob-else.wav");

    adpcm1 = engine->audio.loadADPCM("ziobro1.adpcm");
    adpcm2 = engine->audio.loadADPCM("ziobro2.adpcm");

    texRepo = engine->renderer->getTextureRepository();
    engine->renderer->disableVSync();
    island.loadDff("sunnyisl/", "sunnyisl", 0.1F, false);
    island.rotation.x = -1.6F;
    island.position.set(0.0F, 10.0F, 20.0F);
    island.shouldBeBackfaceCulled = true;
    island.shouldBeFrustumCulled = false;

    fist.size.set(100.0F, 100.0F);
    Texture *fistTex = texRepo->add("2d/", "fist", PNG);
    fistTex->addLink(fist.getId());

    islandAddons.loadDff("sunnyisl/", "sunnyisl3", 0.1F, false);
    islandAddons.shouldBeBackfaceCulled = true;
    islandAddons.rotation.x = -1.6F;
    islandAddons.position.set(0.0F, 10.0F, 20.0F);

    skybox.loadObj("skybox/", "skybox", 100.0F, false);
    skybox.shouldBeFrustumCulled = false;

    waterFloors[0].loadObj("water/", "water", 5.0F, false);
    waterFloors[0].position.set(0.0F, 8.0F, 0.0F);
    texRepo->addByMesh("water/", waterFloors[0], BMP);
    for (u8 i = 0; i < WATER_TILES_COUNT; i++)
    {
        spirals[i].x = 1.0F;
        spirals[i].y = 2.0F;
    }
    u32 spiralOffset = (u32)Math::sqrt(WATER_TILES_COUNT);
    calcSpiral(spiralOffset, spiralOffset);
    for (u8 i = 1; i < WATER_TILES_COUNT; i++)
    {
        waterFloors[i].loadFrom(waterFloors[0]);
        waterFloors[i].position.set(10.0F * spirals[i].x, 8.0F, 10.0F * spirals[i].y);
        texRepo->getByMesh(waterFloors[0].getId(), waterFloors[0].getMaterial(0).getId())
            ->addLink(waterFloors[i].getId(), waterFloors[i].getMaterial(0).getId());
    }

    texRepo->addByMesh("sunnyisl/", island, BMP);
    texRepo->addByMesh("sunnyisl/", islandAddons, BMP);
    texRepo->addByMesh("skybox/", skybox, BMP);
    texRepo->addByMesh("ari/", player.mesh, BMP);
    // engine->audio.playSong();

    // fontx_load("rom0:KROM", &krom_u, SINGLE_BYTE, 2, 1, 1);
    // fontx_load("rom0:KROM", &krom_k, DOUBLE_BYTE, 2, 1, 1);
    // char *ini;
    // if ((ini = fontstudio_load_ini("host:impress.ini")) != NULL)
    // {
    //     fontstudio_parse_ini(&impress, ini, 512, 256);
    //     free(ini);
    // }
    // else
    // {
    //     printf("Error: cannot load ini file.\n");
    // }
}

void Ari::initBulb()
{
    bulb.intensity = 15;
    bulb.position.set(5.0F, 10.0F, 10.0F);
}

// extern unsigned int image_clut32[];
// extern unsigned char image_pixel[];
// #include <font.h>
// fsfont_t impress;
// fontx_t krom_u;
// fontx_t krom_k;

void Ari::onUpdate()
{

    // vertex_t v0;

    // color_t c0;
    // color_t c1;

    // v0.x = 320.0f;
    // v0.y = 240.0f;
    // v0.z = 4;

    // c0.r = 0xFF;
    // c0.g = 0xFF;
    // c0.b = 0xFF;
    // c0.a = 0x80;
    // c0.q = 1.0f;

    // c1.r = 0xFF;
    // c1.g = 0x00;
    // c1.b = 0x00;
    // c1.a = 0x40;
    // c1.q = 1.0f;

    // unsigned char str0[] = {0x61, 0x62, 0xC2, 0xA9, 0x78, 0xC2, 0xA5, 0xC2, 0xB2, '\0'};

    // // Shift-JIS
    // unsigned char str1[] = {0x81, 0xBC, 0x93, 0xF1, 0x93, 0xF1, 0x93, 0xF1, 0x81, 0x69, 0x81, 0x40, 0x81,
    //                         0x4F, 0x83, 0xD6, 0x81, 0x4F, 0x81, 0x6A, 0x93, 0xF1, 0x81, 0xBD, 0x0D, '\0'};
    // impress.scale = 3.0f;

    if (engine->pad.isCrossClicked)
    {
        // engine->audio.loadSong("nob-else.wav");
        engine->audio.playADPCM(adpcm1);
        printf("FPS:%f\n", engine->fps);
    }
    if (engine->pad.isCircleClicked)
        engine->audio.playADPCM(adpcm2);

    camera.update(engine->pad, player.mesh);
    engine->renderer->draw(skybox);
    engine->renderer->draw(fist);
    // engine->renderer->draw(impress, str0, str1, krom_u, krom_k, v0, c0, c1);
    engine->renderer->draw(island);
    engine->renderer->draw(islandAddons);
    engine->renderer->draw(player.mesh);
    for (u8 i = 0; i < WATER_TILES_COUNT; i++)
        engine->renderer->draw(waterFloors[i]);
}

void Ari::calcSpiral(int X, int Y)
{
    int x, y, dx;
    x = y = dx = 0;
    int dy = -1;
    int t = X > Y ? X : Y;
    int maxI = t * t;
    for (int i = 0; i < maxI; i++)
    {
        if ((-X / 2 <= x) && (x <= X / 2) && (-Y / 2 <= y) && (y <= Y / 2))
        {
            spirals[i].x = x;
            spirals[i].y = y;
        }
        if ((x == y) || ((x < 0) && (x == -y)) || ((x > 0) && (x == 1 - y)))
        {
            t = dx;
            dx = -dy;
            dy = t;
        }
        x += dx;
        y += dy;
    }
}
