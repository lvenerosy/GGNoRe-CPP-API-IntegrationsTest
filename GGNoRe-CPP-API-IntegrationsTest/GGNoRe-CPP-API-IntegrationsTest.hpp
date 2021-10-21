/*
 * Copyright 2021 Loïc Venerosy
 */

#pragma once

#include <Core/Data/DATA_CFG.hpp>

int main()
{
	GGNoRe::API::DATA_CFG::Get().LogHumanReadable("LINKER TEST");
	return 0;
}