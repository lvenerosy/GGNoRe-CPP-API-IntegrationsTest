/*
 * Copyright 2021 Loïc Venerosy
 */

#include <GGNoRe-CPP-API-IntegrationsTest.hpp>

#include <GGNoRe-CPP-API.hpp>

#include <functional>
#include <iostream>
#include <set>
#include <stdlib.h>

using namespace GGNoRe::API;

class TEST_CPT_IPT_Emulator final : public ABS_CPT_IPT_Emulator
{
	std::set<uint8_t> LocalMockInputs;
	const bool UseRandomInputs = false;

	std::vector<uint8_t> Inputs;

public:
	explicit TEST_CPT_IPT_Emulator(const bool UseRandomInputs)
		:UseRandomInputs(UseRandomInputs)
	{
	}

	const std::vector<uint8_t>& LatestInputs() const
	{
		assert(!Inputs.empty());
		return Inputs;
	}

	bool ShouldSendInputsToTarget(const uint8_t TargetSystemIndex) const
	{
		assert(HasActivationState());
		assert(LastActivationState().IsActive);

		return LastActivationState().Owner.IsLocal && LastActivationState().Owner.SystemIndex != TargetSystemIndex;
	}

	~TEST_CPT_IPT_Emulator() { ResetAndCleanup(); }

protected:
	void OnRegisterActivationChange(const RegisterActivationChangeEvent RegisteredActivationChange, const ActivationChangeEvent ActivationChange) override
	{
		assert(RegisteredActivationChange.Success == ABS_RB_Rollbackable::RegisterActivationChangeEvent::RegisterSuccess_E::Registered);
	}

	void OnActivationChange(const ActivationChangeEvent ActivationChange) override
	{
		if (ActivationChange.Type == ActivationChangeEvent::ChangeType_E::Activate)
		{
			LocalMockInputs = { { (uint8_t)ActivationChange.Owner.Id }, {10}, {20} };
		}
	}

	void OnRollActivationChangeBack(const ActivationChangeEvent ActivationChange) override {}

	void OnPreRollback(const uint16_t RollbackFrameIndex) override {}
	void OnRollback(const uint16_t RollbackFrameIndex) override {}

	void OnSaveFrame(const uint16_t SavedFrameIndex) override {}
	void OnPostSaveFrame(const uint16_t SavedFrameIndex) override {}

	void OnPreSimulate(const uint16_t SimulatedFrameIndex) override {}
	void OnSimulate(const uint16_t SimulatedFrameIndex, const std::set<uint8_t>& Inputs) override {}

	void OnStarvedForInputFrame(const uint16_t FrameIndex) override {}
	void OnStallAdvantageFrame(const uint16_t FrameIndex) override {}
	void OnStayCurrentFrame(const uint16_t FrameIndex) override {}
	void OnPreToNextFrame(const uint16_t FrameIndex) override {}

	const std::set<uint8_t>& OnPollLocalInputs() override
	{
		if (UseRandomInputs)
		{
			LocalMockInputs = { {(uint8_t)(rand() % CPT_IPT_TogglesPayload::MaxInputToken())} };
		}

		return LocalMockInputs;
	}

	void OnReadyToSendInputs(const std::vector<uint8_t>& BinaryPayload) override
	{
		Inputs = BinaryPayload;
	}

	void ResetAndCleanup() override
	{
		Inputs.clear();
		LocalMockInputs.clear();
	}
};

class TEST_CPT_RB_SaveStates final : public ABS_CPT_RB_SaveStates
{
	uint8_t Counter = 0;

public:
	~TEST_CPT_RB_SaveStates() { ResetAndCleanup(); }

protected:
	void OnRegisterActivationChange(const RegisterActivationChangeEvent RegisteredActivationChange, const ActivationChangeEvent ActivationChange) override
	{
		assert(RegisteredActivationChange.Success == ABS_RB_Rollbackable::RegisterActivationChangeEvent::RegisterSuccess_E::Registered);
	}

	void OnActivationChange(const ActivationChangeEvent ActivationChange) override
	{
		if (ActivationChange.Type == ActivationChangeEvent::ChangeType_E::Activate)
		{
			Counter = (uint8_t)ActivationChange.FrameIndex;
		}
	}

