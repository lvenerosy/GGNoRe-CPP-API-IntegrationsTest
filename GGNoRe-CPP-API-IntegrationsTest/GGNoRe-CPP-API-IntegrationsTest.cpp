/*
 * Copyright 2021 Loïc Venerosy
 */

#include <GGNoRe-CPP-API-IntegrationsTest.hpp>

#include <functional>
#include <set>
#include <stdlib.h>
#include <utility>

using namespace GGNoRe::API;

static void TestLog(const std::string& Message)
{
#if GGNORECPPAPI_LOG
	std::cout << Message << std::endl << std::endl;
#endif
}

class TEST_CPT_IPT_Emulator final : public ABS_CPT_IPT_Emulator
{
	DATA_Player OwnerInternal;

	std::set<uint8_t> LocalMockInputs;
	const bool UseRandomInputs = false;

	std::vector<uint8_t> Inputs;

public:
	explicit TEST_CPT_IPT_Emulator(const bool UseRandomInputs)
		:UseRandomInputs(UseRandomInputs)
	{
	}

	DATA_Player Owner() const { return OwnerInternal; }

	const std::vector<uint8_t>& LatestInputs() const
	{
		assert(!Inputs.empty());
		return Inputs;
	}

	bool ShouldSendInputsToTarget(const uint8_t TargetSystemIndex) const
	{
		return OwnerInternal.SystemIndex != TargetSystemIndex && OwnerInternal.IsLocal && !Inputs.empty();
	}

	~TEST_CPT_IPT_Emulator() { ResetAndCleanup(); }

protected:
	void OnRegisterActivationChange(const RegisterActivationChangeEvent RegisteredActivationChange, const ActivationChangeEvent ActivationChange) override
	{
		if (RegisteredActivationChange.Success == I_RB_Rollbackable::RegisterSuccess_E::Registered)
		{
			// Both OnRegisterActivationChange and ShouldSendInputsToTarget happen outside of the simulation so they can be properly ordered so that the owner is valid
			OwnerInternal = ActivationChange.Owner;
		}
		else
		{
			throw RegisteredActivationChange.Success;
		}
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

	void ResetAndCleanup() noexcept override
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
		if (RegisteredActivationChange.Success != I_RB_Rollbackable::RegisterSuccess_E::Registered)
		{
			throw RegisteredActivationChange.Success;
		}
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

	void ResetAndCleanup() noexcept override
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
		if (RegisteredActivationChange.Success != I_RB_Rollbackable::RegisterSuccess_E::Registered)
		{
			throw RegisteredActivationChange.Success;
		}
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

	void ResetAndCleanup() noexcept override {}
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

	void ActivateInPast(const DATA_Player Owner, const uint16_t StartFrameIndex)
	{
		assert(PlayersInternal.find(this) == PlayersInternal.cend());

		DebugId = DebugIdCounter++;

		PlayersInternal.insert(this);

		EmulatorInternal.ChangeActivationInPast({ I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate, Owner, StartFrameIndex });
		SaveStatesInternal.ChangeActivationInPast({ I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate, Owner, StartFrameIndex });
		SimulatorInternal.ChangeActivationInPast({ I_RB_Rollbackable::ActivationChangeEvent::ChangeType_E::Activate, Owner, StartFrameIndex });
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

		for (auto Player : TEST_Player::Players())
		{
			if (Player->Emulator().ShouldSendInputsToTarget(SystemIndex))
			{
				TestLog("############ INPUT TRANSFER FROM PLAYER " + std::to_string(Player->Emulator().Owner().Id) + " TO SYSTEM " + std::to_string(SystemIndex) + " - FRAME " + std::to_string(CurrentFrameIndex) + " ############");
				assert(SystemMultiton::GetEmulator(SystemIndex).DownloadRemotePlayerBinary(Player->Emulator().LatestInputs().data()) == ABS_CPT_IPT_Emulator::SINGLETON::DownloadSuccess_E::Success);
			}
		}
	}
}

static void TryTickingToNextFrame(float DeltaDurationInSeconds, const bool AllowStarvation)
{
	for (auto SystemIndex : SystemIndexes)
	{
		TestLog("____________ SYSTEM " + std::to_string(SystemIndex) + " START - ITERATION " + std::to_string(MockIterationIndex) + " ____________");

		auto Success = SystemMultiton::GetRollbackable(SystemIndex).TryTickingToNextFrame(DeltaDurationInSeconds);

		switch (Success)
		{
		case ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::DoubleSimulation:
			TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex) + " DOUBLE - ITERATION " + std::to_string(MockIterationIndex) + " ^^^^^^^^^^^^");
			break;
		case ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::StallAdvantage:
			TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex) + " STALLING - ITERATION " + std::to_string(MockIterationIndex) + " ^^^^^^^^^^^^");
			break;
		case ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::StarvedForInput:
			TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex) + " STARVED - ITERATION " + std::to_string(MockIterationIndex) + " ^^^^^^^^^^^^");
			assert(AllowStarvation);
			break;
		case ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::StayCurrent:
			TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex) + " STAY - ITERATION " + std::to_string(MockIterationIndex) + " ^^^^^^^^^^^^");
			break;
		case ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::ToNext:
			TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex) + " NEXT - ITERATION " + std::to_string(MockIterationIndex) + " ^^^^^^^^^^^^");
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

