/*
 * Copyright 2021 Lo�c Venerosy
 */

#include <GGNoRe-CPP-API-IntegrationsTest.hpp>

#include <GGNoRe-CPP-API.hpp>

#include <functional>
#include <iostream>
#include <stdlib.h>

using namespace GGNoRe::API;

class TEST_CPT_IPT_Emulator final : public ABS_CPT_IPT_Emulator
{
public:
	TEST_CPT_IPT_Emulator(const id_t OwningPlayerId, const uint8_t SystemIndex)
		:ABS_CPT_IPT_Emulator(OwningPlayerId, SystemIndex)
	{}

	std::function<void(const std::vector<uint8_t>&)> DownloadInputs;

protected:
	void OnReadyToSendInputs(const std::vector<uint8_t>& BinaryPayload) override
	{
		DownloadInputs(BinaryPayload);
	}
};

class TEST_CPT_RB_Simulator;
class TEST_CPT_RB_SaveStates final : public ABS_CPT_RB_SaveStates
{
	friend class TEST_CPT_RB_Simulator;
	uint8_t Counter = 0;
public:
	TEST_CPT_RB_SaveStates(const id_t OwningPlayerId, const uint8_t SystemIndex)
		:ABS_CPT_RB_SaveStates(OwningPlayerId, SystemIndex)
	{}

protected:
	// This mock should not trigger trigger rollback even if the inputs don't match
	// The objective here is to always fill the buffer the same way so the checksum is consistent
	void OnSerialize(std::vector<uint8_t>& TargetBufferOut) override
	{
		TargetBufferOut.clear();
		// TODO: instead of 0 use the real starting frame index
		//const uint16_t StartFrameIndex = 0;
		// +1 because the inputs are registered after the delay
		//if (CurrentFrameIndex > StartFrameIndex + DATA_CFG::Get().RollbackConfiguration.DelayFramesCount + 1)
		{
			TargetBufferOut.push_back(Counter);
		}
	}
	void OnDeserialize(const std::vector<uint8_t>& SourceBuffer) override
	{
		if (SourceBuffer.size() == 1)
		{
			Counter = SourceBuffer[0];
		}
	}
};

class TEST_CPT_RB_Simulator final : public ABS_CPT_RB_Simulator
{
	TEST_CPT_RB_SaveStates& SaveStates;
public:
	TEST_CPT_RB_Simulator(const id_t OwningPlayerId, const uint8_t SystemIndex, TEST_CPT_RB_SaveStates& SaveStates)
		:ABS_CPT_RB_Simulator(OwningPlayerId, SystemIndex), SaveStates(SaveStates)
	{}

protected:
	void OnSimulateFrame(const float FrameDurationInSeconds, const std::set<uint8_t>& Inputs) override
	{
		SaveStates.Counter++;
	}

	void OnEndOfLife() override {}
};

void LogSuccess(ABS_CPT_IPT_Emulator::SINGLETON::TickSuccess_E Success, std::string SystemName)
{
	switch (Success)
	{
	case ABS_CPT_IPT_Emulator::SINGLETON::TickSuccess_E::StallAdvantage:
		std::cout << "###########" + SystemName + " STALLING############" << std::endl;
			break;
	case ABS_CPT_IPT_Emulator::SINGLETON::TickSuccess_E::StarvedForInput:
		std::cout << "###########" + SystemName + " STARVED############" << std::endl;
			break;
	case ABS_CPT_IPT_Emulator::SINGLETON::TickSuccess_E::StayCurrent:
		std::cout << "###########" + SystemName + " STAY############" << std::endl;
			break;
	case ABS_CPT_IPT_Emulator::SINGLETON::TickSuccess_E::ToNext:
		std::cout << "###########" + SystemName + " NEXT############" << std::endl;
			break;
	default:
		std::cout << "###########" + SystemName + " DEFAULT############" << std::endl;
		break;
	}
}

