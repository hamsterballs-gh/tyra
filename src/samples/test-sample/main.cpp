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

int main()
{
    Engine engine = Engine();
    Test game = Test(&engine);
    game.engine->init(&game, 128);
    SleepThread();
    return 0;
}
