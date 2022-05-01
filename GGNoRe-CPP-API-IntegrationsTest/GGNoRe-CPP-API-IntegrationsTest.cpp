/*
 * Copyright 2021 Loic Venerosy
 */

#include <GGNoRe-CPP-API-IntegrationsTest.hpp>

#include <TEST_Mocks.hpp>

using namespace GGNoRe::API;

namespace TEST_NSPC_Systems
{

const id_t Player1Id = 0;
const id_t Player2Id = 1;

class TEST_LocalMock final : public TEST_ABS_SystemMock
{
	const DATA_Player TrueLocalPlayer1Identity;
	const DATA_Player LocalPlayer2Identity;

	TEST_Player TrueLocalPlayer1;
	TEST_Player LocalPlayer2;

	void OnPreUpdate(const uint16_t, const TEST_CPT_State OtherPlayerInitialStateToTransfer) override
	{
		const auto FrameIndex = SystemMultiton::GetRollbackable(TrueLocalPlayer1Identity.SystemIndex).UnsimulatedFrameIndex();

		if (FrameIndex == RemoteStartFrameIndex + Setup.InitialLatencyInFrames)
		{
			if (Setup.InitialLatencyInFrames == 0 && !LocalPlayer2.Emulator().ExistsAtFrame(FrameIndex))
			{
				LocalPlayer2.ActivateNow(LocalPlayer2Identity, OtherPlayerInitialStateToTransfer);
			}
			else if (!LocalPlayer2.Emulator().ExistsAtFrame(RemoteStartFrameIndex))
			{
				LocalPlayer2.ActivateInPast(LocalPlayer2Identity, RemoteStartFrameIndex, OtherPlayerInitialStateToTransfer);
			}
		}
	}

	uint8_t SystemIndex() const override
	{
		return 0;
	}

	float DeltaDurationInSeconds() const override
	{
		return Setup.LocalMockHardwareFrameDurationInSeconds;
	}

public:
	TEST_LocalMock(const PlayersSetup Setup)
		:
		TEST_ABS_SystemMock(Setup),
		TrueLocalPlayer1Identity({ Player1Id, true, Setup.LocalStartFrameIndex, SystemIndex() }),
		LocalPlayer2Identity({ Player2Id, false, RemoteStartFrameIndex, SystemIndex() }),
		TrueLocalPlayer1(Setup.UseRandomInputs),
		LocalPlayer2(Setup.UseRandomInputs)
	{
		assert(Player1Id != Player2Id);
		assert(SystemIndexes.find(TrueLocalPlayer1Identity.SystemIndex) == SystemIndexes.cend());

		SystemMultiton::GetRollbackable(TrueLocalPlayer1Identity.SystemIndex).SyncWithRemoteFrameIndex(Setup.LocalStartFrameIndex);
		SystemIndexes.insert(TrueLocalPlayer1Identity.SystemIndex);

		TrueLocalPlayer1.ActivateNow(TrueLocalPlayer1Identity);
	}

	const TEST_Player& Player() const override
	{
		return TrueLocalPlayer1;
	}
};

class TEST_RemoteMock final : public TEST_ABS_SystemMock
{
	const DATA_Player TrueRemotePlayer2Identity;
	const DATA_Player RemotePlayer1Identity;

	TEST_Player TrueRemotePlayer2;
	TEST_Player RemotePlayer1;

	void OnPreUpdate(const uint16_t TestFrameIndex, const TEST_CPT_State OtherPlayerInitialStateToTransfer) override
	{
		if (TestFrameIndex == RemoteStartFrameIndex && !TrueRemotePlayer2.Emulator().ExistsAtFrame(TestFrameIndex))
		{
			assert(SystemIndexes.find(TrueRemotePlayer2Identity.SystemIndex) == SystemIndexes.cend());

			SystemMultiton::GetRollbackable(TrueRemotePlayer2Identity.SystemIndex).SyncWithRemoteFrameIndex(RemoteStartFrameIndex);
			SystemIndexes.insert(TrueRemotePlayer2Identity.SystemIndex);

			TrueRemotePlayer2.ActivateNow(TrueRemotePlayer2Identity);
		}

		const auto FrameIndex = SystemMultiton::GetRollbackable(TrueRemotePlayer2Identity.SystemIndex).UnsimulatedFrameIndex();
		if (FrameIndex == RemoteStartFrameIndex + Setup.InitialLatencyInFrames)
		{
			if (Setup.InitialLatencyInFrames == 0 && !RemotePlayer1.Emulator().ExistsAtFrame(FrameIndex))
			{
				RemotePlayer1.ActivateNow(RemotePlayer1Identity, OtherPlayerInitialStateToTransfer);
			}
			else if (!RemotePlayer1.Emulator().ExistsAtFrame(RemoteStartFrameIndex))
			{
				RemotePlayer1.ActivateInPast(RemotePlayer1Identity, RemoteStartFrameIndex, OtherPlayerInitialStateToTransfer);
			}
		}
	}

	uint8_t SystemIndex() const override
	{
		return 1;
	}

	float DeltaDurationInSeconds() const override
	{
		return Setup.RemoteMockHardwareFrameDurationInSeconds;
	}

public:
	TEST_RemoteMock(const PlayersSetup Setup)
		:
		TEST_ABS_SystemMock(Setup),
		TrueRemotePlayer2Identity({ Player2Id, true, RemoteStartFrameIndex, SystemIndex() }),
		RemotePlayer1Identity({ Player1Id, false, RemoteStartFrameIndex, SystemIndex() }),
		TrueRemotePlayer2(Setup.UseRandomInputs),
		RemotePlayer1(Setup.UseRandomInputs)
	{
		assert(Player1Id != Player2Id);
	}

	const TEST_Player& Player() const override
	{
		return TrueRemotePlayer2;
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

	TEST_NSPC_Systems::TEST_LocalMock Local(Setup);
	TEST_NSPC_Systems::TEST_RemoteMock Remote(Setup);

	for (size_t IterationIndex = 0; IterationIndex < Environment.TestDurationInFrames; ++IterationIndex)
	{
		assert(Local.IsRunning());

		if (IterationIndex == Setup.RemoteStartOffsetInFrames)
		{
			Local.SaveCurrentStateToTransfer();
			Remote.SaveCurrentStateToTransfer();
		}

		Local.PreUpdate((uint16_t)IterationIndex + Setup.LocalStartFrameIndex, Remote.InitialStateToTransfer());
		Remote.PreUpdate((uint16_t)IterationIndex + Setup.LocalStartFrameIndex, Local.InitialStateToTransfer());

		Local.Update
		({
			AllowLocalDoubleSimulation,
			AllowLocalStallAdvantage,
			AllowLocalStarvedForInput,
			Setup.LocalMockHardwareFrameDurationInSeconds < Config.SimulationConfiguration.FrameDurationInSeconds
		});

		if (Remote.IsRunning())
		{
			Remote.Update
			({
				AllowRemoteDoubleSimulation,
				AllowRemoteStallAdvantage,
				AllowRemoteStarvedForInput,
				Setup.RemoteMockHardwareFrameDurationInSeconds < Config.SimulationConfiguration.FrameDurationInSeconds
			});
		}

		if (IterationIndex % Environment.ReceiveRemoteIntervalInFrames == 0)
		{
			TEST_NSPC_Systems::TransferLocalPlayersInputs();
		}
	}

	TEST_NSPC_Systems::ForceResetAndCleanup();

	return true;
}
