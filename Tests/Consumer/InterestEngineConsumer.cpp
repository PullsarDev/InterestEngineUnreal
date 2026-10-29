// SPDX-FileCopyrightText: 2026 PullsarDev
// SPDX-License-Identifier: Apache-2.0

// Compiled ONLY by the generated external consumer module, not by the plugin.
// Every adapter entry point is called from StartupModule, so the link is exercised in
// every target and configuration, including Shipping where automation is compiled out.
#include "InterestEngineAdapter.h"
#include "Modules/ModuleManager.h"
#include "Misc/AutomationTest.h"

#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>

namespace InterestEngineConsumer
{
	namespace IE = InterestEngine;
	// Assign directly so optional Unreal NaN diagnostics in FVector constructors
	// do not repair the deliberately invalid test inputs before the adapter sees them.
	FVector RawVector(double X, double Y, double Z)
	{
		FVector Result = FVector::ZeroVector;
		Result.X = X;
		Result.Y = Y;
		Result.Z = Z;
		return Result;
	}
	interest::AttentionProfile Settings(std::span<const interest::ContextPreference> Contexts)
	{
		interest::AttentionProfile Result;
		Result.contexts = Contexts;
		Result.attractionWeight = 1.f;
		Result.salienceWeight = 0.f;
		Result.affinityWeight = 0.f;
		Result.fullRange = 1000.f;
		Result.maxRange = 2000.f;
		Result.acquireThreshold = 0.5f;
		Result.releaseThreshold = 0.25f;
		Result.switchMargin = 0.125f;
		Result.overrideMargin = 0.25f;
		return Result;
	}

	interest::CandidateSnapshot Candidate(interest::CandidateId Id, float Attraction)
	{
		interest::CandidateSnapshot Result;
		Result.id = Id;
		Result.spatial = interest::SpatialKind::NonSpatial;
		Result.attraction = Attraction;
		return Result;
	}

	class FConsumerModule final : public FDefaultGameModuleImpl
	{
		void StartupModule() override
		{
			std::array<interest::ContextPreference, 1> Contexts{};
			IE::FProfileOwner Profile(Settings(Contexts));
			IE::FStepWorkspace Workspace;
			interest::ObserverSnapshot Observer;
			Observer.position = IE::ToKernelPosition(FVector::ZeroVector);
			Observer.referenceForward = IE::ToKernelDirection(FVector::ForwardVector);
			interest::ObserverMemory Memory{};
			interest::Decision Decision{};
			interest::TimeMs Now = 0;
			std::array Candidates{Candidate(42, 1.f)};
			if (!Workspace.Prepare(1) || !IE::TryToKernelTime(1.0, Now) ||
				Profile.Validate() != interest::StepStatus::Ok ||
				Workspace.Step(Observer, Candidates, Profile, Now, Memory, Decision) != interest::StepStatus::Ok ||
				Decision.kind != interest::DecisionKind::Acquire || Decision.selected != 42 ||
				std::strcmp(IE::KernelVersion(), "0.2.0") != 0 ||
				std::strcmp(IE::ToString(Decision.kind), "Acquire") != 0 ||
				std::strcmp(IE::ToString(Decision.reason), "HighestScore") != 0 ||
				std::strcmp(IE::ToString(interest::Eligibility::Eligible), "Eligible") != 0)
			{
				UE_LOG(LogTemp, Fatal, TEXT("InterestEngine consumer startup failed"));
			}
			IE::ResetMemory(Memory);
			if (Memory.current != 0 || Memory.hasStepped)
			{
				UE_LOG(LogTemp, Fatal, TEXT("InterestEngine consumer ResetMemory failed"));
			}
			UE_LOG(LogTemp, Display, TEXT("InterestEngine consumer startup passed: kernel %s"), UTF8_TO_TCHAR(IE::KernelVersion()));
		}
	};
}

IMPLEMENT_PRIMARY_GAME_MODULE(InterestEngineConsumer::FConsumerModule, InterestEngineHost, "InterestEngineHost")

