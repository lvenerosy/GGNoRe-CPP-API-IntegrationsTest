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
	TEST_CPT_RB_SaveStates(uint8_t Counter)
		:ABS_CPT_RB_SaveStates(), Counter(Counter)
	{}

protected:
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
};

class TEST_CPT_RB_Simulator final : public ABS_CPT_RB_Simulator
{
	TEST_CPT_RB_SaveStates& SaveStates;
public:
	TEST_CPT_RB_Simulator(TEST_CPT_RB_SaveStates& SaveStates)
		:SaveStates(SaveStates)
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

	const uint16_t LocalStartFrameIndex = 0;
	const uint16_t RemoteStartFrameIndex = 2;
	assert(RemoteStartFrameIndex >= LocalStartFrameIndex);

	const DATA_Player TrueLocalPlayer1{ 0, true, LocalStartFrameIndex, 0 };
	const DATA_Player LocalPlayer2{ TrueLocalPlayer1.Id + 1, false, RemoteStartFrameIndex, TrueLocalPlayer1.SystemIndex };

	const DATA_Player TrueRemotePlayer2{ LocalPlayer2.Id, true, RemoteStartFrameIndex, uint8_t(LocalPlayer2.SystemIndex + 1) };
	const DATA_Player RemotePlayer1{ TrueLocalPlayer1.Id, false, RemoteStartFrameIndex, TrueRemotePlayer2.SystemIndex };

	const uint16_t ReceiveRemoteIntervalInFrames = 3;
	const uint16_t FrameAdvantageInFrames = 4;
	const size_t MockIterationsCount = 120;

	TEST_CPT_IPT_Emulator TrueLocalPlayer1Emulator;
	TEST_CPT_IPT_Emulator LocalPlayer2Emulator;
	TEST_CPT_RB_SaveStates TrueLocalPlayer1SaveStates(LocalStartFrameIndex);
	TEST_CPT_RB_SaveStates LocalPlayer2SaveStates(RemoteStartFrameIndex);
	TEST_CPT_RB_Simulator TrueLocalPlayer1Simulator(TrueLocalPlayer1SaveStates);
	TEST_CPT_RB_Simulator LocalPlayer2Simulator(LocalPlayer2SaveStates);

	TEST_CPT_IPT_Emulator RemotePlayer1Emulator;
	TEST_CPT_IPT_Emulator TrueRemotePlayer2Emulator;
	TEST_CPT_RB_SaveStates RemotePlayer1SaveStates(RemoteStartFrameIndex);
	TEST_CPT_RB_SaveStates TrueRemotePlayer2SaveStates(RemoteStartFrameIndex);
	TEST_CPT_RB_Simulator RemotePlayer1Simulator(RemotePlayer1SaveStates);
	TEST_CPT_RB_Simulator TrueRemotePlayer2Simulator(TrueRemotePlayer2SaveStates);

	const uint16_t FrameDurationDivider = 2;
	uint16_t MockIterationIndex = LocalStartFrameIndex * FrameDurationDivider;

	TrueLocalPlayer1Emulator.DownloadInputs = [&](const std::vector<uint8_t>& BinaryPayload)
	{
		if (MockIterationIndex >= RemoteStartFrameIndex * FrameDurationDivider)
		{
			SystemMultiton::GetEmulator(TrueRemotePlayer2.SystemIndex).DownloadPlayerBinary(BinaryPayload.data());
		}
	};

	TrueRemotePlayer2Emulator.DownloadInputs = [&](const std::vector<uint8_t>& BinaryPayload)
	{
		if (MockIterationIndex >= (RemoteStartFrameIndex + FrameAdvantageInFrames) * FrameDurationDivider)
		{
			if (MockIterationIndex % ReceiveRemoteIntervalInFrames * FrameDurationDivider == 0)
			{
				SystemMultiton::GetEmulator(TrueLocalPlayer1.SystemIndex).DownloadPlayerBinary(BinaryPayload.data());
			}
		}
	};

