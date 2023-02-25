/*
 * Copyright 2022 Loic Venerosy
 */

#pragma once

#include <TEST_Player.hpp>

namespace TEST_NSPC_Systems
{

std::set<uint8_t> SystemIndexes;

void TransferLocalPlayersInputs()
{
	for (auto CurrentSystemIndex : SystemIndexes)
	{
		auto LocalFrameIndex = GGNoRe::API::SystemMultiton::GetRollbackable(CurrentSystemIndex).UnsimulatedFrameIndex();

		const auto& Players = TEST_Player::Players();
		for (auto Player : Players)
		{
			if (Player->Emulator().ShouldSendInputsToTarget(CurrentSystemIndex))
			{
				TestLog("############ INPUT TRANSFER FROM PLAYER " + std::to_string(Player->Emulator().Owner().Id) + " TO SYSTEM " + std::to_string(CurrentSystemIndex) + " - FRAME " + std::to_string(LocalFrameIndex) + " ############");
				assert(GGNoRe::API::SystemMultiton::GetEmulator(CurrentSystemIndex).DownloadRemotePlayerBinary(Player->Emulator().LatestInputs().data()) == GGNoRe::API::ABS_CPT_IPT_Emulator::SINGLETON::DownloadSuccess_E::Success);
			}
		}
	}
}

void ForceResetAndCleanup()
{
	GGNoRe::API::SystemMultiton::ForceResetAndCleanup();

	SystemIndexes.clear();
}

class TEST_SystemMock final
{
public:
	struct OutcomesSanityCheck
	{
		const bool AllowDoubleSimulation = false;
		const bool AllowStallAdvantage = false;
		const bool AllowStarvedForInput = false;
		const bool AllowStayCurrent = false;
	};

private:
	const GGNoRe::API::DATA_Player ThisPlayerIdentity;
	const GGNoRe::API::DATA_Player OtherPlayerIdentity;

	TEST_Player ThisPlayer;
	TEST_Player OtherPlayer;

	const float DeltaDurationInSeconds = 0.f;

	size_t MockTickIndex = 0;
	GGNoRe::API::SER_FixedPoint UpdateTimer = 0.f;

	// The state of the other player is needed to properly initialize
	// This is an artefact of the mocking, in a real setup you would want to receive from the remote player/server what you need to initialize
	TEST_Player::TEST_CPT_State InitialStateToTransfer;
	// The inputs of the other player are needed to properly initialize
	// This is an artefact of the mocking, in a real setup you would want to receive from the remote player/server what you need to initialize
	GGNoRe::API::ABS_CPT_IPT_Emulator::SINGLETON::InputsBinaryPacketsForStartingRemote InitialInputsToTransfer;
	// Boolean flags required to work around the situation where one player simulates 2 frames and thus skips the pre-calculated transfer frame
	// This is an artefact of the mocking, in a real setup you send/receive when ready, but for testing purposes the transfer timing must be pre-determinated
	bool TransferInitialInputs = false;
	bool LoadInitialInputs = true;
	bool OtherPlayerHasBeenActivated = false;

	const PlayersSetup Setup;

public:
	TEST_SystemMock(const GGNoRe::API::DATA_Player ThisPlayerIdentity, const GGNoRe::API::DATA_Player OtherPlayerIdentity, const float DeltaDurationInSeconds, const PlayersSetup Setup)
		:ThisPlayerIdentity(ThisPlayerIdentity), OtherPlayerIdentity(OtherPlayerIdentity), DeltaDurationInSeconds(DeltaDurationInSeconds), Setup(Setup)
	{
		assert(ThisPlayerIdentity.Local);
		assert(!OtherPlayerIdentity.Local);
		assert(ThisPlayerIdentity.Id != OtherPlayerIdentity.Id);
		assert(DeltaDurationInSeconds > 0.f);
	}

	~TEST_SystemMock() = default;

	inline bool IsRunning() const
	{
		return ThisPlayer.Emulator().ExistsAtFrame(GGNoRe::API::SystemMultiton::GetRollbackable(ThisPlayerIdentity.SystemIndex).UnsimulatedFrameIndex());
	}

