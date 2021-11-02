/*
 * Copyright 2021 Loïc Venerosy
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