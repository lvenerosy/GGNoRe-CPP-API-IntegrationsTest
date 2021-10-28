/*
 * Copyright 2021 Loïc Venerosy
 */

#pragma once

#include <Core/Data/DATA_CFG.hpp>
#include <Input/ABS_CPT_IPT_Emulator.hpp>
#include <Rollback/ABS_CPT_RB_SaveStates.hpp>
#include <Rollback/ABS_CPT_RB_Simulator.hpp>

#include <assert.h>

class TEST_CPT_IPT_Emulator final : public GGNoRe::API::ABS_CPT_IPT_Emulator
{
public:
	TEST_CPT_IPT_Emulator(const uint16_t OwningPlayerIndex)
		:ABS_CPT_IPT_Emulator(OwningPlayerIndex)
	{}

protected:
	void OnReadyToSendInputs(const std::vector<uint8_t>& BinaryPayload) const override {}
};

class TEST_CPT_RB_SaveStates final : public GGNoRe::API::ABS_CPT_RB_SaveStates
{
public:
	TEST_CPT_RB_SaveStates(const uint16_t OwningPlayerIndex)
		:GGNoRe::API::ABS_CPT_RB_SaveStates(OwningPlayerIndex)
	{}

protected:
	void OnSerialize(std::vector<uint8_t>& TargetBufferOut) override
	{
		// This should not trigger trigger rollback
		TargetBufferOut.resize(TargetBufferOut.size() + 2);
		const uint16_t CurrentFrameIndex = GGNoRe::API::ABS_CPT_IPT_Emulator::CurrentFrameIndex();
		std::memcpy(TargetBufferOut.data() + TargetBufferOut.size() - 2, &CurrentFrameIndex, sizeof(uint16_t));
	}
	void OnDeserialize(const std::vector<uint8_t>& SourceBuffer) override {}
};

class TEST_CPT_RB_Simulator final : public GGNoRe::API::ABS_CPT_RB_Simulator
{
public:
	TEST_CPT_RB_Simulator(const uint16_t OwningPlayerIndex)
		:GGNoRe::API::ABS_CPT_RB_Simulator(OwningPlayerIndex)
	{}

protected:
	void OnSimulateFrame(const float FrameDurationInSeconds, const std::set<uint8_t>& Inputs) override {}
	void OnEndOfLife() override {}
};

struct GGNoRe::API::ABS_CPT_IPT_Emulator::Implementation;

bool TestBasicRollback();

// TODO:
// input starved (see comment in TestBasicRollback)
// actor lifetime
// join mid game
// variable framerate

int main()
{
	assert(TestBasicRollback());

	return 0;
}