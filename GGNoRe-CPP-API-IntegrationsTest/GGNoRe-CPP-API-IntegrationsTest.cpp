/*
 * Copyright 2021 Loic Venerosy
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

	~TEST_CPT_IPT_Emulator() = default;

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

struct TEST_CPT_State
{
	uint16_t NonZero = 1; // Necessary otherwise the other fields initialized to 0 generate a checksum with a value of 0 which is considered as being a missing checksum
	uint8_t InputsAccumulator = 0; // To check that the de/serialization are consistent with OnSimulateFrame
	float DeltaDurationAccumulatorInSeconds = 0.f; // To check that the de/serialization are consistent with OnSimulateTick
};

class TEST_CPT_RB_SaveStates final : public ABS_CPT_RB_SaveStates
{
	TEST_CPT_State& PlayerState;

public:
	TEST_CPT_RB_SaveStates(TEST_CPT_State& PlayerState)
		:PlayerState(PlayerState)
	{}

	~TEST_CPT_RB_SaveStates() = default;

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

	void OnStarvedForInputFrame(const uint16_t FrameIndex) override {}
	void OnStallAdvantageFrame(const uint16_t FrameIndex) override {}
	void OnStayCurrentFrame(const uint16_t FrameIndex) override {}
	void OnPreToNextFrame(const uint16_t FrameIndex) override {}

	void OnSerialize(std::vector<uint8_t>& TargetBufferOut) override
	{
		TargetBufferOut.resize(sizeof(PlayerState.NonZero) + sizeof(PlayerState.InputsAccumulator) + sizeof(PlayerState.DeltaDurationAccumulatorInSeconds));

		std::memcpy(TargetBufferOut.data(), &PlayerState.NonZero, sizeof(PlayerState.NonZero));
		size_t CopyOffset = sizeof(PlayerState.NonZero);

		std::memcpy(TargetBufferOut.data() + CopyOffset, &PlayerState.InputsAccumulator, sizeof(PlayerState.InputsAccumulator));
		CopyOffset += sizeof(PlayerState.InputsAccumulator);

		std::memcpy(TargetBufferOut.data() + CopyOffset, &PlayerState.DeltaDurationAccumulatorInSeconds, sizeof(PlayerState.DeltaDurationAccumulatorInSeconds));
	}

	void OnDeserialize(const std::vector<uint8_t>& SourceBuffer) override
	{
		if (!SourceBuffer.empty())
		{
			std::memcpy(&PlayerState.NonZero, SourceBuffer.data(), sizeof(PlayerState.NonZero));
			size_t ReadOffset = sizeof(PlayerState.NonZero);

			std::memcpy(&PlayerState.InputsAccumulator, SourceBuffer.data() + ReadOffset, sizeof(PlayerState.InputsAccumulator));
			ReadOffset += sizeof(PlayerState.InputsAccumulator);

			std::memcpy(&PlayerState.DeltaDurationAccumulatorInSeconds, SourceBuffer.data() + ReadOffset, sizeof(PlayerState.DeltaDurationAccumulatorInSeconds));
		}
	}

	void ResetAndCleanup() noexcept override {}
};

class TEST_CPT_RB_Simulator final : public ABS_CPT_RB_Simulator
{
	TEST_CPT_State& PlayerState;

public:
	TEST_CPT_RB_Simulator(TEST_CPT_State& PlayerState)
		:PlayerState(PlayerState)
	{}

	~TEST_CPT_RB_Simulator() = default;

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

	void OnSimulateFrame(const uint16_t SimulatedFrameIndex, const std::set<uint8_t>& Inputs) override { for (auto Input : Inputs) { PlayerState.InputsAccumulator += Input; } }
	void OnSimulateTick(const float DeltaDurationInSeconds) override { PlayerState.DeltaDurationAccumulatorInSeconds += DeltaDurationInSeconds;  }

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

	SystemMock(const PlayersSetup Setup)
		:Setup(Setup),
		RemoteStartFrameIndex(Setup.LocalStartFrameIndex + Setup.RemoteStartOffsetInFrames)
	{}

	bool Tick(float DeltaDurationInSeconds, const uint8_t SystemIndex, const OutcomesSanityCheck AllowedOutcomes)
	{
		TestLog("____________ SYSTEM " + std::to_string(SystemIndex) + " START - ITERATION " + std::to_string(MockIterationIndex) + " ____________");

		auto Success = SystemMultiton::GetRollbackable(SystemIndex).TryTickingToNextFrame(DeltaDurationInSeconds);

		switch (Success)
		{
		case ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::DoubleSimulation:
			TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex) + " DOUBLE - ITERATION " + std::to_string(MockIterationIndex) + " ^^^^^^^^^^^^");
			assert(AllowedOutcomes.AllowDoubleSimulation);
			break;
		case ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::StallAdvantage:
			TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex) + " STALLING - ITERATION " + std::to_string(MockIterationIndex) + " ^^^^^^^^^^^^");
			assert(AllowedOutcomes.AllowStallAdvantage);
			break;
		case ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::StarvedForInput:
			TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex) + " STARVED - ITERATION " + std::to_string(MockIterationIndex) + " ^^^^^^^^^^^^");
			assert(AllowedOutcomes.AllowStarvedForInput);
			break;
		case ABS_RB_Rollbackable::SINGLETON::TickSuccess_E::StayCurrent:
			TestLog("^^^^^^^^^^^^ SYSTEM " + std::to_string(SystemIndex) + " STAY - ITERATION " + std::to_string(MockIterationIndex) + " ^^^^^^^^^^^^");
			assert(AllowedOutcomes.AllowStayCurrent);
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
	virtual void Update(const OutcomesSanityCheck AllowedOutcomes) = 0;
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

	void Update(const OutcomesSanityCheck AllowedOutcomes) override
	{
		while (!Tick(Setup.LocalMockHardwareFrameDurationInSeconds, TrueLocalPlayer1Identity.SystemIndex, AllowedOutcomes));
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

	// * 2 because you need one transfer to get/send the inputs and then another one to get/send the updated checksum
	const bool AllowStarvedForInput = (size_t)Environment.ReceiveRemoteIntervalInFrames * 2 > Config.RollbackBufferMaxSize();
	
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

		Local.Update
		({
			Setup.LocalMockHardwareFrameDurationInSeconds >= 2.f * Config.SimulationConfiguration.FrameDurationInSeconds,
			Environment.ReceiveRemoteIntervalInFrames > 1,
			AllowStarvedForInput,
			Setup.LocalMockHardwareFrameDurationInSeconds < Config.SimulationConfiguration.FrameDurationInSeconds
		});
		Remote.Update
		({
			Setup.RemoteMockHardwareFrameDurationInSeconds >= 2.f * Config.SimulationConfiguration.FrameDurationInSeconds,
			Environment.ReceiveRemoteIntervalInFrames > 1,
			AllowStarvedForInput,
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
