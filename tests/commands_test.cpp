//
// Created by Robotlab on 07/10/2025.
//

#include <iostream>
#include "../exoskeleton/core/commands.h"

auto main() -> int
{
    exoskeleton::core::parse("c");
    exoskeleton::core::parse("dc");
    exoskeleton::core::parse("cs");

    exoskeleton::core::parse("e e_ext,e_flex");
    exoskeleton::core::parse("d e_ext, e_flex");
    exoskeleton::core::parse("f e_ext,e_flex 0:127 100:60 180:30");
    exoskeleton::core::parse("fs e_ext,e_flex 0");
    exoskeleton::core::parse("ffs e_ext:0 e_flex:1");
    exoskeleton::core::parse("fg e_ext");
    exoskeleton::core::parse("z e_ext");
    exoskeleton::core::parse("z *");
    exoskeleton::core::parse("op e_ext 1000");
    exoskeleton::core::parse("op e_ext -1000");
    exoskeleton::core::parse("s");
    exoskeleton::core::parse("r");

    exoskeleton::core::parse("x");
    exoskeleton::core::parse("env e_flex CSTNY001");
    exoskeleton::core::parse("env");
}