	void OnRollActivationChangeBack(const ActivationChangeEvent ActivationChange) override {}

	void OnPreRollback(const uint16_t RollbackFrameIndex) override {}
	void OnRollback(const uint16_t RollbackFrameIndex) override {}

	void OnSaveFrame(const uint16_t SavedFrameIndex) override {}
	void OnPostSaveFrame(const uint16_t SavedFrameIndex) override {}

	void OnPreSimulate(const uint16_t SimulatedFrameIndex) override {}
	void OnSimulate(const uint16_t SimulatedFrameIndex, const std::set<uint8_t>& Inputs) override { Counter++; }

	void OnStarvedForInputFrame(const uint16_t FrameIndex) override {}
	void OnStallAdvantageFrame(const uint16_t FrameIndex) override {}
	void OnStayCurrentFrame(const uint16_t FrameIndex) override {}
	void OnPreToNextFrame(const uint16_t FrameIndex) override {}

	// This mock should not trigger trigger rollback even if the inputs don't match
	// The objective here is to always fill the buffer the same way so the checksum is consistent
	void OnSerialize(std::vector<uint8_t>& TargetBufferOut) override
	{
		TargetBufferOut.clear();
		TargetBufferOut.push_back(Counter);
	}

	void OnDeserialize(const std::vector<uint8_t>& SourceBuffer) override
	{
		if (SourceBuffer.size() == 1)
		{
			Counter = SourceBuffer[0];
		}
	}

	void ResetAndCleanup() override
	{
		Counter = 0;
	}
};

class TEST_CPT_RB_Simulator final : public ABS_CPT_RB_Simulator
{
public:
	~TEST_CPT_RB_Simulator() { ResetAndCleanup(); }

protected:
	void OnRegisterActivationChange(const RegisterActivationChangeEvent RegisteredActivationChange, const ActivationChangeEvent ActivationChange) override
	{
		assert(RegisteredActivationChange.Success == ABS_RB_Rollbackable::RegisterActivationChangeEvent::RegisterSuccess_E::Registered);
	}

	void OnActivationChange(const ActivationChangeEvent ActivationChange) override {}

	void OnRollActivationChangeBack(const ActivationChangeEvent ActivationChange) override {}

	void OnPreRollback(const uint16_t RollbackFrameIndex) override {}
	void OnRollback(const uint16_t RollbackFrameIndex) override {}

	void OnSaveFrame(const uint16_t SavedFrameIndex) override {}
	void OnPostSaveFrame(const uint16_t SavedFrameIndex) override {}

	void OnPreSimulate(const uint16_t SimulatedFrameIndex) override {}
	void OnSimulate(const uint16_t SimulatedFrameIndex, const std::set<uint8_t>& Inputs) override { }

	void OnStarvedForInputFrame(const uint16_t FrameIndex) override {}
	void OnStallAdvantageFrame(const uint16_t FrameIndex) override {}
	void OnStayCurrentFrame(const uint16_t FrameIndex) override {}
	void OnPreToNextFrame(const uint16_t FrameIndex) override {}

	void ResetAndCleanup() override {}
};

// This is a player class grouping the 3 components for simplicity's sake
// In your game it could be a fireball with only save states and a simulator, or a player input controller with only an emulator
class TEST_Player final
{
	static std::set<TEST_Player*> PlayersInternal;
	static uint32_t DebugIdCounter;

	uint32_t DebugId = 0;

	TEST_CPT_IPT_Emulator EmulatorInternal;
	TEST_CPT_RB_SaveStates SaveStatesInternal;
	TEST_CPT_RB_Simulator SimulatorInternal;

public:
	explicit TEST_Player(const bool UseRandomInputs)
		:EmulatorInternal(UseRandomInputs)
	{
	}

	~TEST_Player()
	{
		PlayersInternal.erase(this);
	}

	static const std::set<TEST_Player*>& Players()
	{
		return PlayersInternal;
	}

	const TEST_CPT_IPT_Emulator& Emulator() const
	{
		return EmulatorInternal;
	}

