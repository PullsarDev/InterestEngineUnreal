// SPDX-FileCopyrightText: 2026 PullsarDev
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "CoreTypes.h"
#include "Math/Vector.h"
#include "interest/attention.hpp"

// Kernel structs are the shared native value contract. Its free functions are NOT
// exported from the module: in modular (Editor) builds an external module that calls
// interest::Step, Reserve, ValidateProfile, ResetMemory, VersionString or ToString
// fails to link, while monolithic Game builds happen to link. Use the adapter entry
// points below, which cover every kernel free function.
namespace InterestEngine
{
	// No axis or unit change; profile ranges must use the same units (normally cm).
	INTERESTENGINERUNTIME_API interest::Vec3d ToKernelPosition(const FVector& Position) noexcept;
	// No normalization/repair. Finite overflow becomes signed infinity; NaN/Inf and
	// zero remain invalid for kernel validation. Tiny values may underflow to zero.
	INTERESTENGINERUNTIME_API interest::Vec3f ToKernelDirection(const FVector& Direction) noexcept;
	// floor(Seconds * 1000), including negative values. Reject non-finite results
	// and values outside [-2^63, 2^63); leave OutMs unchanged on failure.
	INTERESTENGINERUNTIME_API bool TryToKernelTime(double Seconds, interest::TimeMs& OutMs) noexcept;
	INTERESTENGINERUNTIME_API const char* KernelVersion() noexcept;
	// Static, null-terminated enumerator names from the kernel. A value outside the
	// enumeration yields "Unknown". Never free or modify the result.
	INTERESTENGINERUNTIME_API const char* ToString(interest::StepStatus Status) noexcept;
	INTERESTENGINERUNTIME_API const char* ToString(interest::DecisionKind Kind) noexcept;
	INTERESTENGINERUNTIME_API const char* ToString(interest::DecisionReason Reason) noexcept;
	INTERESTENGINERUNTIME_API const char* ToString(interest::Eligibility Value) noexcept;
	// Equivalent to `Memory = {}`: returns one observer's temporal state to "never stepped".
	// It does not make an old candidate event new, and it does not touch any workspace.
	INTERESTENGINERUNTIME_API void ResetMemory(interest::ObserverMemory& Memory) noexcept;

	// Copies Settings and its borrowed contexts during construction (setup may allocate).
	// Immutable, noncopyable and nonmovable so the internal span never becomes stale.
	// Settings.contexts must be valid only during construction. Any view from Get()
	// is borrowed from this owner; do not retain it beyond the owner's lifetime.
	class INTERESTENGINERUNTIME_API FProfileOwner final
	{
	public:
		explicit FProfileOwner(const interest::AttentionProfile& Settings);
		~FProfileOwner();
		FProfileOwner(const FProfileOwner&) = delete;
		FProfileOwner& operator=(const FProfileOwner&) = delete;
		FProfileOwner(FProfileOwner&&) = delete;
		FProfileOwner& operator=(FProfileOwner&&) = delete;

		const interest::AttentionProfile& Get() const & noexcept;
		const interest::AttentionProfile& Get() const && = delete;
		interest::StepStatus Validate() const noexcept;

	private:
		std::vector<interest::ContextPreference> Contexts;
		interest::AttentionProfile Profile;
	};

	// One workspace per concurrent worker. Sequential observers can share one, but
	// each observer owns a persistent, value-initialized interest::ObserverMemory.
	// All methods require exclusive access, including Prepare/GetTrace/destruction.
	class INTERESTENGINERUNTIME_API FStepWorkspace final
	{
	public:
		FStepWorkspace();
		~FStepWorkspace();
		FStepWorkspace(const FStepWorkspace&) = delete;
		FStepWorkspace& operator=(const FStepWorkspace&) = delete;
		FStepWorkspace(FStepWorkspace&&) = delete;
		FStepWorkspace& operator=(FStepWorkspace&&) = delete;

		// Setup only; reserves at least this capacity, never shrinks. Negative input
		// returns false without changing storage, even in Shipping. Optional trace
		// preparation is independent of whether an individual Step requests tracing.
		// Allocation failure follows Unreal's fatal OOM policy, not a false result.
		[[nodiscard]] bool Prepare(int32 CandidateCapacity, bool bPrepareTrace = false);
		std::size_t CandidateCapacity() const noexcept;
		std::size_t TraceCapacity() const noexcept;

		// Direct kernel call: no allocation, policy, input repair or retained input
		// references. Non-Ok leaves memory, output, scratch and trace untouched.
		interest::StepStatus Step(const interest::ObserverSnapshot& Observer,
			std::span<const interest::CandidateSnapshot> Candidates,
			const FProfileOwner& Profile, interest::TimeMs Now,
			interest::ObserverMemory& Memory, interest::Decision& Output,
			bool bTrace = false) noexcept;

		// Last successful traced step. Untraced/failed steps leave it unchanged.
		// Borrowed until preparation, the next successful traced step or destruction.
		const interest::DecisionTrace& GetTrace() const & noexcept;
		const interest::DecisionTrace& GetTrace() const && = delete;

	private:
		interest::StepScratch Scratch;
		interest::DecisionTrace Trace;
	};
}
