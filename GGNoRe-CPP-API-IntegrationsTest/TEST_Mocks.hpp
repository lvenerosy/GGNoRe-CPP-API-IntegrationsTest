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

	uint32_t DebugId = 0;

	TEST_CPT_State State;

	TEST_CPT_IPT_Emulator EmulatorInternal;
	TEST_CPT_RB_SaveStates SaveStatesInternal;
	TEST_CPT_RB_Simulator SimulatorInternal;

public:
	explicit TEST_Player(const bool UseRandomInputs)
		:EmulatorInternal(UseRandomInputs), SaveStatesInternal(State), SimulatorInternal(State)
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

static void TransferLocalPlayersInputs()
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

static void ForceResetAndCleanup()
{
	for (auto SystemIndex : SystemIndexes)
	{
		GGNoRe::API::SystemMultiton::GetRollbackable(SystemIndex).ForceResetAndCleanup();
	}

	SystemIndexes.clear();
}

class TEST_ABS_SystemMock
{
	size_t MockIterationIndex = 0;
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
		:Setup(Setup),
		RemoteStartFrameIndex(Setup.LocalStartFrameIndex + Setup.RemoteStartOffsetInFrames)
	{}

	bool Tick(float DeltaDurationInSeconds, const uint8_t SystemIndex, const OutcomesSanityCheck AllowedOutcomes)
	{
		TestLog("____________ SYSTEM " + std::to_string(SystemIndex) + " START - ITERATION " + std::to_string(MockIterationIndex) + " ____________");

		auto Success = GGNoRe::API::SystemMultiton::GetRollbackable(SystemIndex).TryTickingToNextFrame(DeltaDurationInSeconds);

		switch (Success)
		{
		case GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::DoubleSimulation:
			TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex) + " DOUBLE - ITERATION " + std::to_string(MockIterationIndex) + " ^^^^^^^^^^^^");
			assert(AllowedOutcomes.AllowDoubleSimulation);
			break;
		case GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::StallAdvantage:
			TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex) + " STALLING - ITERATION " + std::to_string(MockIterationIndex) + " ^^^^^^^^^^^^");
			assert(AllowedOutcomes.AllowStallAdvantage);
			break;
		case GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::StarvedForInput:
			TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex) + " STARVED - ITERATION " + std::to_string(MockIterationIndex) + " ^^^^^^^^^^^^");
			assert(AllowedOutcomes.AllowStarvedForInput);
			break;
		case GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::StayCurrent:
			TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex) + " STAY - ITERATION " + std::to_string(MockIterationIndex) + " ^^^^^^^^^^^^");
			assert(AllowedOutcomes.AllowStayCurrent);
			break;
		case GGNoRe::API::ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::ToNext:
			TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex) + " NEXT - ITERATION " + std::to_string(MockIterationIndex) + " ^^^^^^^^^^^^");
			break;
		default:
			assert(false);
			break;
		}

		++MockIterationIndex;

		UpdateTimer += DeltaDurationInSeconds;
		if (UpdateTimer >= GGNoRe::API::DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds)
		{
			UpdateTimer -= GGNoRe::API::DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds;
			return true;
		}
		else
		{
			return false;
		}
	}

public:
	virtual bool PreUpdate(const uint16_t FrameIndex) = 0;
	virtual void Update(const OutcomesSanityCheck AllowedOutcomes) = 0;
};

}