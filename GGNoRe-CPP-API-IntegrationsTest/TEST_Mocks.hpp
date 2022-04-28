/*
 * Copyright 2022 Loic Venerosy
 */

#pragma once

#include <TEST_Components.hpp>

 // This is a player class grouping the 3 components for simplicity's sake
 // In your game it could be a fireball with only save states and a simulator, or a player input controller with only an emulator
class TEST_Player final
{
	static std::set<TEST_Player*> PlayersInternal;
	static uint32_t DebugIdCounter;

	// To identify more quickly which is which when debugging
	uint32_t DebugId = 0;

	TEST_CPT_State StateInternal;

	TEST_CPT_IPT_Emulator EmulatorInternal;
	TEST_CPT_RB_SaveStates SaveStatesInternal;
	TEST_CPT_RB_Simulator SimulatorInternal;

public:
	explicit TEST_Player(const bool UseRandomInputs)
		:EmulatorInternal(UseRandomInputs), SaveStatesInternal(StateInternal), SimulatorInternal(StateInternal)
	{
	}

	~TEST_Player()
	{
		PlayersInternal.erase(this);
	}

	static inline const std::set<TEST_Player*>& Players()
	{
		return PlayersInternal;
	}

	inline const TEST_CPT_IPT_Emulator& Emulator() const
	{
		return EmulatorInternal;
	}

	inline const TEST_CPT_State& State() const
	{
		return StateInternal;
	}

	void ActivateNow(const GGNoRe::API::DATA_Player Owner)
	{
		assert(PlayersInternal.find(this) == PlayersInternal.cend());

		DebugId = DebugIdCounter++;

		PlayersInternal.insert(this);

		EmulatorInternal.ChangeActivationNow(Owner, GGNoRe::API::I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate);
		SaveStatesInternal.ChangeActivationNow(Owner, GGNoRe::API::I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate);
		SimulatorInternal.ChangeActivationNow(Owner, GGNoRe::API::I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate);
	}

	void ActivateInPast(const GGNoRe::API::DATA_Player Owner, const uint16_t StartFrameIndex)
	{
		assert(PlayersInternal.find(this) == PlayersInternal.cend());

		DebugId = DebugIdCounter++;

		PlayersInternal.insert(this);

		EmulatorInternal.ChangeActivationInPast({ GGNoRe::API::I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate, Owner, StartFrameIndex });
		SaveStatesInternal.ChangeActivationInPast({ GGNoRe::API::I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate, Owner, StartFrameIndex });
		SimulatorInternal.ChangeActivationInPast({ GGNoRe::API::I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate, Owner, StartFrameIndex });
	}
};

std::set<TEST_Player*> TEST_Player::PlayersInternal;
uint32_t TEST_Player::DebugIdCounter = 0;

namespace TEST_NSPC_Systems
{

std::set<uint8_t> SystemIndexes;

void TransferLocalPlayersInputs()
{
	for (auto SystemIndex : SystemIndexes)
	{
		auto CurrentFrameIndex = GGNoRe::API::SystemMultiton::GetRollbackable(SystemIndex).UnsimulatedFrameIndex();

		const auto& Players = TEST_Player::Players();
		for (auto Player : Players)
		{
			if (Player->Emulator().ShouldSendInputsToTarget(SystemIndex))
			{
				TestLog("############ INPUT TRANSFER FROM PLAYER " + std::to_string(Player->Emulator().Owner().Id) + " TO SYSTEM " + std::to_string(SystemIndex) + " - FRAME " + std::to_string(CurrentFrameIndex) + " ############");
				assert(GGNoRe::API::SystemMultiton::GetEmulator(SystemIndex).DownloadRemotePlayerBinary(Player->Emulator().LatestInputs().data()) == GGNoRe::API::ABS_CPT_IPT_Emulator::SINGLETON::DownloadSuccess_E::Success);
			}
		}
	}
}

void ForceResetAndCleanup()
{
	for (auto SystemIndex : SystemIndexes)
	{
		GGNoRe::API::SystemMultiton::GetRollbackable(SystemIndex).ForceResetAndCleanup();
	}

	SystemIndexes.clear();
}

class TEST_ABS_SystemMock
{
	size_t MockTickIndex = 0;
	float UpdateTimer = 0.f;

public:
	struct OutcomesSanityCheck
	{
		const bool AllowDoubleSimulation = false;
		const bool AllowStallAdvantage = false;
		const bool AllowStarvedForInput = false;
		const bool AllowStayCurrent = false;
	};

protected:
	const PlayersSetup Setup;
	const uint16_t RemoteStartFrameIndex = 0;

