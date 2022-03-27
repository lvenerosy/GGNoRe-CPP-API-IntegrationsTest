/*
 * Copyright 2021 Loïc Venerosy
 */

#include <GGNoRe-CPP-API-IntegrationsTest.hpp>

#include <functional>
#include <set>
#include <stdlib.h>
#include <utility>

using namespace GGNoRe::API;

static inline void TestLog(const std::string& Message)
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

	inline DATA_Player Owner() const { return OwnerInternal; }

	inline const std::vector<uint8_t>& LatestInputs() const
	{
		assert(!Inputs.empty());
		return Inputs;
	}

	inline bool ShouldSendInputsToTarget(const uint8_t TargetSystemIndex) const
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

	static inline const std::set<TEST_Player*>& Players()
	{
		return PlayersInternal;
	}

	inline const TEST_CPT_IPT_Emulator& Emulator() const
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

static void ForceResetAndCleanup()
{
	for (auto SystemIndex : SystemIndexes)
	{
		SystemMultiton::GetRollbackable(SystemIndex).ForceResetAndCleanup();
	}

	SystemIndexes.clear();
}

class SystemMock
{
	size_t MockIterationIndex = 0;
	float UpdateTimer = 0.f;

protected:
	const PlayersSetup Setup;
	const uint16_t RemoteStartFrameIndex = 0;

	SystemMock(const PlayersSetup Setup)
		:Setup(Setup),
		RemoteStartFrameIndex(Setup.LocalStartFrameIndex + Setup.RemoteStartOffsetInFrames)
	{}

	bool Tick(float DeltaDurationInSeconds, const uint8_t SystemIndex, const bool AllowStarvation)
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

		++MockIterationIndex;

		UpdateTimer += DeltaDurationInSeconds;
		if (UpdateTimer >= DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds)
		{
			UpdateTimer -= DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds;
			return true;
		}
		else
		{
			return false;
		}
	}

public:
	virtual bool PreUpdate(const uint16_t FrameIndex) = 0;
	virtual void Update(const bool AllowStarvation) = 0;
};

class LocalMock final : public SystemMock
{
public:
	const DATA_Player TrueLocalPlayer1Identity;
	const DATA_Player LocalPlayer2Identity;

	TEST_Player TrueLocalPlayer1;
	TEST_Player LocalPlayer2;

	LocalMock(const PlayersSetup Setup)
		:
		SystemMock(Setup),
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
		if (FrameIndex == RemoteStartFrameIndex + Setup.LocalFrameAdvantageInFrames)
		{
			if (Setup.LocalFrameAdvantageInFrames == 0)
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
					return Success == I_RB_Rollbackable::RegisterSuccess_E::UnreachablePastFrame && Setup.LocalFrameAdvantageInFrames > DATA_CFG::Get().RollbackBufferMaxSize();
				}
			}
		}

		return true;
	}

	void Update(const bool AllowStarvation) override
	{
		while (!Tick(Setup.LocalMockHardwareFrameDurationInSeconds, TrueLocalPlayer1Identity.SystemIndex, AllowStarvation));
	}
};

class RemoteMock final : public SystemMock
{
public:
	const DATA_Player TrueRemotePlayer2Identity;
	const DATA_Player RemotePlayer1Identity;

	TEST_Player TrueRemotePlayer2;
	TEST_Player RemotePlayer1;

	RemoteMock(const PlayersSetup Setup)
		:
		SystemMock(Setup),
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

		if (FrameIndex == RemoteStartFrameIndex + Setup.LocalFrameAdvantageInFrames)
		{
			if (Setup.LocalFrameAdvantageInFrames == 0)
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
					return Success == I_RB_Rollbackable::RegisterSuccess_E::UnreachablePastFrame && Setup.LocalFrameAdvantageInFrames > DATA_CFG::Get().RollbackBufferMaxSize();
				}
			}
		}

		return true;
	}

	void Update(const bool AllowStarvation) override
	{
		while (!Tick(Setup.RemoteMockHardwareFrameDurationInSeconds, TrueRemotePlayer2Identity.SystemIndex, AllowStarvation));
	}
};

}

bool Test1Local2RemoteMockRollback(const DATA_CFG Config, const TestEnvironment Environment, const PlayersSetup Setup)
{
	assert(Environment.ReceiveRemoteIntervalInFrames > 0);
	assert(Setup.LocalMockHardwareFrameDurationInSeconds > 0.f);
	assert(Setup.RemoteMockHardwareFrameDurationInSeconds > 0.f);

	DATA_CFG::Load(Config);

	const bool AllowStarvation = Environment.ReceiveRemoteIntervalInFrames > Config.RollbackBufferMaxSize() + 1;
	const auto RemoteStartMockIterationIndex = Setup.LocalStartFrameIndex + Setup.RemoteStartOffsetInFrames + Setup.LocalFrameAdvantageInFrames;

	TEST_NSPC_Systems::LocalMock Local(Setup);
	TEST_NSPC_Systems::RemoteMock Remote(Setup);

	srand(0);

	for (size_t IterationIndex = 0; IterationIndex < Environment.TestDurationInFrames; ++IterationIndex)
	{
		assert(Local.PreUpdate((uint16_t)IterationIndex));
		assert(Remote.PreUpdate((uint16_t)IterationIndex));

		if (IterationIndex == RemoteStartMockIterationIndex)
		{
			TEST_NSPC_Systems::TransferLocalPlayersInputs();
		}

		Local.Update(AllowStarvation);
		Remote.Update(AllowStarvation);

		if (IterationIndex % Environment.ReceiveRemoteIntervalInFrames == 0 && IterationIndex >= RemoteStartMockIterationIndex)
		{
			TEST_NSPC_Systems::TransferLocalPlayersInputs();
		}
	}

	TEST_NSPC_Systems::ForceResetAndCleanup();

	return true;
}
