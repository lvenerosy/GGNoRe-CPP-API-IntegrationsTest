/*
 * Copyright 2021 Loïc Venerosy
 */

#pragma once

#include <assert.h>

bool Test1Local2RemoteMockRollback(const bool UseFakeRollback, const bool UseRandomInputs);

int main()
{
	assert(Test1Local2RemoteMockRollback(false, false));
	assert(Test1Local2RemoteMockRollback(true, false));
	assert(Test1Local2RemoteMockRollback(false, true));
	assert(Test1Local2RemoteMockRollback(true, true));

	return 0;
}