bool TestRemoteMockRollback()
{
	DATA_CFG ConfigWithNoFakedDelay;
	ConfigWithNoFakedDelay.FakedMissedPredictionsFramesCount = 0; // Comment out to use the fake rollback
	DATA_CFG::Load(ConfigWithNoFakedDelay);

	const id_t Player1Id;
	const id_t Player2Id = Player1Id + 1;

	const uint8_t LocalSystemIndex = 0;
	const uint8_t MockRemoteSystemIndex = LocalSystemIndex + 1;

	const uint16_t StartFrameIndex = 0;
	const uint16_t ReceiveRemoteIntervalInFrames = 3;
	const uint16_t FrameAdvantageInFrames = 4;
	const size_t MockIterationsCount = 120;

	//SystemMultiton::GetEmulator(LocalSystemIndex).SyncWithRemoteFrameIndex(StartFrameIndex);
	//SystemMultiton::GetEmulator(MockRemoteSystemIndex).SyncWithRemoteFrameIndex(StartFrameIndex);

	TEST_CPT_IPT_Emulator TrueLocalPlayer1Emulator(Player1Id, LocalSystemIndex);
	TEST_CPT_IPT_Emulator LocalPlayer2Emulator(Player2Id, LocalSystemIndex);
	TEST_CPT_RB_SaveStates TrueLocalPlayer1SaveStates(Player1Id, LocalSystemIndex);
	TEST_CPT_RB_SaveStates LocalPlayer2SaveStates(Player2Id, LocalSystemIndex);
	TEST_CPT_RB_Simulator TrueLocalPlayer1Simulator(Player1Id, LocalSystemIndex, TrueLocalPlayer1SaveStates);
	TEST_CPT_RB_Simulator LocalPlayer2Simulator(Player2Id, LocalSystemIndex, LocalPlayer2SaveStates);

	TEST_CPT_IPT_Emulator RemotePlayer1Emulator(Player1Id, MockRemoteSystemIndex);
	TEST_CPT_IPT_Emulator TrueRemotePlayer2Emulator(Player2Id, MockRemoteSystemIndex);
	TEST_CPT_RB_SaveStates RemotePlayer1SaveStates(Player1Id, MockRemoteSystemIndex);
	TEST_CPT_RB_SaveStates TrueRemotePlayer2SaveStates(Player2Id, MockRemoteSystemIndex);
	TEST_CPT_RB_Simulator RemotePlayer1Simulator(Player1Id, MockRemoteSystemIndex, RemotePlayer1SaveStates);
	TEST_CPT_RB_Simulator TrueRemotePlayer2Simulator(Player2Id, MockRemoteSystemIndex, TrueRemotePlayer2SaveStates);

	uint16_t MockIterationIndex = StartFrameIndex;

	TrueLocalPlayer1Emulator.DownloadInputs = [&](const std::vector<uint8_t>& BinaryPayload)
	{
		SystemMultiton::GetEmulator(MockRemoteSystemIndex).DownloadPlayerBinary(BinaryPayload.data());
	};

	const uint16_t FrameDurationDivider = 2;

	TrueRemotePlayer2Emulator.DownloadInputs = [&](const std::vector<uint8_t>& BinaryPayload)
	{
		if (MockIterationIndex * FrameDurationDivider >= StartFrameIndex + FrameAdvantageInFrames)
		{
			if (MockIterationIndex * FrameDurationDivider % ReceiveRemoteIntervalInFrames == 0)
			{
				SystemMultiton::GetEmulator(LocalSystemIndex).DownloadPlayerBinary(BinaryPayload.data());
			}
		}
	};

	srand(0);

	std::map<id_t, std::set<uint8_t>> SingleFrameLocalPlayerIdToMockInputs;
	SingleFrameLocalPlayerIdToMockInputs.emplace(std::pair<id_t, std::set<uint8_t>>(Player1Id, { {0}, {1}, {2} }));

	std::map<id_t, std::set<uint8_t>> SingleFrameRemotePlayerIdToMockInputs;
	SingleFrameRemotePlayerIdToMockInputs.emplace(std::pair<id_t, std::set<uint8_t>>(Player2Id, { {3}, {2}, {1} }));

	for (; MockIterationIndex < StartFrameIndex + MockIterationsCount; MockIterationIndex++)
	{
		auto LocalSuccess = SystemMultiton::GetEmulator(LocalSystemIndex).TryTickingToNextFrame({
			DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds / FrameDurationDivider, SingleFrameLocalPlayerIdToMockInputs
		});
		LogSuccess(LocalSuccess, "LOCAL");

		if (MockIterationIndex * FrameDurationDivider >= StartFrameIndex + FrameAdvantageInFrames)
		{
			auto RemoteSuccess = SystemMultiton::GetEmulator(MockRemoteSystemIndex).TryTickingToNextFrame({
				DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds / FrameDurationDivider, SingleFrameRemotePlayerIdToMockInputs
			});
			LogSuccess(RemoteSuccess, "REMOTE");
		}

		//SingleFrameLocalPlayerIdToMockInputs[Player1Id] = { {(uint8_t)(rand() % CPT_IPT_TogglesPayload::MaxInputToken())} };
		//SingleFrameRemotePlayerIdToMockInputs[Player2Id] = { {(uint8_t)(rand() % CPT_IPT_TogglesPayload::MaxInputToken())} };
	}

	return true;
}
