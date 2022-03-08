/*
 * Copyright 2021 Loïc Venerosy
 */

#pragma once

#include <assert.h>

bool TestRemoteMockRollback(const bool UseFakeRollback, const bool UseRandomInputs);

// TODO:
// actor lifetime
// pooling support
// more than 2 players
// Activate/DeactivateInPast

int main()
{
	assert(TestRemoteMockRollback(false, false));
	assert(TestRemoteMockRollback(true, false));
	assert(TestRemoteMockRollback(false, true));
	assert(TestRemoteMockRollback(true, true));

	return 0;
}