	TEST_ABS_SystemMock(const PlayersSetup Setup)
		:Setup(Setup), RemoteStartFrameIndex(Setup.LocalStartFrameIndex + Setup.RemoteStartOffsetInFrames)
	{}

	virtual void OnPreUpdate(const uint16_t TestFrameIndex, const TEST_CPT_State OtherPlayerState) = 0;

	virtual uint8_t SystemIndex() const = 0;

	virtual float DeltaDurationInSeconds() const = 0;

public:
	virtual ~TEST_ABS_SystemMock() = default;

	bool IsRunning() const
	{
		return Player().Emulator().ExistsAtFrame(GGNoRe::API::SystemMultiton::GetRollbackable(SystemIndex()).UnsimulatedFrameIndex());
	}

	// The state of the other player is needed to properly initialize
	void PreUpdate(const uint16_t TestFrameIndex, const TEST_CPT_State OtherPlayerState)
	{
		try
		{
			OnPreUpdate(TestFrameIndex, OtherPlayerState);
		}
		catch (const GGNoRe::API::I_RB_Rollbackable::RegisterSuccess_E& Success)
		{
			// Basically illegal activation, in a real use you first would make sure you can activate before actually activating
			assert(Success == GGNoRe::API::I_RB_Rollbackable::RegisterSuccess_E::UnreachablePastFrame && Setup.InitialLatencyInFrames > GGNoRe::API::DATA_CFG::Get().RollbackFrameCount());
		}
	}

	void Update(const OutcomesSanityCheck AllowedOutcomes)
	{
		assert(IsRunning());

		bool ReadyForNextFrame = false;

		while (!ReadyForNextFrame)
		{
			TestLog("____________ SYSTEM " + std::to_string(SystemIndex()) + " START - TICK " + std::to_string(MockTickIndex) + " ____________");

			auto Success = GGNoRe::API::SystemMultiton::GetRollbackable(SystemIndex()).TryTickingToNextFrame(DeltaDurationInSeconds());

			switch (Success)
			{
			case GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::DoubleSimulation:
				TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex()) + " DOUBLE - TICK " + std::to_string(MockTickIndex) + " ^^^^^^^^^^^^");
				assert(AllowedOutcomes.AllowDoubleSimulation);
				break;
			case GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::FailedInitialization:
				TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex()) + " FAILURE - TICK " + std::to_string(MockTickIndex) + " ^^^^^^^^^^^^");
				assert(false);
			break;
			case GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::NoActiveEmulator:
				TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex()) + " NO EMULATOR - TICK " + std::to_string(MockTickIndex) + " ^^^^^^^^^^^^");
				assert(false);
			break;
			case GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::StallAdvantage:
				TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex()) + " STALLING - TICK " + std::to_string(MockTickIndex) + " ^^^^^^^^^^^^");
				assert(AllowedOutcomes.AllowStallAdvantage);
				break;
			case GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::StarvedForInput:
				TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex()) + " STARVED - TICK " + std::to_string(MockTickIndex) + " ^^^^^^^^^^^^");
				assert(AllowedOutcomes.AllowStarvedForInput);
				break;
			case GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::StayCurrent:
				TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex()) + " STAY - TICK " + std::to_string(MockTickIndex) + " ^^^^^^^^^^^^");
				assert(AllowedOutcomes.AllowStayCurrent);
				break;
			case GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::ToNext:
				TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex()) + " NEXT - TICK " + std::to_string(MockTickIndex) + " ^^^^^^^^^^^^");
				break;
			default:
				assert(false);
				break;
			}

			++MockTickIndex;

			UpdateTimer += DeltaDurationInSeconds();
			if (UpdateTimer >= GGNoRe::API::DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds)
			{
				UpdateTimer -= GGNoRe::API::DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds;
				ReadyForNextFrame = true;
			}
		}
	}

	virtual const TEST_Player& Player() const = 0;
};

}