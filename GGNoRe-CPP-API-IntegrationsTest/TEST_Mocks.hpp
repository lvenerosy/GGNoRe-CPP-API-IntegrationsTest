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

	void OnActivateNow(const GGNoRe::API::DATA_Player Owner)
	{
		assert(PlayersInternal.find(this) == PlayersInternal.cend());

		DebugId = DebugIdCounter++;

		PlayersInternal.insert(this);

		EmulatorInternal.ChangeActivationNow(Owner, GGNoRe::API::I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate);
		SaveStatesInternal.ChangeActivationNow(Owner, GGNoRe::API::I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate);
		SimulatorInternal.ChangeActivationNow(Owner, GGNoRe::API::I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate);
	}

	void OnActivateInPast(const GGNoRe::API::DATA_Player Owner, const uint16_t StartFrameIndex)
	{
		assert(PlayersInternal.find(this) == PlayersInternal.cend());

		DebugId = DebugIdCounter++;

		PlayersInternal.insert(this);

		EmulatorInternal.ChangeActivationInPast({ GGNoRe::API::I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate, Owner, StartFrameIndex });
		SaveStatesInternal.ChangeActivationInPast({ GGNoRe::API::I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate, Owner, StartFrameIndex });
		SimulatorInternal.ChangeActivationInPast({ GGNoRe::API::I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate, Owner, StartFrameIndex });
	}

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

	inline const TEST_CPT_State& State() const
	{
		return StateInternal;
	}

	inline const TEST_CPT_IPT_Emulator& Emulator() const
	{
		return EmulatorInternal;
	}

	void ActivateNow(const GGNoRe::API::DATA_Player Owner)
	{
		assert(Owner.Local);

		OnActivateNow(Owner);
	}

	void ActivateNow(const GGNoRe::API::DATA_Player Owner, const TEST_CPT_State InitialState)
	{
		assert(!Owner.Local);

		StateInternal = InitialState;

		OnActivateNow(Owner);
	}

	void ActivateInPast(const GGNoRe::API::DATA_Player Owner, const uint16_t StartFrameIndex)
	{
		assert(Owner.Local);

		OnActivateInPast(Owner, StartFrameIndex);
	}

	void ActivateInPast(const GGNoRe::API::DATA_Player Owner, const uint16_t StartFrameIndex, const TEST_CPT_State InitialState)
	{
		assert(!Owner.Local);

		StateInternal = InitialState;

		OnActivateInPast(Owner, StartFrameIndex);
	}
};

std::set<TEST_Player*> TEST_Player::PlayersInternal;
uint32_t TEST_Player::DebugIdCounter = 0;

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
	for (auto SystemIndex : SystemIndexes)
	{
		GGNoRe::API::SystemMultiton::GetRollbackable(SystemIndex).ForceResetAndCleanup();
	}

	SystemIndexes.clear();
}

class TEST_ABS_SystemMock
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
	size_t MockTickIndex = 0;
	float UpdateTimer = 0.f;

	// The state and inputs of the other player are needed to properly initialize
	// This is a shortcut, in a real setup you would want to receive from the remote player/server what you need to initialize
	TEST_CPT_State InitialStateToTransferInternal;
	GGNoRe::API::ABS_CPT_IPT_Emulator::SINGLETON::InputsBinaryPacketsForStartingRemote InitialInputsToTransferInternal;

protected:
	const PlayersSetup Setup;
	const uint16_t RemoteStartFrameIndex = 0;

	TEST_ABS_SystemMock(const PlayersSetup Setup)
		:Setup(Setup), RemoteStartFrameIndex(Setup.LocalStartFrameIndex + Setup.RemoteStartOffsetInFrames)
	{}

	virtual void OnPreUpdate(const uint16_t TestFrameIndex, const TEST_CPT_State OtherPlayerInitialState) = 0;

	virtual void OnPostUpdate(const uint16_t TestFrameIndex, const GGNoRe::API::ABS_CPT_IPT_Emulator::SINGLETON::InputsBinaryPacketsForStartingRemote OtherPlayerInitialInputs) = 0;

	virtual uint8_t SystemIndex() const = 0;

	virtual float DeltaDurationInSeconds() const = 0;

public:
	virtual ~TEST_ABS_SystemMock() = default;

	inline TEST_CPT_State InitialStateToTransfer() const
	{
		return InitialStateToTransferInternal;
	}

	inline GGNoRe::API::ABS_CPT_IPT_Emulator::SINGLETON::InputsBinaryPacketsForStartingRemote InitialInputsToTransfer() const
	{
		return InitialInputsToTransferInternal;
	}

	inline bool IsRunning() const
	{
		return Player().Emulator().ExistsAtFrame(GGNoRe::API::SystemMultiton::GetRollbackable(SystemIndex()).UnsimulatedFrameIndex());
	}

	inline void SaveInitialStateToTransfer() noexcept
	{
		InitialStateToTransferInternal = Player().State();
	}

	void PreUpdate(const uint16_t TestFrameIndex, const TEST_CPT_State OtherPlayerInitialState)
	{
		try
		{
			OnPreUpdate(TestFrameIndex, OtherPlayerInitialState);
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

		// + 1 because should happen post TryTickingToNextFrame
		if (GGNoRe::API::SystemMultiton::GetRollbackable(SystemIndex()).UnsimulatedFrameIndex() == RemoteStartFrameIndex + 1)
		{
			assert(IsRunning());
			InitialInputsToTransferInternal = GGNoRe::API::SystemMultiton::GetEmulator(SystemIndex()).UploadInputsFromRemoteStartFrameIndex(RemoteStartFrameIndex);
			assert(InitialInputsToTransferInternal.UploadSuccess == GGNoRe::API::ABS_CPT_IPT_Emulator::SINGLETON::InputsBinaryPacketsForStartingRemote::UploadSuccess_E::Success);
		}
	}

	void PostUpdate(const uint16_t TestFrameIndex, const GGNoRe::API::ABS_CPT_IPT_Emulator::SINGLETON::InputsBinaryPacketsForStartingRemote OtherPlayerInitialInputs)
	{
		OnPostUpdate(TestFrameIndex, OtherPlayerInitialInputs);
	}

	virtual const TEST_Player& Player() const = 0;
};

}