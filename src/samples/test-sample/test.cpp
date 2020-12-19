/*
# ______       ____   ___
#   |     \/   ____| |___|    
#   |     |   |   \  |   |       
#-----------------------------------------------------------------------
# Copyright 2020, tyra - https://github.com/h4570/tyra
# Licenced under Apache License 2.0
# Sandro Sobczyński <sandro.sobczynski@gmail.com>
*/

#include "test.hpp"

#include "utils/math.hpp"

// ----
// Constructors/Destructors
// ----

const u16 WATER_TILES_COUNT = 36;

Test::Test(Engine *t_engine) : engine(t_engine), camera(&engine->screen)
{
    waterFloors = new Mesh[WATER_TILES_COUNT];
    spirals = new Point[WATER_TILES_COUNT];
}

Test::~Test() {}

// ----
// Methods
// ----

void Test::onInit()
{
    engine->renderer->setCameraDefinitions(&camera.worldView, &camera.position, camera.planes);
    texRepo = engine->renderer->getTextureRepository();
    engine->renderer->disableVSync();

    triangle.loadObj("triangle/", "triangle", 3.0F, false);
    texRepo->addByMesh("triangle/", triangle, BMP);
    triangle.position.z = -5.0F;
    triangle.position.y = 30.0F;

    waterFloors[0].loadObj("water/", "water", 10.0F, false);
    waterFloors[0].position.set(0.0F, 10.0F, 0.0F);
    waterFloors[0].shouldBeBackfaceCulled = false;
    waterFloors[0].shouldBeFrustumCulled = false;
    texRepo->addByMesh("water/", waterFloors[0], BMP);
    for (u16 i = 0; i < WATER_TILES_COUNT; i++)
    {
        spirals[i].x = 1.0F;
        spirals[i].y = 2.0F;
    }
    u32 spiralOffset = (u32)Math::sqrt(WATER_TILES_COUNT);
    calcSpiral(spiralOffset, spiralOffset);
    for (u16 i = 1; i < WATER_TILES_COUNT; i++)
    {
        waterFloors[i].loadFrom(waterFloors[0]);
        waterFloors[i].shouldBeBackfaceCulled = false;
        waterFloors[i].shouldBeFrustumCulled = false;
        waterFloors[i].position.set(20.0F * spirals[i].x, 10.0F, 20.0F * spirals[i].y);
        texRepo->getByMesh(waterFloors[0].getId(), waterFloors[0].getMaterial(0).getId())
            ->addLink(waterFloors[i].getId(), waterFloors[i].getMaterial(0).getId());
    }
}

void Test::onUpdate()
{

    // My notes:
    // - I know.. a lot of code is a mess, but I plan huge refactor, especially after the last tries
    // - Screen settings (width, height, fov...) can be changed in /engine.cpp
    // - Matrix have two implementations of multiply
    //     First (PS2SDK) = operator*
    //     Second (DMC2, PS2GL) = operator& (we are using this one)
    //   One of them will be deleted, when code will be fixed & refactored
    // - View matrix is set in Matrix::lookAt()
    // - Proj matrix is set in Matrix::setFrustum()
    // - I found that PS2GL is using 480x224 non interlaced
    // - Drawing env & framebuffers are set in renderer.cpp - initDrawEnv() and allocateBuffers()
    // - The meat is in
    //     vif_sender.cpp - constructor, calcModelViewProjMatrix(), drawVertices()
    //     draw3D.vclpp
    // - A lot of code needs refactor, but at first it must work.
    // - vclpp needed - https://github.com/glampert/vclpp

    // - Refactor drawTheSameWithOtherMatrices()
    // - 2x duplicate, vif sender i renderer
    // - Bug z floorsami

    padStuff();
    camera.update(engine->pad, waterFloors[0]);
    engine->renderer->draw(triangle);
    // for (size_t i = 0; i < WATER_TILES_COUNT; i++)
    //     engine->renderer->draw(waterFloors[i]);
    engine->renderer->draw(waterFloors[13]);
}

void Test::padStuff()
{
    if (engine->pad.lJoyV <= 100)
        triangle.position.y += 0.3F;
    else if (engine->pad.lJoyV >= 200)
        triangle.position.y -= 0.3F;
    if (engine->pad.lJoyH <= 100)
        triangle.position.x -= 0.3F;
    else if (engine->pad.lJoyH >= 200)
        triangle.position.x += 0.3F;
    if (engine->pad.isTriangleClicked)
        triangle.position.z += .5F;
    if (engine->pad.isSquareClicked)
        triangle.position.z -= .5F;

    if (engine->pad.isCrossClicked)
    {
        printf("FPS:%f\n", engine->fps);
        triangle.rotation.z += 0.1F;
    }
    if (engine->pad.isCircleClicked)
        triangle.rotation.z -= 0.1F;
}

void Test::calcSpiral(int X, int Y)
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
