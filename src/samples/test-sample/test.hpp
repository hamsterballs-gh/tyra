/*
# ______       ____   ___
#   |     \/   ____| |___|    
#   |     |   |   \  |   |       
#-----------------------------------------------------------------------
# Copyright 2020, tyra - https://github.com/h4570/tyra
# Licenced under Apache License 2.0
# Sandro Sobczyński <sandro.sobczynski@gmail.com>
*/

#ifndef _TEST_
#define _TEST_

#include <tamtypes.h>
#include <audsrv.h>
#include <game.hpp>
#include <engine.hpp>
#include <modules/texture_repository.hpp>
#include "./camera.hpp"
#include "models/sprite.hpp"

class Test : public Game
{

public:
    Test(Engine *t_engine);
    ~Test();

    void onInit();
    void onUpdate();

    Engine *engine;

private:
    void calcSpiral(int X, int Y);
    void padStuff();
    Mesh triangle, *waterFloors;
    Point *spirals;
    Camera camera;
    TextureRepository *texRepo;
};

#endif