	SystemMultiton::GetEmulator(TrueLocalPlayer1.SystemIndex).SyncWithRemoteFrameIndex(LocalStartFrameIndex);

	assert(TrueLocalPlayer1Emulator.Enable(TrueLocalPlayer1, LocalStartFrameIndex));
	TrueLocalPlayer1SaveStates.Enable(LocalStartFrameIndex, TrueLocalPlayer1.SystemIndex);
	TrueLocalPlayer1Simulator.Enable(TrueLocalPlayer1, LocalStartFrameIndex);

	srand(0);

	std::map<id_t, std::set<uint8_t>> SingleFrameLocalPlayerIdToMockInputs;
	SingleFrameLocalPlayerIdToMockInputs.emplace(std::pair<id_t, std::set<uint8_t>>(TrueLocalPlayer1.Id, { {0}, {1}, {2} }));

	std::map<id_t, std::set<uint8_t>> SingleFrameRemotePlayerIdToMockInputs;
	SingleFrameRemotePlayerIdToMockInputs.emplace(std::pair<id_t, std::set<uint8_t>>(TrueRemotePlayer2.Id, { {3}, {2}, {1} }));

	bool IsRemoteInitialized = false;

	for (; MockIterationIndex < LocalStartFrameIndex * FrameDurationDivider + (uint16_t)MockIterationsCount; MockIterationIndex++)
	{
		if (MockIterationIndex == RemoteStartFrameIndex * FrameDurationDivider && !IsRemoteInitialized)
		{
			IsRemoteInitialized = true;

			assert(LocalPlayer2Emulator.Enable(LocalPlayer2, MockIterationIndex / FrameDurationDivider));
			LocalPlayer2SaveStates.Enable(RemoteStartFrameIndex, LocalPlayer2.SystemIndex);
			LocalPlayer2Simulator.Enable(LocalPlayer2, RemoteStartFrameIndex);

			SystemMultiton::GetEmulator(TrueRemotePlayer2.SystemIndex).SyncWithRemoteFrameIndex(RemoteStartFrameIndex);

			assert(RemotePlayer1Emulator.Enable(RemotePlayer1, MockIterationIndex / FrameDurationDivider));
			assert(TrueRemotePlayer2Emulator.Enable(TrueRemotePlayer2, MockIterationIndex / FrameDurationDivider));
			RemotePlayer1SaveStates.Enable(RemoteStartFrameIndex, RemotePlayer1.SystemIndex);
			TrueRemotePlayer2SaveStates.Enable(RemoteStartFrameIndex, TrueRemotePlayer2.SystemIndex);
			RemotePlayer1Simulator.Enable(RemotePlayer1, RemoteStartFrameIndex);
			TrueRemotePlayer2Simulator.Enable(TrueRemotePlayer2, RemoteStartFrameIndex);
		}

		auto LocalSuccess = SystemMultiton::GetEmulator(TrueLocalPlayer1.SystemIndex).TryTickingToNextFrame({
			DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds / FrameDurationDivider, SingleFrameLocalPlayerIdToMockInputs
		});
		LogSuccess(LocalSuccess, "LOCAL");

		if (MockIterationIndex >= (RemoteStartFrameIndex + FrameAdvantageInFrames) * FrameDurationDivider)
		{
				auto RemoteSuccess = SystemMultiton::GetEmulator(TrueRemotePlayer2.SystemIndex).TryTickingToNextFrame({
				DATA_CFG::Get().SimulationConfiguration.FrameDurationInSeconds / FrameDurationDivider, SingleFrameRemotePlayerIdToMockInputs
			});
			LogSuccess(RemoteSuccess, "REMOTE");
		}

		//SingleFrameLocalPlayerIdToMockInputs[TrueLocalPlayer1.Id] = { {(uint8_t)(rand() % CPT_IPT_TogglesPayload::MaxInputToken())} };
		//SingleFrameRemotePlayerIdToMockInputs[TrueRemotePlayer2.Id] = { {(uint8_t)(rand() % CPT_IPT_TogglesPayload::MaxInputToken())} };
	}

	return true;
}
