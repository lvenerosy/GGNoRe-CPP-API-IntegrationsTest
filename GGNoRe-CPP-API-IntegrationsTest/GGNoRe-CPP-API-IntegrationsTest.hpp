/*
 * Copyright 2021 Loic Venerosy
 */

#pragma once

#include <GGNoRe-CPP-API.hpp>

#include <array>
#include <cassert>
#include <cstdlib>
#include <functional>
#include <iostream>

struct TestEnvironment
{
	size_t TestDurationInFrames = 60;
	uint16_t ReceiveRemoteIntervalInFrames = 3;
};

struct PlayersSetup
{
	bool UseRandomInputs = false;
	uint16_t LocalStartFrameIndex = 0;
	uint16_t RemoteStartOffsetInFrames = 2;
	uint16_t InitialLatencyInFrames = 3;
	float LocalMockHardwareFrameDurationInSeconds = 0.016667f;
	float RemoteMockHardwareFrameDurationInSeconds = 0.016667f;
};

struct RangeFunctorChain
{
	size_t GlobalTestCount = 1;
	std::function<void()> RangeFunctor;
};

template<typename T, std::size_t N> RangeFunctorChain GetRangeFunctor(const std::array<T, N> ValueRange, T& Value, const RangeFunctorChain ChainedRangeFunctor) noexcept
{
	return
	{
		ChainedRangeFunctor.GlobalTestCount * N,
		[ValueRange, &Value, ChainedRangeFunctor]()
		{
			for (const auto& NewValue : ValueRange)
			{
				Value = NewValue;
				ChainedRangeFunctor.RangeFunctor();
			}
		}
	};
}

bool Test1Local2RemoteMockRollback(const GGNoRe::API::DATA_CFG Config, const TestEnvironment Environment, const PlayersSetup Setup);

int main()
{
	GGNoRe::API::DATA_CFG Config;
	TestEnvironment Environment;
	PlayersSetup Setup;

	GGNoRe::API::ABS_DBG_HumanReadable::LoggingLevel = GGNoRe::API::ABS_DBG_HumanReadable::LoggingLevel_E::Lean;

	struct TestProgress
	{
		size_t CurrentTestCounter = 0;
		const size_t StartTestIndex = 1; // Run the sln in development mode for optimal speed while keeping asserts, then if an assert is hit start from the failing test and run in debug mode
	};
	TestProgress Progress;
	RangeFunctorChain Tests;
	const RangeFunctorChain TestRunner
	{
		1,
		[&Config, &Environment, &Setup, &Tests, &Progress]()
		{
			++Progress.CurrentTestCounter;
			if (Progress.CurrentTestCounter >= Progress.StartTestIndex)
			{
				assert(Test1Local2RemoteMockRollback(Config, Environment, Setup));
				std::cout << std::to_string(Progress.CurrentTestCounter) << "/" << Tests.GlobalTestCount << std::endl;
			}
		}
	};

	Tests = GetRangeFunctor(std::array<size_t, 4>{ 0, 1, 2, 3 }, Config.RollbackConfiguration.DelayFramesCount, TestRunner);
	Tests = GetRangeFunctor(std::array<size_t, 4>{ 0, 1, 2, 3 }, Config.RollbackConfiguration.InputLeniencyFramesCount, Tests);
	Tests = GetRangeFunctor(std::array<size_t, 3>{ 1, 4, 7 }, Config.RollbackConfiguration.MinRollbackFrameCount, Tests);
	Tests = GetRangeFunctor(std::array<bool, 1>{ false }, Config.RollbackConfiguration.ForceMaximumRollback, Tests);

	// 144hz, 60hz, 45hz, 30hz
	Tests = GetRangeFunctor(std::array<GGNoRe::API::SER_FixedPoint, 4>{ 0.006944f, 0.016667f, 0.022222f, 0.033333f }, Config.SimulationConfiguration.FrameDurationInSeconds, Tests);
	Tests = GetRangeFunctor(std::array<GGNoRe::API::SER_FixedPoint, 2>{ 0.f, 60.f * 0.016667f }, Config.SimulationConfiguration.StallTimerDurationInSeconds, Tests);
	Tests = GetRangeFunctor(std::array<GGNoRe::API::SER_FixedPoint, 2>{ 0.f, 60.f * 0.016667f }, Config.SimulationConfiguration.DoubleSimulationTimerDurationInSeconds, Tests);

	Tests = GetRangeFunctor(std::array<uint16_t, 3>{ 1, 2, 5 }, Environment.ReceiveRemoteIntervalInFrames, Tests);

	Tests = GetRangeFunctor(std::array<bool, 1>{ false }, Setup.UseRandomInputs, Tests);
	Tests = GetRangeFunctor(std::array<uint16_t, 3>{ 0, 1, 10 }, Setup.LocalStartFrameIndex, Tests);
	Tests = GetRangeFunctor(std::array<uint16_t, 3>{ 0, 2, 5 }, Setup.RemoteStartOffsetInFrames, Tests);
	Tests = GetRangeFunctor(std::array<uint16_t, 3>{ 0, 2, 5 }, Setup.InitialLatencyInFrames, Tests);
	// 90fps, 60fps, 30fps
	Tests = GetRangeFunctor(std::array<float, 3>{ 0.011111f, 0.016667f, 0.033333f }, Setup.LocalMockHardwareFrameDurationInSeconds, Tests);
	// 120fps, 60fps, 40fps, 16fps
	Tests = GetRangeFunctor(std::array<float, 4>{ 0.008333f, 0.016667f, 0.025f, 0.0625f }, Setup.RemoteMockHardwareFrameDurationInSeconds, Tests);

	srand(0);
	Tests.RangeFunctor();

	return 0;
}