	void PreUpdate(const uint16_t TestFrameIndex, const TEST_SystemMock& OtherSystem)
	{
		if (!IsRunning() && TestFrameIndex == ThisPlayerIdentity.JoinFrameIndex)
		{
			assert(!ThisPlayer.Emulator().ExistsAtFrame(TestFrameIndex));
			assert(SystemIndexes.find(ThisPlayerIdentity.SystemIndex) == SystemIndexes.cend());

			GGNoRe::API::SystemMultiton::GetRollbackable(ThisPlayerIdentity.SystemIndex).SyncWithRemoteFrameIndex(ThisPlayerIdentity.JoinFrameIndex);
			SystemIndexes.insert(ThisPlayerIdentity.SystemIndex);

			try
			{
				if (TestFrameIndex == OtherPlayerIdentity.JoinFrameIndex)
				{
					assert(!OtherPlayer.Emulator().ExistsAtFrame(TestFrameIndex));
					assert(!OtherPlayerHasBeenActivated);

					if (ThisPlayerIdentity.Id < OtherPlayerIdentity.Id)
					{
						ThisPlayer.ActivateNow(ThisPlayerIdentity);
						OtherPlayer.ActivateNow(OtherPlayerIdentity, OtherSystem.ThisPlayer.State());
						OtherPlayerHasBeenActivated = true;
					}
					else
					{
						OtherPlayer.ActivateNow(OtherPlayerIdentity, OtherSystem.ThisPlayer.State());
						OtherPlayerHasBeenActivated = true;
						ThisPlayer.ActivateNow(ThisPlayerIdentity);
					}
				}
				else
				{
					ThisPlayer.ActivateNow(ThisPlayerIdentity);
				}
			}
			catch (const GGNoRe::API::I_RB_Rollbackable::RegisterSuccess_E&)
			{
				assert(false);
			}
		}

		if (TestFrameIndex == OtherPlayerIdentity.JoinFrameIndex)
		{
			InitialStateToTransfer = ThisPlayer.State();
		}
	}

