/*
 * Copyright 2021 Lo�c Venerosy
 */

#pragma once

#include <assert.h>

bool TestRemoteMockRollback();

// TODO:
// actor lifetime
// join mid game
// variable framerate

int main()
{
	assert(TestRemoteMockRollback());

	return 0;
}