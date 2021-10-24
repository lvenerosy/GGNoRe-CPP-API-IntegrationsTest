/*
 * Copyright 2021 Loïc Venerosy
 */

#pragma once

#include <Core/Data/DATA_CFG.hpp>
#include <Input/ABS_CPT_IPT_Emulator.hpp>
#include <Rollback/ABS_CPT_RB_SaveStates.hpp>
#include <Rollback/ABS_CPT_RB_Simulator.hpp>

class TEST_CPT_RB_SaveStates : public GGNoRe::API::ABS_CPT_RB_SaveStates
{
protected:
	void OnSerialize(std::vector<uint8_t>& TargetBufferOut) override {}
	void OnDeserialize(const std::vector<uint8_t>& SourceBuffer) override {}
};

class TEST_CPT_RB_Simulator : public GGNoRe::API::ABS_CPT_RB_Simulator
{
public:
	TEST_CPT_RB_Simulator(const uint16_t SpawnFrameIndex)
		:ABS_CPT_RB_Simulator(SpawnFrameIndex)
	{
	}

protected:
	virtual void OnSimulateFrame(const float FrameDurationInSeconds) override {}
	virtual void OnEndOfLife() override {}
};

class CPT_IPT_Emulator : public GGNoRe::API::ABS_CPT_IPT_Emulator
{

};

int main()
{
	GGNoRe::API::DATA_CFG::Get().LogHumanReadable("LINKER TEST");

	TEST_CPT_RB_SaveStates SaveStates;
	TEST_CPT_RB_Simulator Simulator(0);

	Simulator.SimulateFrame(0);
	SaveStates.Serialize(0);
	Simulator.SimulateFrame(1);
	SaveStates.Serialize(1);
	SaveStates.Deserialize(0);
	Simulator.SimulateFrame(0);

	GGNoRe::API::CPT_IPT_TogglesPayload TogglesPayload;
	const uint8_t InputToggle0 = 0;
	const uint8_t InputToggle1 = 1;
	const uint8_t InputToggle10 = 10;
	const uint8_t InputToggle11 = 11;
	// Raw inputs: 0 1 | 0 1 10 11 | 0 10 | |
	// Binary: 00000000 00000001 | 00000000 00000001 00001010 00001011 | 00000000 00001010 | |
	// Toggles: 00000000 00000001 0 | 00001010 00001011 0 | 00000001 00001011 0 | 00000000 00001010 0 |
	// Network ready toggles: 10000000 10000001 0 | 10001010 10001011 0 | 10000001 10001011 0 | 10000000 10001010 0 |
	TogglesPayload.Initialize(0);
	TogglesPayload.PushInputAsToggle(InputToggle0);
	TogglesPayload.PushInputAsToggle(InputToggle1);
	TogglesPayload.PushFrameEnd();
	TogglesPayload.PushInputAsToggle(InputToggle10);
	TogglesPayload.PushInputAsToggle(InputToggle11);
	TogglesPayload.PushFrameEnd();
	TogglesPayload.PushInputAsToggle(InputToggle1);
	TogglesPayload.PushInputAsToggle(InputToggle11);
	TogglesPayload.PushFrameEnd();
	TogglesPayload.PushInputAsToggle(InputToggle0);
	TogglesPayload.PushInputAsToggle(InputToggle10);
	TogglesPayload.PushFrameEnd();
	TogglesPayload.LogHumanReadable("PAYLOAD");
	CPT_IPT_Emulator Emulator;
	Emulator.LoadNetworkStream(TogglesPayload);

	return 0;
}