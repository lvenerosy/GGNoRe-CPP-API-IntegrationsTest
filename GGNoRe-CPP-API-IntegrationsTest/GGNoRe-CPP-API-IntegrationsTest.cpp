/*
 * Copyright 2021 Loïc Venerosy
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
	TEST_CPT_IPT_Emulator(const uint16_t OwningPlayerIndex, const uint8_t SystemIndex)
		:ABS_CPT_IPT_Emulator(OwningPlayerIndex, SystemIndex)
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
	TEST_CPT_RB_SaveStates(const uint16_t OwningPlayerIndex, const uint8_t SystemIndex)
		:ABS_CPT_RB_SaveStates(OwningPlayerIndex, SystemIndex)
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
	TEST_CPT_RB_Simulator(const uint16_t OwningPlayerIndex, const uint8_t SystemIndex, TEST_CPT_RB_SaveStates& SaveStates)
		:ABS_CPT_RB_Simulator(OwningPlayerIndex, SystemIndex), SaveStates(SaveStates)
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

	const uint16_t Player1Index = 0;
	const uint16_t Player2Index = Player1Index + 1;

	const uint8_t LocalSystemIndex = 0;
	const uint8_t MockRemoteSystemIndex = LocalSystemIndex + 1;

	const uint16_t StartFrameIndex = 0;
	const uint16_t ReceiveRemoteIntervalInFrames = 3;
	const uint16_t FrameAdvantageInFrames = 4;
	const size_t MockIterationsCount = 120;

	//SystemMultiton::GetEmulator(LocalSystemIndex).SyncWithRemoteFrameIndex(StartFrameIndex);
	//SystemMultiton::GetEmulator(MockRemoteSystemIndex).SyncWithRemoteFrameIndex(StartFrameIndex);

	TEST_CPT_IPT_Emulator TrueLocalPlayer1Emulator(Player1Index, LocalSystemIndex);
	TEST_CPT_IPT_Emulator LocalPlayer2Emulator(Player2Index, LocalSystemIndex);
	TEST_CPT_RB_SaveStates TrueLocalPlayer1SaveStates(Player1Index, LocalSystemIndex);
	TEST_CPT_RB_SaveStates LocalPlayer2SaveStates(Player2Index, LocalSystemIndex);
	TEST_CPT_RB_Simulator TrueLocalPlayer1Simulator(Player1Index, LocalSystemIndex, TrueLocalPlayer1SaveStates);
	TEST_CPT_RB_Simulator LocalPlayer2Simulator(Player2Index, LocalSystemIndex, LocalPlayer2SaveStates);

	TEST_CPT_IPT_Emulator RemotePlayer1Emulator(Player1Index, MockRemoteSystemIndex);
	TEST_CPT_IPT_Emulator TrueRemotePlayer2Emulator(Player2Index, MockRemoteSystemIndex);
	TEST_CPT_RB_SaveStates RemotePlayer1SaveStates(Player1Index, MockRemoteSystemIndex);
	TEST_CPT_RB_SaveStates TrueRemotePlayer2SaveStates(Player2Index, MockRemoteSystemIndex);
	TEST_CPT_RB_Simulator RemotePlayer1Simulator(Player1Index, MockRemoteSystemIndex, RemotePlayer1SaveStates);
	TEST_CPT_RB_Simulator TrueRemotePlayer2Simulator(Player2Index, MockRemoteSystemIndex, TrueRemotePlayer2SaveStates);

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

	std::map<uint16_t, std::set<uint8_t>> SingleFrameLocalPlayerIndexToMockInputs;
	SingleFrameLocalPlayerIndexToMockInputs.emplace(std::pair<uint16_t, std::set<uint8_t>>(Player1Index, { {0}, {1}, {2} }));

	std::map<uint16_t, std::set<uint8_t>> SingleFrameRemotePlayerIndexToMockInputs;
	SingleFrameRemotePlayerIndexToMockInputs.emplace(std::pair<uint16_t, std::set<uint8_t>>(Player2Index, { {3}, {2}, {1} }));

	for (; MockIterationIndex < StartFrameIndex + MockIterationsCount; MockIterationIndex++)
	{
		auto LocalSuccess = SystemMultiton::GetEmulator(LocalSystemIndex).TryTickingToNextFrame({
			DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds / FrameDurationDivider, SingleFrameLocalPlayerIndexToMockInputs
		});
		LogSuccess(LocalSuccess, "LOCAL");

		if (MockIterationIndex * FrameDurationDivider >= StartFrameIndex + FrameAdvantageInFrames)
		{
			auto RemoteSuccess = SystemMultiton::GetEmulator(MockRemoteSystemIndex).TryTickingToNextFrame({
				DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds / FrameDurationDivider, SingleFrameRemotePlayerIndexToMockInputs
			});
			LogSuccess(RemoteSuccess, "REMOTE");
		}

		//SingleFrameLocalPlayerIndexToMockInputs[Player1Index] = { {(uint8_t)(rand() % CPT_IPT_TogglesPayload::MaxInputToken())} };
		//SingleFrameRemotePlayerIndexToMockInputs[Player2Index] = { {(uint8_t)(rand() % CPT_IPT_TogglesPayload::MaxInputToken())} };
	}

	return true;
}
