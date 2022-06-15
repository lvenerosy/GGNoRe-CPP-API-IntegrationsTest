/*
 * Copyright 2021 Loic Venerosy
 */

#include <GGNoRe-CPP-API-IntegrationsTest.hpp>

#include <TEST_SystemMock.hpp>

using namespace GGNoRe::API;

bool Test1Local1RemoteMockRollback(const DATA_CFG Config, const TestEnvironment Environment, const PlayersSetup Setup)
{
	assert(Environment.ReceiveRemoteIntervalInFrames > 0);
	assert(Setup.LocalMockHardwareFrameDurationInSeconds > 0.f);
	assert(Setup.RemoteMockHardwareFrameDurationInSeconds > 0.f);

	DATA_CFG::Load(Config);

	const bool AllowLocalDoubleSimulation = Setup.LocalMockHardwareFrameDurationInSeconds > Config.SimulationConfiguration.FrameDurationInSeconds;
	if (AllowLocalDoubleSimulation)
	{
		// Creates too many edge cases around test initialization
		return true;
	}

	const bool AllowRemoteDoubleSimulation = Setup.RemoteMockHardwareFrameDurationInSeconds > Config.SimulationConfiguration.FrameDurationInSeconds;

	const bool AllowLocalStallAdvantage = Environment.ReceiveRemoteIntervalInFrames > 1 || AllowLocalDoubleSimulation;
	const bool AllowRemoteStallAdvantage = Environment.ReceiveRemoteIntervalInFrames > 1 || AllowRemoteDoubleSimulation;

	const bool RoundTripPossibleWithinRollbackWindow = (size_t)Environment.ReceiveRemoteIntervalInFrames * 2 < Config.RollbackConfiguration.MinRollbackFrameCount;

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

	const uint8_t Player1SystemIndex = 0;
	const uint8_t Player2SystemIndex = 1;

	// In this test, local updates before remote and the activations are order sensitive so the player ids are used to ensure proper ordering
	assert(TEST_NSPC_Systems::Player1Id < TEST_NSPC_Systems::Player2Id);

	TEST_NSPC_Systems::TEST_SystemMock Local(
		DATA_Player{ TEST_NSPC_Systems::Player1Id, true, Setup.LocalStartFrameIndex, Player1SystemIndex },
		DATA_Player{ TEST_NSPC_Systems::Player2Id, false, uint16_t(Setup.LocalStartFrameIndex + Setup.RemoteStartOffsetInFrames), Player1SystemIndex },
		Setup.LocalMockHardwareFrameDurationInSeconds,
		Setup
	);
	TEST_NSPC_Systems::TEST_SystemMock Remote(
		DATA_Player{ TEST_NSPC_Systems::Player2Id, true, uint16_t(Setup.LocalStartFrameIndex + Setup.RemoteStartOffsetInFrames), Player2SystemIndex },
		DATA_Player{ TEST_NSPC_Systems::Player1Id, false,  uint16_t(Setup.LocalStartFrameIndex + Setup.RemoteStartOffsetInFrames), Player2SystemIndex },
		Setup.RemoteMockHardwareFrameDurationInSeconds,
		Setup
	);

	for (size_t IterationIndex = 0; IterationIndex < Environment.TestDurationInFrames; ++IterationIndex)
	{
		const uint16_t TestFrameIndex = (uint16_t)IterationIndex + Setup.LocalStartFrameIndex;

		Local.PreUpdate(TestFrameIndex, Remote);
		Remote.PreUpdate(TestFrameIndex, Local);

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

		Local.PostUpdate(TestFrameIndex, Remote);
		Remote.PostUpdate(TestFrameIndex, Local);

		// Must be "greater than" in order to make sure that the initialization packet is loaded first otherwise it might be overwritten by a regular packet
		if (IterationIndex > (size_t)Setup.RemoteStartOffsetInFrames && IterationIndex % Environment.ReceiveRemoteIntervalInFrames == 0)
		{
			TEST_NSPC_Systems::TransferLocalPlayersInputs();
		}
	}

	TEST_NSPC_Systems::ForceResetAndCleanup();

	return true;
}
