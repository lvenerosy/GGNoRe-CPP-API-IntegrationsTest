/*
 * Copyright 2021 Loïc Venerosy
 */

#pragma once

#include <Core/Data/DATA_CFG.hpp>
#include <GGNoRe-CPP-API.hpp>
#include <Input/ABS_CPT_IPT_Emulator.hpp>
#include <Rollback/ABS_CPT_RB_SaveStates.hpp>
#include <Rollback/ABS_CPT_RB_Simulator.hpp>

#include <assert.h>
#include <iostream>

class TEST_CPT_IPT_Emulator final : public GGNoRe::API::ABS_CPT_IPT_Emulator
{
public:
	TEST_CPT_IPT_Emulator(const uint16_t OwningPlayerIndex, const uint8_t SystemIndex = 0)
		:ABS_CPT_IPT_Emulator(OwningPlayerIndex, SystemIndex)
	{}

protected:
	void OnReadyToSendInputs(const std::vector<uint8_t>& BinaryPayload) const override {}
};

class TEST_CPT_RB_SaveStates final : public GGNoRe::API::ABS_CPT_RB_SaveStates
{
public:
	TEST_CPT_RB_SaveStates(const uint16_t OwningPlayerIndex, const uint8_t SystemIndex = 0)
		:GGNoRe::API::ABS_CPT_RB_SaveStates(OwningPlayerIndex, SystemIndex)
	{}

protected:
	// This mock should not trigger trigger rollback even if the inputs don't match
	// The objective here is to always fill the buffer the same way so the checksum is consistent
	void OnSerialize(std::vector<uint8_t>& TargetBufferOut) override
	{
		TargetBufferOut.clear();
		const uint16_t CurrentFrameIndex = GGNoRe::API::SystemMultiton::GetEmulator(GetSystemIndex()).CurrentFrameIndex();
		// TODO: instead of 0 use the real starting frame index
		const uint16_t StartFrameIndex = 0;
		// +1 because the inputs are registered after the delay
		if (CurrentFrameIndex > StartFrameIndex + GGNoRe::API::DATA_CFG::Get().RollbackConfiguration.DelayFramesCount + 1)
		{
			TargetBufferOut.resize(2);
			std::memcpy(TargetBufferOut.data() + TargetBufferOut.size() - 2, &CurrentFrameIndex, sizeof(uint16_t));
		}
	}
	void OnDeserialize(const std::vector<uint8_t>& SourceBuffer) override {}
};

class TEST_CPT_RB_Simulator final : public GGNoRe::API::ABS_CPT_RB_Simulator
{
public:
	TEST_CPT_RB_Simulator(const uint16_t OwningPlayerIndex, const uint8_t SystemIndex = 0)
		:GGNoRe::API::ABS_CPT_RB_Simulator(OwningPlayerIndex, SystemIndex)
	{}

protected:
	void OnSimulateFrame(const float FrameDurationInSeconds, const std::set<uint8_t>& Inputs) override {}
	void OnEndOfLife() override {}
};

struct GGNoRe::API::ABS_CPT_IPT_Emulator::Implementation;

bool TestBasicRollback();

// TODO:
// actor lifetime
// join mid game
// variable framerate

int main()
{
	assert(TestBasicRollback());

	return 0;
}