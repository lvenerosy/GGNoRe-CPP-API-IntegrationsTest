/*
 * Copyright 2021 Loic Venerosy
 */

#include <GGNoRe-CPP-API-IntegrationsTest.hpp>

#include <TEST_Mocks.hpp>

using namespace GGNoRe::API;

namespace TEST_NSPC_Systems
{

class TEST_LocalMock final : public TEST_ABS_SystemMock
{
public:
	const DATA_Player TrueLocalPlayer1Identity;
	const DATA_Player LocalPlayer2Identity;

	TEST_Player TrueLocalPlayer1;
	TEST_Player LocalPlayer2;

	TEST_LocalMock(const PlayersSetup Setup)
		:
		TEST_ABS_SystemMock(Setup),
		TrueLocalPlayer1Identity({ 0, true, Setup.LocalStartFrameIndex, 0 }),
		LocalPlayer2Identity({ 1, false, RemoteStartFrameIndex, TrueLocalPlayer1Identity.SystemIndex }),
		TrueLocalPlayer1(Setup.UseRandomInputs),
		LocalPlayer2(Setup.UseRandomInputs)
	{
		assert(SystemIndexes.find(TrueLocalPlayer1Identity.SystemIndex) == SystemIndexes.cend());

		SystemMultiton::GetRollbackable(TrueLocalPlayer1Identity.SystemIndex).SyncWithRemoteFrameIndex(Setup.LocalStartFrameIndex);
		SystemIndexes.insert(TrueLocalPlayer1Identity.SystemIndex);

		TrueLocalPlayer1.ActivateNow(TrueLocalPlayer1Identity);
	}

	bool PreUpdate(const uint16_t FrameIndex) override
	{
		if (FrameIndex == RemoteStartFrameIndex + Setup.InitialLatencyInFrames)
		{
			if (Setup.InitialLatencyInFrames == 0)
			{
				LocalPlayer2.ActivateNow(LocalPlayer2Identity);
			}
			else
			{
				try
				{
					LocalPlayer2.ActivateInPast(LocalPlayer2Identity, RemoteStartFrameIndex);
				}
				catch (const I_RB_Rollbackable::RegisterSuccess_E& Success)
				{
					// Basically illegal activation, in a real use you first would make sure you can activate before actually activating
					return Success == I_RB_Rollbackable::RegisterSuccess_E::UnreachablePastFrame && Setup.InitialLatencyInFrames > DATA_CFG::Get().RollbackFrameCount();
				}
			}
		}

		return true;
	}

	void Update(const OutcomesSanityCheck AllowedOutcomes) override
	{
		while (!Tick(Setup.LocalMockHardwareFrameDurationInSeconds, TrueLocalPlayer1Identity.SystemIndex, AllowedOutcomes));
	}
};

class TEST_RemoteMock final : public TEST_ABS_SystemMock
{
public:
	const DATA_Player TrueRemotePlayer2Identity;
	const DATA_Player RemotePlayer1Identity;

	TEST_Player TrueRemotePlayer2;
	TEST_Player RemotePlayer1;

	TEST_RemoteMock(const PlayersSetup Setup)
		:
		TEST_ABS_SystemMock(Setup),
		TrueRemotePlayer2Identity({ 1, true, RemoteStartFrameIndex, 1 }),
		RemotePlayer1Identity({ 0, false, RemoteStartFrameIndex, TrueRemotePlayer2Identity.SystemIndex }),
		TrueRemotePlayer2(Setup.UseRandomInputs),
		RemotePlayer1(Setup.UseRandomInputs)
	{
	}