	void Update(const OutcomesSanityCheck AllowedOutcomes, const TEST_SystemMock& OtherSystem)
	{
		assert(IsRunning());

		const auto FrameIndex = GGNoRe::API::SystemMultiton::GetRollbackable(ThisPlayerIdentity.SystemIndex).UnsimulatedFrameIndex();

		if (!OtherPlayerHasBeenActivated && FrameIndex >= OtherPlayerIdentity.JoinFrameIndex)
		{
			assert(ThisPlayer.Emulator().ExistsAtFrame(FrameIndex));
			assert(OtherPlayerIdentity.JoinFrameIndex > ThisPlayerIdentity.JoinFrameIndex);

			try
			{
				if (OtherPlayerIdentity.JoinFrameIndex == FrameIndex)
				{
					OtherPlayer.ActivateNow(OtherPlayerIdentity, OtherSystem.ThisPlayer.State());
				}
				else
				{
					OtherPlayer.ActivateInPast(OtherPlayerIdentity, OtherPlayerIdentity.JoinFrameIndex, OtherSystem.InitialStateToTransfer);
				}

				OtherPlayerHasBeenActivated = true;
			}
			catch (const GGNoRe::API::I_RB_Rollbackable::RegisterSuccess_E&)
			{
				assert(false);
			}
		}

		bool ReadyForNextFrame = false;

		GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::TickHistory History;

		while (!ReadyForNextFrame)
		{
			TestLog("____________ SYSTEM " + std::to_string(ThisPlayerIdentity.SystemIndex) + " START - TICK " + std::to_string(MockTickIndex) + " ____________");

			History.DeltaDurationInSeconds = DeltaDurationInSeconds;
			assert(History.DeltaDurationInSeconds > 0.f);

			auto& Rollbackable = GGNoRe::API::SystemMultiton::GetRollbackable(ThisPlayerIdentity.SystemIndex);

			auto Plan = Rollbackable.PreSimulation(History);
			assert((Plan.TickSuccess != GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::SimulationPlan::TickSuccess_E::DoubleSimulation));
			assert((Plan.TickSuccess != GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::SimulationPlan::TickSuccess_E::NoActiveEmulator));

			// Your main loop should start here
			{
				auto& Simulator = GGNoRe::API::SystemMultiton::GetSimulator(ThisPlayerIdentity.SystemIndex);
				assert(Simulator.Simulation().Stage == GGNoRe::API::I_RB_Rollbackable::SimulationStage_E::Neither);

				auto& Emulator = GGNoRe::API::SystemMultiton::GetEmulator(ThisPlayerIdentity.SystemIndex);

				const auto SimulateTick = [this, &Simulator, &Rollbackable](const GGNoRe::API::SER_FixedPoint DeltaDurationInSeconds, const GGNoRe::API::SER_FixedPoint DeltaDurationInSecondsConsumedPreActivationChange, const uint16_t FrameIndex)
				{
					assert(DeltaDurationInSeconds + DeltaDurationInSecondsConsumedPreActivationChange > 0.f);

					Simulator.SimulateTick(DeltaDurationInSeconds, FrameIndex);

					Rollbackable.PostTick(FrameIndex, DeltaDurationInSecondsConsumedPreActivationChange);
				};

				const auto AdvanceToNextFrame = [this, &Emulator, &Simulator, &Rollbackable](const uint16_t FrameIndex)
				{
					assert(FrameIndex <= Rollbackable.UnsimulatedFrameIndex());

					Simulator.SimulateFrame(FrameIndex, Emulator.GetPlayerIdToInputsAtFrame(FrameIndex));
				};

				const uint16_t ResimulationFramesCount = uint16_t(Plan.SimulationFramesCount - (Plan.TickSuccess == GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::SimulationPlan::TickSuccess_E::ToNext));
				if (ResimulationFramesCount > 0)
				{
					const uint16_t RollbackFrameIndex = Rollbackable.UnsimulatedFrameIndex() - ResimulationFramesCount;

					for (auto FrameOffset = 0; FrameOffset < ResimulationFramesCount; FrameOffset++)
					{
						const auto ExistingFrameIndex = RollbackFrameIndex + FrameOffset;

						assert(ExistingFrameIndex < Rollbackable.UnsimulatedFrameIndex());

						SimulateTick(GGNoRe::API::DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds, 0.f, ExistingFrameIndex);

						AdvanceToNextFrame(ExistingFrameIndex);

						Rollbackable.PostResimulationFrame(ExistingFrameIndex, Plan.MostRecentValidFrameIndex);
					}

					if (
						History.ConsumedDeltaDurationInSecondsFromFrameStart > 0.f &&
						Plan.TickSuccess != GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::SimulationPlan::TickSuccess_E::StallAdvantage &&
						Plan.TickSuccess != GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::SimulationPlan::TickSuccess_E::StarvedForInput
						)
					{
						SimulateTick(History.ConsumedDeltaDurationInSecondsFromFrameStart, 0.f, Rollbackable.UnsimulatedFrameIndex());
					}
				}

				if (Plan.TickSuccess == GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::SimulationPlan::TickSuccess_E::StayCurrent)
				{
					SimulateTick(History.DeltaDurationInSeconds, History.ConsumedDeltaDurationInSecondsFromFrameStart, Rollbackable.UnsimulatedFrameIndex());
				}
				else if (Plan.TickSuccess == GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::SimulationPlan::TickSuccess_E::ToNext)
				{
					const auto SimulateNewFrame = [this, &SimulateTick, &AdvanceToNextFrame, &Rollbackable](const GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::TickHistory History)
					{
						const GGNoRe::API::SER_FixedPoint DeltaToNextFrame = GGNoRe::API::DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds - History.ConsumedDeltaDurationInSecondsFromFrameStart;
						assert(DeltaToNextFrame >= 0.f);
						SimulateTick(DeltaToNextFrame, History.ConsumedDeltaDurationInSecondsFromFrameStart, Rollbackable.UnsimulatedFrameIndex());

						AdvanceToNextFrame(Rollbackable.UnsimulatedFrameIndex());
					};

					SimulateNewFrame(History);

					Plan = Rollbackable.PostNewFrame(Plan);

					// The original plan describes how to simulate until here, then you may use the updated plan to simulate one more frame if the local client has enough excess delta time
					// In case you cannot partition your simulation, for example if you have to know how many frames to simulate before starting the main loop,
					// you can just use the original plan and ignore the double simulation and setting AllowDoubleSimulation to false
					if (Plan.TickSuccess == GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::SimulationPlan::TickSuccess_E::DoubleSimulation)
					{
						assert(GGNoRe::API::DATA_CFG::Get().SimulationConfiguration.AllowDoubleSimulation);

						SimulateNewFrame({});

						Plan = Rollbackable.PostNewFrame(Plan);
					}
				}
			}
			// Your main loop should end here

			// Use this call instead of the main loop example in order to trigger internal asserts/logs
			//Plan = Rollbackable.TryTickingToNextFrame(History, Plan);

			Rollbackable.PostSimulation(Plan);

			if (Plan.TickSuccess == GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::SimulationPlan::TickSuccess_E::StayCurrent)
			{
				History.ConsumedDeltaDurationInSecondsFromFrameStart += DeltaDurationInSeconds;
			}
			else
			{
				History.ConsumedDeltaDurationInSecondsFromFrameStart = 0.f;
			}

			switch (Plan.TickSuccess)
			{
			case GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::SimulationPlan::TickSuccess_E::DoubleSimulation:
				TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(ThisPlayerIdentity.SystemIndex) + " DOUBLE - TICK " + std::to_string(MockTickIndex) + " ^^^^^^^^^^^^");
				assert(AllowedOutcomes.AllowDoubleSimulation);
				break;
			case GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::SimulationPlan::TickSuccess_E::NoActiveEmulator:
				TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(ThisPlayerIdentity.SystemIndex) + " NO EMULATOR - TICK " + std::to_string(MockTickIndex) + " ^^^^^^^^^^^^");
				// IMPORTANT: there must always be at least one active emulator
				// If you want, for example, to keep an empty server running, it should have its own emulator
				assert(false);
				break;
			case GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::SimulationPlan::TickSuccess_E::StallAdvantage:
				TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(ThisPlayerIdentity.SystemIndex) + " STALLING - TICK " + std::to_string(MockTickIndex) + " ^^^^^^^^^^^^");
				assert(AllowedOutcomes.AllowStallAdvantage);
				break;
			case GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::SimulationPlan::TickSuccess_E::StarvedForInput:
				TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(ThisPlayerIdentity.SystemIndex) + " STARVED - TICK " + std::to_string(MockTickIndex) + " ^^^^^^^^^^^^");
				assert(AllowedOutcomes.AllowStarvedForInput);
				break;
			case GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::SimulationPlan::TickSuccess_E::StayCurrent:
				TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(ThisPlayerIdentity.SystemIndex) + " STAY - TICK " + std::to_string(MockTickIndex) + " ^^^^^^^^^^^^");
				assert(AllowedOutcomes.AllowStayCurrent);
				break;
			case GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::SimulationPlan::TickSuccess_E::ToNext:
				TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(ThisPlayerIdentity.SystemIndex) + " NEXT - TICK " + std::to_string(MockTickIndex) + " ^^^^^^^^^^^^");
				break;
			default:
				assert(false);
				break;
			}

			++MockTickIndex;

			UpdateTimer += DeltaDurationInSeconds;
			if (UpdateTimer >= GGNoRe::API::DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds)
			{
				UpdateTimer -= GGNoRe::API::DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds;
				ReadyForNextFrame = true;
			}
		}

		// + 1 because should happen post TryTickingToNextFrame
		if (!TransferInitialInputs && GGNoRe::API::SystemMultiton::GetRollbackable(ThisPlayerIdentity.SystemIndex).UnsimulatedFrameIndex() >= OtherPlayerIdentity.JoinFrameIndex + 1)
		{
			InitialInputsToTransfer = GGNoRe::API::SystemMultiton::GetEmulator(ThisPlayerIdentity.SystemIndex).UploadInputsFromRemoteStartFrameIndex(OtherPlayerIdentity.JoinFrameIndex);
			assert(InitialInputsToTransfer.UploadSuccess == GGNoRe::API::ABS_CPT_IPT_Emulator::SINGLETON::InputsBinaryPacketsForStartingRemote::UploadSuccess_E::Success);
			TransferInitialInputs = true;
		}
	}

	void PostUpdate(const uint16_t TestFrameIndex, const TEST_SystemMock& OtherSystem)
	{
		if (LoadInitialInputs && OtherSystem.TransferInitialInputs && OtherPlayerHasBeenActivated)
		{
			assert(OtherSystem.InitialInputsToTransfer.UploadSuccess == GGNoRe::API::ABS_CPT_IPT_Emulator::SINGLETON::InputsBinaryPacketsForStartingRemote::UploadSuccess_E::Success);

			LoadInitialInputs = false;

			for (const auto& OtherPlayerBinary : OtherSystem.InitialInputsToTransfer.InputsBinaryPackets)
			{
				TestLog("############ INITIAL INPUT TRANSFER FROM PLAYER " + std::to_string(OtherSystem.ThisPlayerIdentity.Id) + " TO SYSTEM " + std::to_string(ThisPlayerIdentity.SystemIndex) + " ############");
				assert(GGNoRe::API::SystemMultiton::GetEmulator(ThisPlayerIdentity.SystemIndex).DownloadRemotePlayerBinary(OtherPlayerBinary.data()) == GGNoRe::API::ABS_CPT_IPT_Emulator::SINGLETON::DownloadSuccess_E::Success);
			}
		}
	}
};

}