	void ActivateNow(const DATA_Player Owner)
	{
		assert(PlayersInternal.find(this) == PlayersInternal.cend());

		DebugId = DebugIdCounter++;

		PlayersInternal.insert(this);

		EmulatorInternal.ChangeActivationNow(Owner, I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate);
		SaveStatesInternal.ChangeActivationNow(Owner, I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate);
		SimulatorInternal.ChangeActivationNow(Owner, I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate);
	}

	void ActivateInPast(const I_RB_Rollbackable::ActivationChangeEvent Activation)
	{
		assert(PlayersInternal.find(this) == PlayersInternal.cend());

		DebugId = DebugIdCounter++;

		PlayersInternal.insert(this);

		EmulatorInternal.ChangeActivationInPast(Activation);
		SaveStatesInternal.ChangeActivationInPast(Activation);
		SimulatorInternal.ChangeActivationInPast(Activation);
	}
};

std::set<TEST_Player*> TEST_Player::PlayersInternal;
uint32_t TEST_Player::DebugIdCounter = 0;

namespace TEST_NSPC_Systems
{

std::set<uint8_t> SystemIndexes;
uint16_t MockIterationIndex = 0;

static void SyncWithRemoteFrameIndex(const uint8_t SystemIndex, const uint16_t StartFrameIndex)
{
	assert(SystemIndexes.find(SystemIndex) == SystemIndexes.cend());

	SystemMultiton::GetRollbackable(SystemIndex).SyncWithRemoteFrameIndex(StartFrameIndex);
	SystemIndexes.insert(SystemIndex);
}

static void TransferLocalPlayersInputs()
{
	for (auto SystemIndex : SystemIndexes)
	{
		auto CurrentFrameIndex = SystemMultiton::GetRollbackable(SystemIndex).UnsimulatedFrameIndex();

		std::cout << "############ INPUT TRANSFER TO SYSTEM " + std::to_string(SystemIndex) + " - FRAME " + std::to_string(CurrentFrameIndex) + " ############" << std::endl << std::endl;

		for (auto Player : TEST_Player::Players())
		{
			if (Player->Emulator().ShouldSendInputsToTarget(SystemIndex))
			{
				assert(SystemMultiton::GetEmulator(SystemIndex).DownloadRemotePlayerBinary(Player->Emulator().LatestInputs().data()) == ABS_CPT_IPT_Emulator::SINGLETON::DownloadSuccess_E::Success);
			}
		}

		std::cout << "############ ############ ############ ############" << std::endl << std::endl;
	}
}

static void TryTickingToNextFrame(float DeltaDurationInSeconds)
{
	for (auto SystemIndex : SystemIndexes)
	{
		std::cout << "____________ SYSTEM " + std::to_string(SystemIndex) + " START - ITERATION " + std::to_string(MockIterationIndex) + " ____________" << std::endl << std::endl;

		auto Success = SystemMultiton::GetRollbackable(SystemIndex).TryTickingToNextFrame(DeltaDurationInSeconds);

		switch (Success)
		{
		case ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::StallAdvantage:
			std::cout << "^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex) + " STALLING - ITERATION " + std::to_string(MockIterationIndex) + " ^^^^^^^^^^^^" << std::endl << std::endl;
			break;
		case ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::StarvedForInput:
			std::cout << "^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex) + " STARVED - ITERATION " + std::to_string(MockIterationIndex) + " ^^^^^^^^^^^^" << std::endl << std::endl;
			assert(false);
			break;
		case ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::StayCurrent:
			std::cout << "^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex) + " STAY - ITERATION " + std::to_string(MockIterationIndex) + " ^^^^^^^^^^^^" << std::endl << std::endl;
			break;
		case ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::ToNext:
			std::cout << "^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex) + " NEXT - ITERATION " + std::to_string(MockIterationIndex) + " ^^^^^^^^^^^^" << std::endl << std::endl;
			break;
		default:
			assert(false);
			break;
		}
	}
}

static void ForceResetAndCleanup()
{
	for (auto SystemIndex : SystemIndexes)
	{
		SystemMultiton::GetRollbackable(SystemIndex).ForceResetAndCleanup();
	}

	MockIterationIndex = 0;
	SystemIndexes.clear();
}

}

