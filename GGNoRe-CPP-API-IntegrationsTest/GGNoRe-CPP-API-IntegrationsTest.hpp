/*
 * Copyright 2021 Loïc Venerosy
 */

#pragma once

#include <assert.h>

bool TestRemoteMockRollback();

// TODO:
// actor lifetime
// pooling support
// join mid game
// more than 2 players

int main()
{
	assert(TestRemoteMockRollback());

	return 0;
}