	bool PreUpdate(const uint16_t FrameIndex) override
	{
		if (FrameIndex == RemoteStartFrameIndex)
		{
			assert(SystemIndexes.find(TrueRemotePlayer2Identity.SystemIndex) == SystemIndexes.cend());

			SystemMultiton::GetRollbackable(TrueRemotePlayer2Identity.SystemIndex).SyncWithRemoteFrameIndex(Setup.LocalStartFrameIndex);
			SystemIndexes.insert(TrueRemotePlayer2Identity.SystemIndex);

			TrueRemotePlayer2.ActivateNow(TrueRemotePlayer2Identity);
		}

		if (FrameIndex == RemoteStartFrameIndex + Setup.InitialLatencyInFrames)
		{
			if (Setup.InitialLatencyInFrames == 0)
			{
				RemotePlayer1.ActivateNow(RemotePlayer1Identity);
			}
			else
			{
				try
				{
					RemotePlayer1.ActivateInPast(RemotePlayer1Identity, RemoteStartFrameIndex);
				}
				catch (const I_RB_Rollbackable::RegisterSuccess_E& Success)
				{
					// Basically illegal activation, in a real use you first would make sure you can activate before actually activating
					return Success == I_RB_Rollbackable::RegisterSuccess_E::UnreachablePastFrame && Setup.InitialLatencyInFrames > DATA_CFG::Get().RollbackFrameCount();
				}
			}
		}

		return true;
	}

	void Update(const OutcomesSanityCheck AllowedOutcomes) override
	{
		while (!Tick(Setup.RemoteMockHardwareFrameDurationInSeconds, TrueRemotePlayer2Identity.SystemIndex, AllowedOutcomes));
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
	const bool AllowRemoteDoubleSimulation = Setup.RemoteMockHardwareFrameDurationInSeconds > Config.SimulationConfiguration.FrameDurationInSeconds;
	const bool AllowLocalStallAdvantage = Environment.ReceiveRemoteIntervalInFrames > 1 || AllowLocalDoubleSimulation || Setup.InitialLatencyInFrames > 0;
	const bool AllowRemoteStallAdvantage = Environment.ReceiveRemoteIntervalInFrames > 1 || AllowRemoteDoubleSimulation || Setup.InitialLatencyInFrames > 0;
	const bool RoundTripPossibleWithinRollbackWindow = (size_t)Environment.ReceiveRemoteIntervalInFrames + 1 <= Config.RollbackConfiguration.MinRollbackFrameCount && Setup.InitialLatencyInFrames < Config.RollbackConfiguration.MinRollbackFrameCount;
	const bool AllowLocalStarvedForInput =
		(size_t)Environment.ReceiveRemoteIntervalInFrames + 1 + AllowRemoteStallAdvantage + AllowLocalDoubleSimulation >= Config.RollbackConfiguration.MinRollbackFrameCount ||
		AllowRemoteDoubleSimulation ||
		!RoundTripPossibleWithinRollbackWindow;
	const bool AllowRemoteStarvedForInput =
		(size_t)Environment.ReceiveRemoteIntervalInFrames + 1 + AllowLocalStallAdvantage + AllowRemoteDoubleSimulation >= Config.RollbackConfiguration.MinRollbackFrameCount ||
		AllowLocalDoubleSimulation ||
		!RoundTripPossibleWithinRollbackWindow;

	const auto RemoteStartMockIterationIndex = Setup.LocalStartFrameIndex + Setup.RemoteStartOffsetInFrames + Setup.InitialLatencyInFrames;

	TEST_NSPC_Systems::TEST_LocalMock Local(Setup);
	TEST_NSPC_Systems::TEST_RemoteMock Remote(Setup);

	for (size_t IterationIndex = 0; IterationIndex < Environment.TestDurationInFrames; ++IterationIndex)
	{
		assert(Local.PreUpdate((uint16_t)IterationIndex));
		assert(Remote.PreUpdate((uint16_t)IterationIndex));

		Local.Update
		({
			AllowLocalDoubleSimulation,
			AllowLocalStallAdvantage,
			AllowLocalStarvedForInput,
			Setup.LocalMockHardwareFrameDurationInSeconds < Config.SimulationConfiguration.FrameDurationInSeconds
		});
		Remote.Update
		({
			AllowRemoteDoubleSimulation,
			AllowRemoteStallAdvantage,
			AllowRemoteStarvedForInput,
			Setup.RemoteMockHardwareFrameDurationInSeconds < Config.SimulationConfiguration.FrameDurationInSeconds
		});

		if (IterationIndex % Environment.ReceiveRemoteIntervalInFrames == 0 && IterationIndex >= RemoteStartMockIterationIndex)
		{
			TEST_NSPC_Systems::TransferLocalPlayersInputs();
		}
	}

	TEST_NSPC_Systems::ForceResetAndCleanup();

	return true;
}