bool Test1Local2RemoteMockRollback(const bool UseFakeRollback, const bool UseRandomInputs)
{
	DATA_CFG Config;
	Config.FakedMissedPredictionsFramesCount *= UseFakeRollback;
	DATA_CFG::Load(Config);

	const uint16_t LocalStartFrameIndex = 0;
	const uint16_t RemoteStartFrameIndex = 2;
	assert(RemoteStartFrameIndex >= LocalStartFrameIndex);

	const DATA_Player TrueLocalPlayer1Identity{ 0, true, LocalStartFrameIndex, 0 };
	const DATA_Player LocalPlayer2Identity{ TrueLocalPlayer1Identity.Id + 1, false, RemoteStartFrameIndex, TrueLocalPlayer1Identity.SystemIndex };

	const DATA_Player TrueRemotePlayer2Identity{ LocalPlayer2Identity.Id, true, RemoteStartFrameIndex, uint8_t(LocalPlayer2Identity.SystemIndex + 1) };
	const DATA_Player RemotePlayer1Identity{ TrueLocalPlayer1Identity.Id, false, RemoteStartFrameIndex, TrueRemotePlayer2Identity.SystemIndex };

	const uint16_t ReceiveRemoteIntervalInFrames = 3;
	const uint16_t FrameAdvantageInFrames = 3;
	assert(RemoteStartFrameIndex + FrameAdvantageInFrames + 1 < Config.RollbackBufferMaxSize());
	const uint16_t FrameDurationDivider = 2;
	const size_t MockIterationsCount = 60 * FrameDurationDivider;

	TEST_Player TrueLocalPlayer1(UseRandomInputs);
	TEST_Player LocalPlayer2(UseRandomInputs);
	TEST_Player RemotePlayer1(UseRandomInputs);
	TEST_Player TrueRemotePlayer2(UseRandomInputs);

	TEST_NSPC_Systems::MockIterationIndex = LocalStartFrameIndex * FrameDurationDivider;

	const uint16_t TrueStartMockIterationIndex = RemoteStartFrameIndex * FrameDurationDivider;
	const uint16_t RemoteStartMockIterationIndex = (RemoteStartFrameIndex + FrameAdvantageInFrames) * FrameDurationDivider;

	TEST_NSPC_Systems::SyncWithRemoteFrameIndex(TrueLocalPlayer1Identity.SystemIndex, LocalStartFrameIndex);

	TrueLocalPlayer1.ActivateNow(TrueLocalPlayer1Identity);

	srand(0);

	for (; TEST_NSPC_Systems::MockIterationIndex < LocalStartFrameIndex * FrameDurationDivider + (uint16_t)MockIterationsCount; TEST_NSPC_Systems::MockIterationIndex++)
	{
		if (TEST_NSPC_Systems::MockIterationIndex == TrueStartMockIterationIndex)
		{
			TEST_NSPC_Systems::SyncWithRemoteFrameIndex(TrueRemotePlayer2Identity.SystemIndex, RemoteStartFrameIndex);

			TrueRemotePlayer2.ActivateNow(TrueRemotePlayer2Identity);
		}

		if (TEST_NSPC_Systems::MockIterationIndex == RemoteStartMockIterationIndex)
		{
			RemotePlayer1.ActivateInPast({ I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate, RemotePlayer1Identity,  RemoteStartFrameIndex });
			LocalPlayer2.ActivateInPast({ I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate, LocalPlayer2Identity,  RemoteStartFrameIndex });

			// Necessary to properly initialize the starting inputs and avoid triggering starvation
			TEST_NSPC_Systems::TransferLocalPlayersInputs();
		}
		
		if (TEST_NSPC_Systems::MockIterationIndex % (ReceiveRemoteIntervalInFrames * FrameDurationDivider) == 0 && TEST_NSPC_Systems::MockIterationIndex > RemoteStartMockIterationIndex)
		{
			TEST_NSPC_Systems::TransferLocalPlayersInputs();
		}

		TEST_NSPC_Systems::TryTickingToNextFrame(DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds / FrameDurationDivider);
	}

	TEST_NSPC_Systems::ForceResetAndCleanup();

	return true;
}
