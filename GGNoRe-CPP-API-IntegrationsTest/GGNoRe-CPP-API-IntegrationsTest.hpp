/*
 * Copyright 2021 Lo�c Venerosy
 */

#pragma once

#include <assert.h>

bool TestRemoteMockRollback();

// TODO:
// actor lifetime
// pooling support

int main()
{
	assert(TestRemoteMockRollback());

	return 0;
}