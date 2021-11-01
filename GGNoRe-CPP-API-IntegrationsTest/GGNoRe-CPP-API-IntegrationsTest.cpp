/*
 * Copyright 2021 Loïc Venerosy
 */

#include <GGNoRe-CPP-API-IntegrationsTest.hpp>

using namespace GGNoRe::API;

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
	const uint16_t FrameAdvantageInFrames = 2;
	const size_t MockIterationsCount = 120;

	SystemMultiton::GetEmulator(LocalSystemIndex).SyncWithRemoteFrameIndex(StartFrameIndex);
	SystemMultiton::GetEmulator(MockRemoteSystemIndex).SyncWithRemoteFrameIndex(StartFrameIndex);

	TEST_CPT_IPT_Emulator TrueLocalPlayer1Emulator(Player1Index, LocalSystemIndex);
	TEST_CPT_IPT_Emulator LocalPlayer2Emulator(Player2Index, LocalSystemIndex);
	TEST_CPT_RB_SaveStates TrueLocalPlayer1SaveStates(Player1Index, LocalSystemIndex);
	TEST_CPT_RB_SaveStates LocalPlayer2SaveStates(Player2Index, LocalSystemIndex);
	TEST_CPT_RB_Simulator TrueLocalPlayer1Simulator(Player1Index, LocalSystemIndex);
	TEST_CPT_RB_Simulator LocalPlayer2Simulator(Player2Index, LocalSystemIndex);

	TEST_CPT_IPT_Emulator RemotePlayer1Emulator(Player1Index, MockRemoteSystemIndex);
	TEST_CPT_IPT_Emulator TrueRemotePlayer2Emulator(Player2Index, MockRemoteSystemIndex);
	TEST_CPT_RB_SaveStates RemotePlayer1SaveStates(Player1Index, MockRemoteSystemIndex);
	TEST_CPT_RB_SaveStates TrueRemotePlayer2SaveStates(Player2Index, MockRemoteSystemIndex);
	TEST_CPT_RB_Simulator RemotePlayer1Simulator(Player1Index, MockRemoteSystemIndex);
	TEST_CPT_RB_Simulator TrueRemotePlayer2Simulator(Player2Index, MockRemoteSystemIndex);

	uint16_t MockIterationIndex = StartFrameIndex;

	TrueLocalPlayer1Emulator.DownloadInputs = [&](const std::vector<uint8_t>& BinaryPayload)
	{
		SystemMultiton::GetEmulator(MockRemoteSystemIndex).DownloadPlayerBinary(BinaryPayload.data());
	};

	TrueRemotePlayer2Emulator.DownloadInputs = [&](const std::vector<uint8_t>& BinaryPayload)
	{
		if (MockIterationIndex >= StartFrameIndex + FrameAdvantageInFrames)
		{
			if (MockIterationIndex % ReceiveRemoteIntervalInFrames == 0)
			{
				SystemMultiton::GetEmulator(LocalSystemIndex).DownloadPlayerBinary(BinaryPayload.data());
			}
		}
	};

	std::map<uint16_t, std::set<uint8_t>> SingleFrameLocalPlayerIndexToMockInputs;
	SingleFrameLocalPlayerIndexToMockInputs.emplace(std::pair<uint16_t, std::set<uint8_t>>(Player1Index, { {0}, {1}, {2} }));

	std::map<uint16_t, std::set<uint8_t>> SingleFrameRemotePlayerIndexToMockInputs;
	SingleFrameRemotePlayerIndexToMockInputs.emplace(std::pair<uint16_t, std::set<uint8_t>>(Player2Index, { {3}, {2}, {1} }));

	for (; MockIterationIndex < StartFrameIndex + MockIterationsCount; MockIterationIndex++)
	{
		auto LocalSuccess = SystemMultiton::GetEmulator(LocalSystemIndex).TryTickingToNextFrame({
			DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds / 2.f, SingleFrameLocalPlayerIndexToMockInputs
		});
		LogSuccess(LocalSuccess, "LOCAL");

		if (MockIterationIndex >= StartFrameIndex + FrameAdvantageInFrames)
		{
			auto RemoteSuccess = SystemMultiton::GetEmulator(MockRemoteSystemIndex).TryTickingToNextFrame({
				DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds / 2.f, SingleFrameRemotePlayerIndexToMockInputs
			});
			LogSuccess(RemoteSuccess, "REMOTE");
		}
	}

	return true;
}
