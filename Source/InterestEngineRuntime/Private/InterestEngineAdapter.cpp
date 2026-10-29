// SPDX-FileCopyrightText: 2026 PullsarDev
// SPDX-License-Identifier: Apache-2.0

#include "InterestEngineAdapter.h"
#include "interest/version.hpp"

#include <cmath>
#include <limits>
#include <type_traits>

namespace InterestEngine
{
	namespace
	{
		float NarrowDirectionComponent(double Value) noexcept
		{
			constexpr double FloatMax = static_cast<double>(std::numeric_limits<float>::max());
			if (std::isfinite(Value) && std::fabs(Value) > FloatMax)
			{
				return Value > 0.0 ? std::numeric_limits<float>::infinity() : -std::numeric_limits<float>::infinity();
			}
			return static_cast<float>(Value);
		}
	}

	interest::Vec3d ToKernelPosition(const FVector& Position) noexcept
	{
		static_assert(std::is_same_v<FVector::FReal, double>);
		return {Position.X, Position.Y, Position.Z};
	}

	interest::Vec3f ToKernelDirection(const FVector& Direction) noexcept
	{
		return {NarrowDirectionComponent(Direction.X), NarrowDirectionComponent(Direction.Y), NarrowDirectionComponent(Direction.Z)};
	}

	bool TryToKernelTime(double Seconds, interest::TimeMs& OutMs) noexcept
	{
		if (!std::isfinite(Seconds))
		{
			return false;
		}
		const double Milliseconds = std::floor(Seconds * 1000.0);
		constexpr double Limit = 9223372036854775808.0;
		if (!(Milliseconds >= -Limit && Milliseconds < Limit))
		{
			return false;
		}
		OutMs = static_cast<interest::TimeMs>(Milliseconds);
		return true;
	}

	const char* KernelVersion() noexcept { return interest::VersionString(); }
	const char* ToString(interest::StepStatus Status) noexcept { return interest::ToString(Status); }
	const char* ToString(interest::DecisionKind Kind) noexcept { return interest::ToString(Kind); }
	const char* ToString(interest::DecisionReason Reason) noexcept { return interest::ToString(Reason); }
	const char* ToString(interest::Eligibility Value) noexcept { return interest::ToString(Value); }
	void ResetMemory(interest::ObserverMemory& Memory) noexcept { interest::ResetMemory(Memory); }

	FProfileOwner::FProfileOwner(const interest::AttentionProfile& Settings) : Profile(Settings)
	{
		if (!Settings.contexts.empty())
		{
			Contexts.assign(Settings.contexts.begin(), Settings.contexts.end());
		}
		Profile.contexts = std::span<const interest::ContextPreference>(Contexts);
	}

	FProfileOwner::~FProfileOwner() = default;
	const interest::AttentionProfile& FProfileOwner::Get() const & noexcept { return Profile; }
	interest::StepStatus FProfileOwner::Validate() const noexcept { return interest::ValidateProfile(Profile); }

	FStepWorkspace::FStepWorkspace() = default;
	FStepWorkspace::~FStepWorkspace() = default;

	bool FStepWorkspace::Prepare(int32 InCandidateCapacity, bool bPrepareTrace)
	{
		if (InCandidateCapacity < 0)
		{
			return false;
		}
		interest::Reserve(Scratch, static_cast<std::size_t>(InCandidateCapacity));
		if (bPrepareTrace)
		{
			interest::Reserve(Trace, static_cast<std::size_t>(InCandidateCapacity));
		}
		return true;
	}

	std::size_t FStepWorkspace::CandidateCapacity() const noexcept { return Scratch.sortedIds.capacity(); }
	std::size_t FStepWorkspace::TraceCapacity() const noexcept { return Trace.candidates.capacity(); }

	interest::StepStatus FStepWorkspace::Step(const interest::ObserverSnapshot& Observer,
		std::span<const interest::CandidateSnapshot> Candidates, const FProfileOwner& Profile,
		interest::TimeMs Now, interest::ObserverMemory& Memory, interest::Decision& Output, bool bTrace) noexcept
	{
		return interest::Step(Observer, Candidates, Profile.Get(), Now, Memory, Scratch, Output, bTrace ? &Trace : nullptr);
	}

	const interest::DecisionTrace& FStepWorkspace::GetTrace() const & noexcept { return Trace; }
}
