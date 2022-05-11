/*
 * Copyright 2021 Loic Venerosy
 */

#include <GGNoRe-CPP-API-IntegrationsTest.hpp>

#include <TEST_Mocks.hpp>

using namespace GGNoRe::API;

namespace TEST_NSPC_Systems
{

const id_t Player1Id = 0;
const uint8_t Player1SystemIndex = 0;
const id_t Player2Id = 1;
const uint8_t Player2SystemIndex = 1;

class TEST_LocalMock final : public TEST_ABS_SystemMock
{
	float DeltaDurationInSeconds() const override
	{
		return Setup.LocalMockHardwareFrameDurationInSeconds;
	}

public:
	TEST_LocalMock(const PlayersSetup Setup)
		:
		TEST_ABS_SystemMock(DATA_Player{ Player1Id, true, Setup.LocalStartFrameIndex, Player1SystemIndex }, DATA_Player{ Player2Id, false, uint16_t(Setup.LocalStartFrameIndex + Setup.RemoteStartOffsetInFrames), Player1SystemIndex }, Setup)
	{
		assert(Player1Id != Player2Id);
	}
};

class TEST_RemoteMock final : public TEST_ABS_SystemMock
{
	float DeltaDurationInSeconds() const override
	{
		return Setup.RemoteMockHardwareFrameDurationInSeconds;
	}

public:
	TEST_RemoteMock(const PlayersSetup Setup)
		:
		TEST_ABS_SystemMock(DATA_Player{ Player2Id, true,  uint16_t(Setup.LocalStartFrameIndex + Setup.RemoteStartOffsetInFrames), Player2SystemIndex }, DATA_Player{ Player1Id, false,  uint16_t(Setup.LocalStartFrameIndex + Setup.RemoteStartOffsetInFrames), Player2SystemIndex }, Setup)
	{
		assert(Player1Id != Player2Id);
	}
};

}

bool Test1Local2RemoteMockRollback(const DATA_CFG Config, const TestEnvironment Environment, const PlayersSetup Setup)
{
	assert(Environment.ReceiveRemoteIntervalInFrames > 0);
	assert(Setup.LocalMockHardwareFrameDurationInSeconds > 0.f);
	assert(Setup.RemoteMockHardwareFrameDurationInSeconds > 0.f);

	DATA_CFG::Load(Config);

	if (Setup.InitialLatencyInFrames > Config.RollbackConfiguration.MinRollbackFrameCount)
	{
		return true;
	}

	const bool AllowLocalDoubleSimulation = Setup.LocalMockHardwareFrameDurationInSeconds > Config.SimulationConfiguration.FrameDurationInSeconds;
	if (AllowLocalDoubleSimulation)
	{
		// Creates too many edge cases around test initialization
		return true;
	}

	const bool AllowRemoteDoubleSimulation = Setup.RemoteMockHardwareFrameDurationInSeconds > Config.SimulationConfiguration.FrameDurationInSeconds;

	const bool AllowLocalStallAdvantage = Environment.ReceiveRemoteIntervalInFrames > 1 || AllowLocalDoubleSimulation || Setup.InitialLatencyInFrames > 0;
	const bool AllowRemoteStallAdvantage = Environment.ReceiveRemoteIntervalInFrames > 1 || AllowRemoteDoubleSimulation || Setup.InitialLatencyInFrames > 0;

	const bool RoundTripPossibleWithinRollbackWindow = (size_t)Environment.ReceiveRemoteIntervalInFrames * 2 + Setup.InitialLatencyInFrames < Config.RollbackConfiguration.MinRollbackFrameCount;

	const bool AllowLocalStarvedForInput =
		(size_t)AllowRemoteStallAdvantage * (Config.SimulationConfiguration.StallTimerDurationInSeconds < Setup.RemoteMockHardwareFrameDurationInSeconds) * Config.RollbackConfiguration.MinRollbackFrameCount +
		(size_t)AllowLocalDoubleSimulation * (Config.SimulationConfiguration.DoubleSimulationTimerDurationInSeconds < Setup.LocalMockHardwareFrameDurationInSeconds) * Config.RollbackConfiguration.MinRollbackFrameCount >=
			Config.RollbackConfiguration.MinRollbackFrameCount ||
		AllowRemoteDoubleSimulation ||
		!RoundTripPossibleWithinRollbackWindow;
	const bool AllowRemoteStarvedForInput =
		(size_t)AllowLocalStallAdvantage * (Config.SimulationConfiguration.StallTimerDurationInSeconds < Setup.LocalMockHardwareFrameDurationInSeconds) * Config.RollbackConfiguration.MinRollbackFrameCount +
		(size_t)AllowRemoteDoubleSimulation * (Config.SimulationConfiguration.DoubleSimulationTimerDurationInSeconds < Setup.RemoteMockHardwareFrameDurationInSeconds) * Config.RollbackConfiguration.MinRollbackFrameCount >=
			Config.RollbackConfiguration.MinRollbackFrameCount ||
		AllowLocalDoubleSimulation ||
		!RoundTripPossibleWithinRollbackWindow;

	TEST_NSPC_Systems::TEST_LocalMock Local(Setup);
	TEST_NSPC_Systems::TEST_RemoteMock Remote(Setup);

	for (size_t IterationIndex = 0; IterationIndex < Environment.TestDurationInFrames; ++IterationIndex)
	{
		Local.PreUpdate((uint16_t)IterationIndex + Setup.LocalStartFrameIndex);
		Remote.PreUpdate((uint16_t)IterationIndex + Setup.LocalStartFrameIndex);

		assert(Local.IsRunning());

		Local.Update
		(
			{
				AllowLocalDoubleSimulation,
				AllowLocalStallAdvantage,
				AllowLocalStarvedForInput,
				Setup.LocalMockHardwareFrameDurationInSeconds < Config.SimulationConfiguration.FrameDurationInSeconds
			},
			Remote
		);

		if (Remote.IsRunning())
		{
			Remote.Update
			(
				{
					AllowRemoteDoubleSimulation,
					AllowRemoteStallAdvantage,
					AllowRemoteStarvedForInput,
					Setup.RemoteMockHardwareFrameDurationInSeconds < Config.SimulationConfiguration.FrameDurationInSeconds
				},
				Local
			);
		}

		Local.PostUpdate((uint16_t)IterationIndex + Setup.LocalStartFrameIndex, Remote);
		Remote.PostUpdate((uint16_t)IterationIndex + Setup.LocalStartFrameIndex, Local);

		// Must be "greater than" in order to make sure that the initialization packet is loaded first otherwise it might be overwritten by a regular packet
		if (IterationIndex > (size_t)Setup.RemoteStartOffsetInFrames + Setup.InitialLatencyInFrames && IterationIndex % Environment.ReceiveRemoteIntervalInFrames == 0)
		{
			TEST_NSPC_Systems::TransferLocalPlayersInputs();
		}
	}

	TEST_NSPC_Systems::ForceResetAndCleanup();

	return true;
}