bool Test1Local2RemoteMockRollback(const DATA_CFG Config, const TestEnvironment Environment, const PlayersSetup Setup)
{
	assert(Environment.ReceiveRemoteIntervalInFrames > 0);
	assert(Environment.MockHardwareFrameDurationInSeconds > 0.f);

	DATA_CFG::Load(Config);

	const uint16_t RemoteStartFrameIndex = Setup.LocalStartFrameIndex + Setup.RemoteStartOffsetInFrames;
	const bool AllowStarvation = Environment.ReceiveRemoteIntervalInFrames > Config.RollbackBufferMaxSize() + 1;

	const DATA_Player TrueLocalPlayer1Identity{ 0, true, Setup.LocalStartFrameIndex, 0 };
	const DATA_Player LocalPlayer2Identity{ TrueLocalPlayer1Identity.Id + 1, false, RemoteStartFrameIndex, TrueLocalPlayer1Identity.SystemIndex };

	const DATA_Player TrueRemotePlayer2Identity{ LocalPlayer2Identity.Id, true, RemoteStartFrameIndex, uint8_t(LocalPlayer2Identity.SystemIndex + 1) };
	const DATA_Player RemotePlayer1Identity{ TrueLocalPlayer1Identity.Id, false, RemoteStartFrameIndex, TrueRemotePlayer2Identity.SystemIndex };

	const auto FrameToIteration = [Config, Environment](const uint16_t FrameIndex) -> uint16_t
	{
		// TODO: ensure that this is not undefined behavior
		return uint16_t(float(FrameIndex) * Config.SimulationConfiguration.FrameDurationInSeconds / Environment.MockHardwareFrameDurationInSeconds);
	};

	const size_t MockIterationsCount = FrameToIteration(uint16_t(Environment.TestDurationInFrames));

	TEST_Player TrueLocalPlayer1(Setup.UseRandomInputs);
	TEST_Player LocalPlayer2(Setup.UseRandomInputs);
	TEST_Player RemotePlayer1(Setup.UseRandomInputs);
	TEST_Player TrueRemotePlayer2(Setup.UseRandomInputs);

	TEST_NSPC_Systems::MockIterationIndex = FrameToIteration(Setup.LocalStartFrameIndex);

	const uint16_t TrueStartMockIterationIndex = FrameToIteration(RemoteStartFrameIndex);
	const uint16_t RemoteStartMockIterationIndex = FrameToIteration(RemoteStartFrameIndex + Environment.LocalFrameAdvantageInFrames);

	TEST_NSPC_Systems::SyncWithRemoteFrameIndex(TrueLocalPlayer1Identity.SystemIndex, Setup.LocalStartFrameIndex);

	TrueLocalPlayer1.ActivateNow(TrueLocalPlayer1Identity);

	srand(0);

	for (; TEST_NSPC_Systems::MockIterationIndex < FrameToIteration(Setup.LocalStartFrameIndex) + (uint16_t)MockIterationsCount; TEST_NSPC_Systems::MockIterationIndex++)
	{
		if (TEST_NSPC_Systems::MockIterationIndex == TrueStartMockIterationIndex)
		{
			TEST_NSPC_Systems::SyncWithRemoteFrameIndex(TrueRemotePlayer2Identity.SystemIndex, RemoteStartFrameIndex);

			TrueRemotePlayer2.ActivateNow(TrueRemotePlayer2Identity);
		}

		if (TEST_NSPC_Systems::MockIterationIndex == RemoteStartMockIterationIndex)
		{
			try
			{
				RemotePlayer1.ActivateInPast(RemotePlayer1Identity, RemoteStartFrameIndex);
				LocalPlayer2.ActivateInPast(LocalPlayer2Identity, RemoteStartFrameIndex);
			}
			catch (const I_RB_Rollbackable::RegisterSuccess_E& Success)
			{
				// Basically illegal activation, in a real use you first would make sure you can activate before actually activating
				return Success == I_RB_Rollbackable::RegisterSuccess_E::UnreachablePastFrame && Environment.LocalFrameAdvantageInFrames > Config.RollbackBufferMaxSize();
			}

			TEST_NSPC_Systems::TransferLocalPlayersInputs();
		}

		if (TEST_NSPC_Systems::MockIterationIndex % std::max(FrameToIteration(Environment.ReceiveRemoteIntervalInFrames), uint16_t(1)) == 0 && TEST_NSPC_Systems::MockIterationIndex > RemoteStartMockIterationIndex)
		{
			TEST_NSPC_Systems::TransferLocalPlayersInputs();
		}

		TEST_NSPC_Systems::TryTickingToNextFrame(Environment.MockHardwareFrameDurationInSeconds, AllowStarvation);
	}

	TEST_NSPC_Systems::ForceResetAndCleanup();

	return true;
}
