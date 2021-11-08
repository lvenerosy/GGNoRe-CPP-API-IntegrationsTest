/*
 * Copyright 2021 Loïc Venerosy
 */

#pragma once

#include <assert.h>

bool TestRemoteMockRollback();

// TODO:
// actor lifetime

int main()
{
	assert(TestRemoteMockRollback());

	return 0;
}