#if WITH_DEV_AUTOMATION_TESTS
// Defined in the generated ReadmeExample1.cpp, compiled from the README's first C++ block.
namespace ReadmeExample1 { interest::Decision MinimalInterestExample(); }

namespace InterestEngineConsumer
{
	static_assert(!std::is_copy_constructible_v<IE::FProfileOwner>);
	static_assert(!std::is_move_constructible_v<IE::FProfileOwner>);
	static_assert(!std::is_copy_constructible_v<IE::FStepWorkspace>);
	static_assert(!std::is_move_constructible_v<IE::FStepWorkspace>);

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReadmeExample, "InterestEngine.Adapter.ReadmeExample",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	bool FReadmeExample::RunTest(const FString&)
	{
		const interest::Decision Result = ::ReadmeExample1::MinimalInterestExample();
		TestTrue(TEXT("README minimal example acquires its single candidate"),
			Result.kind == interest::DecisionKind::Acquire && Result.selected == 42 &&
			Result.reason == interest::DecisionReason::HighestScore && Result.previous == 0);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStringsAndReset, "InterestEngine.Adapter.StringsAndReset",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	bool FStringsAndReset::RunTest(const FString&)
	{
		auto Same = [](const char* Actual, const char* Expected) { return Actual && std::strcmp(Actual, Expected) == 0; };
		// Every enumerator maps to its own name; the lookup tables live only in the kernel.
		const std::array<std::pair<interest::StepStatus, const char*>, 5> Statuses{{
			{interest::StepStatus::Ok, "Ok"}, {interest::StepStatus::TimeWentBackwards, "TimeWentBackwards"},
			{interest::StepStatus::InvalidObserver, "InvalidObserver"}, {interest::StepStatus::InvalidProfile, "InvalidProfile"},
			{interest::StepStatus::InsufficientCapacity, "InsufficientCapacity"}}};
		for (const auto& [Value, Name] : Statuses)
		{
			TestTrue(TEXT("StepStatus string"), Same(IE::ToString(Value), Name));
		}
		const std::array<std::pair<interest::DecisionKind, const char*>, 5> Kinds{{
			{interest::DecisionKind::None, "None"}, {interest::DecisionKind::Acquire, "Acquire"},
			{interest::DecisionKind::Keep, "Keep"}, {interest::DecisionKind::Switch, "Switch"},
			{interest::DecisionKind::Clear, "Clear"}}};
		for (const auto& [Value, Name] : Kinds)
		{
			TestTrue(TEXT("DecisionKind string"), Same(IE::ToString(Value), Name));
		}
		const std::array<std::pair<interest::DecisionReason, const char*>, 10> Reasons{{
			{interest::DecisionReason::NoEligibleCandidate, "NoEligibleCandidate"},
			{interest::DecisionReason::BelowAcquireThreshold, "BelowAcquireThreshold"},
			{interest::DecisionReason::HighestScore, "HighestScore"},
			{interest::DecisionReason::CurrentInvalid, "CurrentInvalid"},
			{interest::DecisionReason::BelowReleaseThreshold, "BelowReleaseThreshold"},
			{interest::DecisionReason::BetterCandidate, "BetterCandidate"},
			{interest::DecisionReason::DwellOverride, "DwellOverride"},
			{interest::DecisionReason::MinimumDwell, "MinimumDwell"},
			{interest::DecisionReason::InsufficientAdvantage, "InsufficientAdvantage"},
			{interest::DecisionReason::AboveReleaseThreshold, "AboveReleaseThreshold"}}};
		for (const auto& [Value, Name] : Reasons)
		{
			TestTrue(TEXT("DecisionReason string"), Same(IE::ToString(Value), Name));
		}
		const std::array<std::pair<interest::Eligibility, const char*>, 11> Eligibilities{{
			{interest::Eligibility::Eligible, "Eligible"}, {interest::Eligibility::InvalidId, "InvalidId"},
			{interest::Eligibility::DuplicateId, "DuplicateId"}, {interest::Eligibility::InvalidValue, "InvalidValue"},
			{interest::Eligibility::InvalidGeometry, "InvalidGeometry"}, {interest::Eligibility::Disabled, "Disabled"},
			{interest::Eligibility::NotPerceived, "NotPerceived"}, {interest::Eligibility::MissingRequiredTag, "MissingRequiredTag"},
			{interest::Eligibility::ExcludedTag, "ExcludedTag"}, {interest::Eligibility::OutOfRange, "OutOfRange"},
			{interest::Eligibility::InvalidEvent, "InvalidEvent"}}};
		for (const auto& [Value, Name] : Eligibilities)
		{
			TestTrue(TEXT("Eligibility string"), Same(IE::ToString(Value), Name));
		}
		// The kernel (v0.2.0) answers "Unknown" for a value outside the enumeration.
		TestTrue(TEXT("out-of-range kind"), Same(IE::ToString(static_cast<interest::DecisionKind>(0xFF)), "Unknown"));
		TestTrue(TEXT("out-of-range reason"), Same(IE::ToString(static_cast<interest::DecisionReason>(0xFF)), "Unknown"));
		TestTrue(TEXT("out-of-range eligibility"), Same(IE::ToString(static_cast<interest::Eligibility>(0xFF)), "Unknown"));

		// ResetMemory returns one observer to "never stepped"; time may then restart earlier.
		std::array<interest::ContextPreference, 1> Contexts{};
		IE::FProfileOwner Profile(Settings(Contexts));
		IE::FStepWorkspace Workspace;
		TestTrue(TEXT("prepare"), Workspace.Prepare(1));
		interest::ObserverSnapshot Observer;
		interest::ObserverMemory Memory{}, Other{};
		interest::Decision Output{};
		std::array Candidates{Candidate(7, 1.f)};
		TestTrue(TEXT("seed both observers"), Workspace.Step(Observer, Candidates, Profile, 5000, Memory, Output) == interest::StepStatus::Ok &&
			Workspace.Step(Observer, Candidates, Profile, 5000, Other, Output) == interest::StepStatus::Ok);
		TestTrue(TEXT("time may not go backwards without a reset"), Workspace.Step(Observer, Candidates, Profile, 1000, Memory, Output) == interest::StepStatus::TimeWentBackwards);
		IE::ResetMemory(Memory);
		TestTrue(TEXT("reset equals value-initialized memory"), Memory.current == 0 && Memory.focusStartTime == 0 && Memory.lastStepTime == 0 && !Memory.hasStepped);
		TestTrue(TEXT("reset leaves other observers alone"), Other.current == 7 && Other.hasStepped && Other.lastStepTime == 5000);
		TestTrue(TEXT("earlier time and a fresh acquisition after reset"), Workspace.Step(Observer, Candidates, Profile, 1000, Memory, Output) == interest::StepStatus::Ok &&
			Output.kind == interest::DecisionKind::Acquire && Output.previous == 0 && Output.selected == 7);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConversions, "InterestEngine.Adapter.Conversions",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	bool FConversions::RunTest(const FString&)
	{
		const double Max = std::numeric_limits<double>::max();
		const double Inf = std::numeric_limits<double>::infinity();
		const double NaN = std::numeric_limits<double>::quiet_NaN();
		const auto P = IE::ToKernelPosition(FVector(Max, -Max, 1234567890123.25));
		TestTrue(TEXT("positions retain double precision and extremes"), P.x == Max && P.y == -Max && P.z == 1234567890123.25);
		const auto InvalidPosition = IE::ToKernelPosition(RawVector(NaN, Inf, -Inf));
		TestTrue(TEXT("position NaN/Inf preserved"), std::isnan(InvalidPosition.x) && std::isinf(InvalidPosition.y) && InvalidPosition.z < 0.0);
		const auto D = IE::ToKernelDirection(FVector(2.0, -4.0, 0.0));
		TestTrue(TEXT("directions copied without normalization"), D.x == 2.f && D.y == -4.f && D.z == 0.f);
		const auto Overflow = IE::ToKernelDirection(RawVector(Max, -Max, NaN));
		TestTrue(TEXT("overflow becomes invalid signed Inf; NaN remains"), std::isinf(Overflow.x) && Overflow.x > 0.f && std::isinf(Overflow.y) && Overflow.y < 0.f && std::isnan(Overflow.z));
		const auto Infinite = IE::ToKernelDirection(RawVector(Inf, -Inf, 0.0));
		TestTrue(TEXT("infinity remains invalid"), std::isinf(Infinite.x) && std::isinf(Infinite.y));
		const auto FloatEdge = IE::ToKernelDirection(FVector(static_cast<double>(std::numeric_limits<float>::max()), 0.0, std::numeric_limits<double>::denorm_min()));
		TestTrue(TEXT("float maximum and tiny underflow"), FloatEdge.x == std::numeric_limits<float>::max() && FloatEdge.z == 0.f);

		interest::TimeMs Ms = 99;
		TestTrue(TEXT("floor positive seconds"), IE::TryToKernelTime(1.0009, Ms) && Ms == 1000);
		TestTrue(TEXT("floor negative seconds"), IE::TryToKernelTime(-0.0001, Ms) && Ms == -1);
		TestTrue(TEXT("zero"), IE::TryToKernelTime(-0.0, Ms) && Ms == 0);
		constexpr double LimitSeconds = 9223372036854775808.0 / 1000.0;
		TestTrue(TEXT("lower bound accepted"), IE::TryToKernelTime(-LimitSeconds, Ms) && Ms == std::numeric_limits<interest::TimeMs>::min());
		TestTrue(TEXT("last representable seconds below upper bound"), IE::TryToKernelTime(std::nextafter(LimitSeconds, 0.0), Ms) && Ms > 0);
		for (double Invalid : {NaN, Inf, -Inf, Max, -Max, LimitSeconds, std::nextafter(-LimitSeconds, -Inf)})
		{
			Ms = 99;
			TestTrue(TEXT("invalid time rejected without writing output"), !IE::TryToKernelTime(Invalid, Ms) && Ms == 99);
		}
		TestEqual(TEXT("version exported"), FString(IE::KernelVersion()), FString(TEXT("0.2.0")));
		TestEqual(TEXT("status string exported"), FString(IE::ToString(interest::StepStatus::InvalidObserver)), FString(TEXT("InvalidObserver")));
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOwnership, "InterestEngine.Adapter.Ownership",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	bool FOwnership::RunTest(const FString&)
	{
		std::unique_ptr<IE::FProfileOwner> Profile;
		{
			std::array<interest::ContextPreference, 2> Contexts{};
			Contexts[1].fallbackAffinity = 0.75f;
			Contexts[1].affinityTags = 4;
			Contexts[1].tagAffinity[2] = 0.875f;
			auto Source = Settings(Contexts);
			Profile = std::make_unique<IE::FProfileOwner>(Source);
			TestTrue(TEXT("contexts copied"), Profile->Get().contexts.data() != Contexts.data());
			Contexts[1].fallbackAffinity = -1.f;
			Contexts[1].tagAffinity[2] = -1.f;
			Source.maxRange = -1.f;
		}
		TestTrue(TEXT("owner survives source destruction and mutation"), Profile->Validate() == interest::StepStatus::Ok &&
			Profile->Get().contexts.size() == 2 && Profile->Get().contexts[1].fallbackAffinity == 0.75f && Profile->Get().contexts[1].tagAffinity[2] == 0.875f);
		IE::FStepWorkspace Workspace;
		TestTrue(TEXT("prepare"), Workspace.Prepare(1));
		interest::ObserverSnapshot Observer;
		Observer.context = 1;
		interest::ObserverMemory Memory{};
		interest::Decision Output{};
		std::array Candidates{Candidate(1, 1.f)};
		TestTrue(TEXT("step using destroyed source contexts"), Workspace.Step(Observer, Candidates, *Profile, 0, Memory, Output) == interest::StepStatus::Ok);
		IE::FProfileOwner Empty(Settings({}));
		TestTrue(TEXT("empty contexts not repaired"), Empty.Validate() == interest::StepStatus::InvalidProfile);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisions, "InterestEngine.Adapter.Decisions",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	bool FDecisions::RunTest(const FString&)
	{
		std::array<interest::ContextPreference, 1> Contexts{};
		IE::FProfileOwner Profile(Settings(Contexts));
		IE::FStepWorkspace Workspace;
		TestTrue(TEXT("reserve scratch and trace"), Workspace.Prepare(2, true));
		interest::ObserverSnapshot Observer;
		interest::ObserverMemory A{}, B{};
		interest::Decision Output{};
		std::array Candidates{Candidate(20, 0.5f), Candidate(10, 0.5f)};
		TestTrue(TEXT("acquire with deterministic ID tie"), Workspace.Step(Observer, Candidates, Profile, 1000, A, Output, true) == interest::StepStatus::Ok && Output.kind == interest::DecisionKind::Acquire && Output.selected == 10);
		const auto* TraceData = Workspace.GetTrace().candidates.data();
		const auto ScratchCapacity = Workspace.CandidateCapacity();
		const auto TraceCapacity = Workspace.TraceCapacity();
		bool Stable = true;
		for (interest::TimeMs Now = 1000; Now < 1100; ++Now)
		{
			Stable &= Workspace.Step(Observer, Candidates, Profile, Now, A, Output, true) == interest::StepStatus::Ok && Output.kind == interest::DecisionKind::Keep && Output.selected == 10;
		}
		TestTrue(TEXT("100 consecutive calls keep state and storage"), Stable && TraceData == Workspace.GetTrace().candidates.data() && Workspace.CandidateCapacity() == ScratchCapacity && Workspace.TraceCapacity() == TraceCapacity);
		TestTrue(TEXT("independent observer acquires on reused workspace"), Workspace.Step(Observer, Candidates, Profile, 0, B, Output) == interest::StepStatus::Ok && Output.kind == interest::DecisionKind::Acquire && B.current == 10 && A.lastStepTime == 1099);
		Candidates[0].attraction = 1.f;
		TestTrue(TEXT("switch"), Workspace.Step(Observer, Candidates, Profile, 1100, A, Output, true) == interest::StepStatus::Ok && Output.kind == interest::DecisionKind::Switch && Output.previous == 10 && Output.selected == 20);
		TestTrue(TEXT("clear absent current"), Workspace.Step(Observer, {}, Profile, 1101, A, Output) == interest::StepStatus::Ok && Output.kind == interest::DecisionKind::Clear && Output.selected == 0 && A.current == 0 && B.current == 10);
		TestTrue(TEXT("untraced step retains trace"), Workspace.GetTrace().leading == 20 && Workspace.GetTrace().candidates.size() == 2);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFailures, "InterestEngine.Adapter.FailuresAndCapacity",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	bool FFailures::RunTest(const FString&)
	{
		std::array<interest::ContextPreference, 1> Contexts{};
		IE::FProfileOwner Profile(Settings(Contexts));
		IE::FStepWorkspace Workspace;
		interest::ObserverSnapshot Observer;
		interest::ObserverMemory Memory{};
		interest::Decision Output{};
		std::array Candidates{Candidate(1, 1.f)};
		TestTrue(TEXT("unprepared nonempty step fails"), Workspace.Step(Observer, Candidates, Profile, 0, Memory, Output) == interest::StepStatus::InsufficientCapacity && !Memory.hasStepped);
		TestTrue(TEXT("negative preparation rejected"), !Workspace.Prepare(-1, true) && Workspace.CandidateCapacity() == 0 && Workspace.TraceCapacity() == 0);
		TestTrue(TEXT("scratch only"), Workspace.Prepare(1) && Workspace.TraceCapacity() == 0);
		TestTrue(TEXT("unprepared trace fails"), Workspace.Step(Observer, Candidates, Profile, 0, Memory, Output, true) == interest::StepStatus::InsufficientCapacity);
		TestTrue(TEXT("prepare both"), Workspace.Prepare(1, true));
		TestTrue(TEXT("seed successful state"), Workspace.Step(Observer, Candidates, Profile, 100, Memory, Output, true) == interest::StepStatus::Ok);
		const auto InitialCapacity = Workspace.CandidateCapacity();
		const auto InitialTraceCapacity = Workspace.TraceCapacity();
		const auto* TraceData = Workspace.GetTrace().candidates.data();
		TestTrue(TEXT("smaller and invalid preparation never shrink"), Workspace.Prepare(0, true) && !Workspace.Prepare(-1, true) && Workspace.CandidateCapacity() == InitialCapacity && Workspace.TraceCapacity() == InitialTraceCapacity);

		const auto BeforeMemory = Memory;
		const auto BeforeOutput = Output;
		const auto BeforeTrace = Workspace.GetTrace();
		auto Unchanged = [&]()
		{
			const auto& Trace = Workspace.GetTrace();
			return Memory.current == BeforeMemory.current && Memory.focusStartTime == BeforeMemory.focusStartTime && Memory.lastStepTime == BeforeMemory.lastStepTime && Memory.hasStepped == BeforeMemory.hasStepped &&
				Output.kind == BeforeOutput.kind && Output.reason == BeforeOutput.reason && Output.selected == BeforeOutput.selected && Output.previous == BeforeOutput.previous && Output.rawScore == BeforeOutput.rawScore && Output.effectiveScore == BeforeOutput.effectiveScore &&
				Trace.candidates.data() == TraceData && Trace.candidates.size() == BeforeTrace.candidates.size() && Trace.leading == BeforeTrace.leading && Trace.leadingScore == BeforeTrace.leadingScore && Trace.currentEligible == BeforeTrace.currentEligible && Trace.currentScore == BeforeTrace.currentScore && Trace.challenger == BeforeTrace.challenger && Trace.challengerScore == BeforeTrace.challengerScore && Trace.dwellRemainingMs == BeforeTrace.dwellRemainingMs && Trace.acquireThreshold == BeforeTrace.acquireThreshold && Trace.releaseThreshold == BeforeTrace.releaseThreshold && Trace.switchMargin == BeforeTrace.switchMargin && Trace.overrideMargin == BeforeTrace.overrideMargin &&
				Trace.candidates[0].id == BeforeTrace.candidates[0].id && Trace.candidates[0].effectiveScore == BeforeTrace.candidates[0].effectiveScore && Trace.candidates[0].noveltyGate == BeforeTrace.candidates[0].noveltyGate && Workspace.CandidateCapacity() == InitialCapacity && Workspace.TraceCapacity() == InitialTraceCapacity;
		};
		TestTrue(TEXT("backward time propagated; state untouched"), Workspace.Step(Observer, Candidates, Profile, 99, Memory, Output, true) == interest::StepStatus::TimeWentBackwards && Unchanged());
		auto BadObserver = Observer;
		BadObserver.referenceForward = IE::ToKernelDirection(FVector(std::numeric_limits<double>::max(), 0.0, 0.0));
		TestTrue(TEXT("invalid narrowed direction propagated"), Workspace.Step(BadObserver, Candidates, Profile, 101, Memory, Output, true) == interest::StepStatus::InvalidObserver && Unchanged());
		BadObserver.position = IE::ToKernelPosition(RawVector(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0));
		TestTrue(TEXT("invalid position propagated"), Workspace.Step(BadObserver, Candidates, Profile, 101, Memory, Output, true) == interest::StepStatus::InvalidObserver && Unchanged());
		IE::FProfileOwner BadProfile(Settings({}));
		TestTrue(TEXT("profile validation precedes observer/time"), Workspace.Step(BadObserver, Candidates, BadProfile, 99, Memory, Output, true) == interest::StepStatus::InvalidProfile && Unchanged());
		std::vector<interest::CandidateSnapshot> TooMany(InitialCapacity + 1, Candidate(2, 1.f));
		TestTrue(TEXT("capacity failure never grows buffers"), Workspace.Step(Observer, TooMany, Profile, 101, Memory, Output, true) == interest::StepStatus::InsufficientCapacity && Unchanged());
		Candidates[0].spatial = interest::SpatialKind::Bearing;
		Candidates[0].bearing = IE::ToKernelDirection(FVector::ZeroVector);
		TestTrue(TEXT("candidate error remains candidate rejection"), Workspace.Step(Observer, Candidates, Profile, 101, Memory, Output, true) == interest::StepStatus::Ok && Workspace.GetTrace().candidates[0].eligibility == interest::Eligibility::InvalidGeometry && Output.kind == interest::DecisionKind::Clear);
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefaultNovelty, "InterestEngine.Adapter.DefaultNoveltyCompatibility",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	bool FDefaultNovelty::RunTest(const FString&)
	{
		std::array<interest::ContextPreference, 1> Contexts{};
		auto Config = Settings(Contexts);
		Config.noveltyBeta = 0.5f;
		IE::FProfileOwner Profile(Config);
		IE::FStepWorkspace Workspace;
		TestTrue(TEXT("prepare default novelty fixture"), Workspace.Prepare(3, true));
		interest::ObserverSnapshot Observer;
		interest::ObserverMemory Memory{};
		interest::Decision Output{};
		std::array Candidates{Candidate(1, 0.5f), Candidate(2, 0.5f), Candidate(3, 1.f)};
		const std::array Locations{FVector(500.0, 0.0, 0.0), FVector(0.0, 500.0, 0.0), FVector(-500.0, 0.0, 0.0)};
		for (std::size_t I = 0; I < Candidates.size(); ++I)
		{
			Candidates[I].spatial = interest::SpatialKind::Point;
			Candidates[I].position = IE::ToKernelPosition(Locations[I]);
			Candidates[I].eventTimeMs = 1000;
			Candidates[I].eventStrength = 1.f;
			TestTrue(TEXT("omitted directionality defaults to legacy gating"), Candidates[I].eventDirectionality == 1.f);
		}
		Candidates[1].perceptionStrength = 0.5f;
		TestTrue(TEXT("legacy acquisition with aged events"), Workspace.Step(Observer, Candidates, Profile, 2000, Memory, Output, true) == interest::StepStatus::Ok && Output.kind == interest::DecisionKind::Acquire && Output.selected == 1 && Output.rawScore == 0.5f && Output.effectiveScore == 0.75f);
		const auto DefaultTrace = Workspace.GetTrace();
		const std::array ExpectedGates{1.f, 0.25f, 0.f};
		const std::array ExpectedRaw{0.5f, 0.125f, 0.f};
		const std::array ExpectedEffective{0.75f, 0.1875f, 0.f};
		if (!TestTrue(TEXT("trace contains every default candidate"), DefaultTrace.candidates.size() == Candidates.size())) { return false; }
		for (std::size_t I = 0; I < Candidates.size(); ++I)
		{
			const auto& Entry = DefaultTrace.candidates[I];
			TestTrue(TEXT("legacy raw/effective scores and exact rounded gate reuse"), Entry.eligibility == interest::Eligibility::Eligible && Entry.gate == ExpectedGates[I] && Entry.noveltyGate == Entry.gate && Entry.rawScore == ExpectedRaw[I] && Entry.eventNovelty == 0.5 && Entry.effectiveScore == ExpectedEffective[I]);
			Candidates[I].eventDirectionality = 1.f;
		}
		Memory = {};
		TestTrue(TEXT("explicit default matches omitted default decision"), Workspace.Step(Observer, Candidates, Profile, 2000, Memory, Output, true) == interest::StepStatus::Ok && Output.kind == interest::DecisionKind::Acquire && Output.selected == 1 && Output.effectiveScore == 0.75f);
		for (std::size_t I = 0; I < Candidates.size(); ++I)
		{
			const auto& Entry = Workspace.GetTrace().candidates[I];
			TestTrue(TEXT("explicit default matches omitted default trace"), Entry.rawScore == DefaultTrace.candidates[I].rawScore && Entry.noveltyGate == DefaultTrace.candidates[I].noveltyGate && Entry.noveltyContribution == DefaultTrace.candidates[I].noveltyContribution && Entry.effectiveScore == DefaultTrace.candidates[I].effectiveScore);
		}
		return true;
	}

	IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNoveltyDirectionality, "InterestEngine.Adapter.NoveltyDirectionality",
		EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
	bool FNoveltyDirectionality::RunTest(const FString&)
	{
		std::array<interest::ContextPreference, 1> Contexts{};
		auto Config = Settings(Contexts);
		Config.noveltyBeta = 1.f;
		IE::FProfileOwner Profile(Config);
		IE::FStepWorkspace Workspace;
		TestTrue(TEXT("prepare directionality fixture"), Workspace.Prepare(1, true));
		interest::ObserverSnapshot Observer;
		interest::ObserverMemory Memory{};
		interest::Decision Output{};
		std::array Candidates{Candidate(42, 1.f)};
		auto& Event = Candidates[0];
		Event.spatial = interest::SpatialKind::Point;
		Event.position = IE::ToKernelPosition(FVector(-500.0, 0.0, 0.0));
		Event.eventTimeMs = 1000;
		Event.eventStrength = 1.f;
		auto Run = [&]()
		{
			Memory = {};
			return Workspace.Step(Observer, Candidates, Profile, 1000, Memory, Output, true) == interest::StepStatus::Ok;
		};
		for (float Directionality : {1.f, 0.5f, 0.f})
		{
			Event.eventDirectionality = Directionality;
			if (!TestTrue(TEXT("behind event steps successfully"), Run())) { return false; }
			const auto& Entry = Workspace.GetTrace().candidates[0];
			const float Expected = 1.f - Directionality;
			TestTrue(TEXT("only novelty gains angular independence"), Entry.angleGate == 0.f && Entry.gate == 0.f && Entry.rawScore == 0.f && Entry.noveltyGate == Expected && Entry.noveltyContribution == Expected && Entry.effectiveScore == Expected);
			TestTrue(TEXT("behind event acquisition follows novelty threshold"), Output.kind == (Directionality == 1.f ? interest::DecisionKind::None : interest::DecisionKind::Acquire) && Output.selected == (Directionality == 1.f ? 0 : 42));
		}
		Event.position = IE::ToKernelPosition(FVector(-1500.0, 0.0, 0.0));
		Event.perceptionStrength = 0.5f;
		TestTrue(TEXT("distance falloff and partial perception still attenuate novelty"), Run() && Workspace.GetTrace().candidates[0].distanceGate == 0.5f && Workspace.GetTrace().candidates[0].noveltyGate == 0.25f && Workspace.GetTrace().candidates[0].effectiveScore == 0.25f && Output.kind == interest::DecisionKind::None);
		Event.position = IE::ToKernelPosition(FVector(-2500.0, 0.0, 0.0));
		TestTrue(TEXT("direction-independent novelty cannot bypass profile range"), Run() && Workspace.GetTrace().candidates[0].eligibility == interest::Eligibility::OutOfRange && Workspace.GetTrace().candidates[0].noveltyGate == 0.f && Output.selected == 0);
		Event.position = IE::ToKernelPosition(FVector(-1500.0, 0.0, 0.0));
		Event.maxDistance = 1000.f;
		TestTrue(TEXT("direction-independent novelty cannot bypass candidate range"), Run() && Workspace.GetTrace().candidates[0].eligibility == interest::Eligibility::OutOfRange && Output.selected == 0);
		Event.maxDistance = 0.f;
		Event.position = IE::ToKernelPosition(FVector(-500.0, 0.0, 0.0));
		Event.perceptionStrength = 0.f;
		TestTrue(TEXT("direction-independent novelty cannot bypass zero perception"), Run() && Workspace.GetTrace().candidates[0].eligibility == interest::Eligibility::NotPerceived && Workspace.GetTrace().candidates[0].noveltyGate == 0.f && Output.selected == 0);
		Event.perceptionStrength = 1.f;
		Event.eventDirectionality = std::numeric_limits<float>::quiet_NaN();
		TestTrue(TEXT("invalid directionality is a candidate error, not a failed step"), Run() && Workspace.GetTrace().candidates[0].eligibility == interest::Eligibility::InvalidEvent && Output.selected == 0);
		return true;
	}
}
#endif
