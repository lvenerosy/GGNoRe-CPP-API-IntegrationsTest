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

	// The state of the other player are needed to properly initialize
	// This is an artefact of the mocking, in a real setup you would want to receive from the remote player/server what you need to initialize
	TEST_Player::TEST_CPT_State InitialStateToTransfer;
	// The inputs of the other player are needed to properly initialize
	// This is an artefact of the mocking, in a real setup you would want to receive from the remote player/server what you need to initialize
	GGNoRe::API::ABS_CPT_IPT_Emulator::SINGLETON::InputsBinaryPacketsForStartingRemote InitialInputsToTransfer;
	// Boolean flags required to work around the situation where one player simulates 2 frames and thus skips the pre-calculated transfer frame
	// This is an artefact of the mocking, in a real setup you send/receive when ready, but for testing purposes the transfer timing must be pre-determinated
	bool TransferInitialState = false;
	bool LoadInitialState = true;
	bool TransferInitialInputs = false;
	bool LoadInitialInputs = true;

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

	void PreUpdate(const uint16_t TestFrameIndex)
	{
		try
		{
			if (TestFrameIndex == ThisPlayerIdentity.JoinFrameIndex && !ThisPlayer.Emulator().ExistsAtFrame(TestFrameIndex))
			{
				assert(SystemIndexes.find(ThisPlayerIdentity.SystemIndex) == SystemIndexes.cend());

				GGNoRe::API::SystemMultiton::GetRollbackable(ThisPlayerIdentity.SystemIndex).SyncWithRemoteFrameIndex(ThisPlayerIdentity.JoinFrameIndex);
				SystemIndexes.insert(ThisPlayerIdentity.SystemIndex);

				ThisPlayer.ActivateNow(ThisPlayerIdentity);
			}

			if (!TransferInitialState && TestFrameIndex >= OtherPlayerIdentity.JoinFrameIndex)
			{
				InitialStateToTransfer = ThisPlayer.State();
				TransferInitialState = true;
			}
		}
		catch (const GGNoRe::API::I_RB_Rollbackable::RegisterSuccess_E& Success)
		{
			// Basically illegal activation, in a real use you first would make sure you can activate before actually activating
			assert(Success == GGNoRe::API::I_RB_Rollbackable::RegisterSuccess_E::UnreachablePastFrame && Setup.InitialLatencyInFrames > GGNoRe::API::DATA_CFG::Get().RollbackFrameCount());
		}
	}

	void Update(const OutcomesSanityCheck AllowedOutcomes, const TEST_SystemMock& OtherSystem)
	{
		assert(IsRunning());

		try
		{
			const auto FrameIndex = GGNoRe::API::SystemMultiton::GetRollbackable(ThisPlayerIdentity.SystemIndex).UnsimulatedFrameIndex();
			// For testing ActivateInPast, in case of a player joining you would want to schedule the activation to a future frame according to the ping
			// One use case for ActivateInPast is sensitive server authoritative real time activations that you don't want triggerable through user inputs in order to avoid cheats for example
			if (LoadInitialState && OtherSystem.TransferInitialState && FrameIndex >= OtherPlayerIdentity.JoinFrameIndex + Setup.InitialLatencyInFrames)
			{
				LoadInitialState = false;

				if (Setup.InitialLatencyInFrames == 0 && FrameIndex == OtherPlayerIdentity.JoinFrameIndex && !OtherPlayer.Emulator().ExistsAtFrame(FrameIndex))
				{
					OtherPlayer.ActivateNow(OtherPlayerIdentity, OtherSystem.InitialStateToTransfer);
				}
				else if (!OtherPlayer.Emulator().ExistsAtFrame(OtherPlayerIdentity.JoinFrameIndex))
				{
					OtherPlayer.ActivateInPast(OtherPlayerIdentity, OtherPlayerIdentity.JoinFrameIndex, OtherSystem.InitialStateToTransfer);
				}
			}
		}
		catch (const GGNoRe::API::I_RB_Rollbackable::RegisterSuccess_E& Success)
		{
			// Basically illegal activation, in a real use you first would make sure you can activate before actually activating
			assert(Success == GGNoRe::API::I_RB_Rollbackable::RegisterSuccess_E::UnreachablePastFrame && Setup.InitialLatencyInFrames > GGNoRe::API::DATA_CFG::Get().RollbackFrameCount());
		}

		bool ReadyForNextFrame = false;

		while (!ReadyForNextFrame)
		{
			TestLog("____________ SYSTEM " + std::to_string(ThisPlayerIdentity.SystemIndex) + " START - TICK " + std::to_string(MockTickIndex) + " ____________");

			auto Success = GGNoRe::API::SystemMultiton::GetRollbackable(ThisPlayerIdentity.SystemIndex).TryTickingToNextFrame(DeltaDurationInSeconds);

			switch (Success)
			{
			case GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::DoubleSimulation:
				TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(ThisPlayerIdentity.SystemIndex) + " DOUBLE - TICK " + std::to_string(MockTickIndex) + " ^^^^^^^^^^^^");
				assert(AllowedOutcomes.AllowDoubleSimulation);
				break;
			case GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::NoActiveEmulator:
				TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(ThisPlayerIdentity.SystemIndex) + " NO EMULATOR - TICK " + std::to_string(MockTickIndex) + " ^^^^^^^^^^^^");
				assert(false);
				break;
			case GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::StallAdvantage:
				TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(ThisPlayerIdentity.SystemIndex) + " STALLING - TICK " + std::to_string(MockTickIndex) + " ^^^^^^^^^^^^");
				assert(AllowedOutcomes.AllowStallAdvantage);
				break;
			case GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::StarvedForInput:
				TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(ThisPlayerIdentity.SystemIndex) + " STARVED - TICK " + std::to_string(MockTickIndex) + " ^^^^^^^^^^^^");
				assert(AllowedOutcomes.AllowStarvedForInput);
				break;
			case GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::StayCurrent:
				TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(ThisPlayerIdentity.SystemIndex) + " STAY - TICK " + std::to_string(MockTickIndex) + " ^^^^^^^^^^^^");
				assert(AllowedOutcomes.AllowStayCurrent);
				break;
			case GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::ToNext:
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
			assert(IsRunning());
			InitialInputsToTransfer = GGNoRe::API::SystemMultiton::GetEmulator(ThisPlayerIdentity.SystemIndex).UploadInputsFromRemoteStartFrameIndex(OtherPlayerIdentity.JoinFrameIndex);
			assert(InitialInputsToTransfer.UploadSuccess == GGNoRe::API::ABS_CPT_IPT_Emulator::SINGLETON::InputsBinaryPacketsForStartingRemote::UploadSuccess_E::Success);
			TransferInitialInputs = true;
		}
	}

	void PostUpdate(const uint16_t TestFrameIndex, const TEST_SystemMock& OtherSystem)
	{
		const auto FrameIndex = GGNoRe::API::SystemMultiton::GetRollbackable(ThisPlayerIdentity.SystemIndex).UnsimulatedFrameIndex();
		if (LoadInitialInputs && OtherSystem.TransferInitialInputs && !LoadInitialState)
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