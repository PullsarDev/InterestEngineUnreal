# Interest Engine for Unreal

A native C++ runtime plugin that lets an Unreal Engine 5.8 game answer one question per
observer, deterministically: **of the things around this character, which single one holds its
attention right now?** It wraps the [Interest Engine](https://github.com/PullsarDev/InterestEngine)
attention-selection kernel (a small standard-C++ library) and adds the Unreal-side pieces a
host needs: type conversions, owned profile storage and prepared workspaces.

You give it candidates (a person, a sound, a pickup, a scripted cue), each with scores you
computed, plus the observer's position, facing and the current time. It returns one decision:
acquire a target, keep it, switch, clear, or nothing, with a reason and, if you ask, a
per-candidate trace of how the scores were reached. A target is held through brief dips and
near-ties instead of flickering between candidates.

> **What this plugin does not do.** The adapter has **no perception, no actor discovery, no
> motion detection, no gaze or head/eye animation, no replication and no Blueprint API.** The
> kernel does not see your world. It never detects movement or sound: **your game decides what the
> candidates are and supplies their `attraction`, `salience`, `perceptionStrength` and event
> values.** The kernel scores those numbers, applies range, angle and tag rules, and runs the
> hold/switch policy. Everything around that, such as gathering candidates, mapping actors to IDs,
> deriving salience from motion, and driving the head or eyes toward the result, is your code.

Version 0.2.1 (beta). The plugin version and the vendored kernel version are separate; the kernel
is **v0.2.0** and `InterestEngine::KernelVersion()` reports it.

## Requirements and verified platform support

| | |
| --- | --- |
| Unreal Engine | **5.8** (verified on 5.8.4) |
| Platform | **Windows, Win64 only.** The plugin descriptor lists `Win64` as its only supported target platform, so other platforms are not built. |
| Toolchain | The Visual Studio C++ toolchain and Windows SDK that Unreal Engine 5.8 requires. Verified with the versions listed under [Build verification](#build-verification). |
| Language | C++20 (set by the module; your consuming module should set it too). |
| Dependencies | None beyond the engine's `Core` module. The kernel is vendored. |

Verified: Win64 Editor Development, Game Development and Game Shipping builds, each with unity
and non-unity compilation, plus automation tests in the Editor. Not verified: other platforms,
other engine versions, packaged and cooked execution. See [Known limitations](#known-limitations).

## Installation

1. Put this repository at `<YourProject>/Plugins/InterestEngineUnreal/`:

   ```powershell
   git clone https://github.com/PullsarDev/InterestEngineUnreal.git Plugins/InterestEngineUnreal
   ```

2. **The kernel is already included.** The unchanged Interest Engine v0.2.0 sources are vendored
   in `ThirdParty/InterestEngine/` and pinned to a specific upstream commit (see
   [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)). You do not clone the kernel repository, install
   CMake or build a separate library, and you must not add the kernel's `.cpp` files to your own
   module. Unreal Build Tool compiles them once, inside the plugin's module.

3. Enable the plugin in your `.uproject`:

   ```json
   "Plugins": [{ "Name": "InterestEngineUnreal", "Enabled": true, "SupportedTargetPlatforms": ["Win64"] }]
   ```

4. Add the module to the `Build.cs` of the module that will call it:

   ```csharp
   CppStandard = CppStandardVersion.Cpp20;
   PrivateDependencyModuleNames.AddRange(new[] { "Core", "InterestEngineRuntime" });
   ```

   Use `PublicDependencyModuleNames` if your own public headers expose adapter or kernel types.
   Some examples below also use `CoreUObject`, `Engine` and `GameplayTags`.

5. Regenerate project files and build your project with Unreal Build Tool.

Build every module with the same engine version and toolchain. The public types include
`std::span` and `std::vector`, so the interface is not a stable ABI across toolchains or engine
versions.

## Quick start

This is the whole flow in one function: describe how candidates are scored and held (the
*profile*), prepare a workspace, describe the observer and its candidates, and take one step. A
real game keeps the profile, workspace and memory alive between updates; see
[Per-observer state](#per-observer-state-and-arrays).

```cpp
#include "CoreMinimal.h"
#include "InterestEngineAdapter.h"

#include <array>

// One observer, one candidate, one step. Returns the decision.
interest::Decision MinimalInterestExample()
{
	// Profile: how candidates are scored and when a target is kept or replaced.
	// Distances are in Unreal units (centimetres), the same units as positions.
	std::array<interest::ContextPreference, 1> Contexts{};
	interest::AttentionProfile Settings{};
	Settings.contexts = Contexts;
	Settings.fullRange = 1000.f;      // full strength out to 10 m
	Settings.maxRange = 3000.f;       // nothing beyond 30 m is considered
	Settings.acquireThreshold = 0.5f; // a target needs a score of at least this
	Settings.releaseThreshold = 0.35f;
	Settings.switchMargin = 0.1f;     // a challenger must lead by this to replace the target
	Settings.overrideMargin = 0.3f;   // ...or by this during the minimum dwell
	Settings.minDwellMs = 400;
	InterestEngine::FProfileOwner Profile(Settings); // copies Settings and its contexts
	InterestEngine::FStepWorkspace Workspace;
	if (Profile.Validate() != interest::StepStatus::Ok || !Workspace.Prepare(/*CandidateCapacity=*/64, /*bPrepareTrace=*/true))
	{
		return {};
	}

	// Observer: where it is and which way its body or travel is facing.
	interest::ObserverSnapshot Observer{};
	Observer.position = InterestEngine::ToKernelPosition(FVector(100.0, 200.0, 50.0));
	Observer.referenceForward = InterestEngine::ToKernelDirection(FVector::ForwardVector);

	// Candidates: your actors, each with an ID you own and scores you computed.
	std::array<interest::CandidateSnapshot, 1> Candidates{};
	Candidates[0].id = 42; // stable and nonzero; 0 means "no candidate"
	Candidates[0].position = InterestEngine::ToKernelPosition(FVector(600.0, 200.0, 50.0));
	Candidates[0].attraction = 0.8f; // lasting appeal, [0, 1]
	Candidates[0].salience = 0.7f;   // momentary prominence, [0, 1]

	// Time: one explicit clock, nondecreasing for this observer.
	interest::TimeMs Now{};
	if (!InterestEngine::TryToKernelTime(/*Seconds=*/1.25, Now))
	{
		return {};
	}

	// Memory: this observer's temporal state. Keep one per observer across steps.
	interest::ObserverMemory Memory{};
	interest::Decision Decision{};
	const interest::StepStatus Status = Workspace.Step(Observer, Candidates, Profile, Now, Memory, Decision, /*bTrace=*/true);
	if (Status != interest::StepStatus::Ok)
	{
		UE_LOG(LogTemp, Error, TEXT("Interest step failed: %s"), UTF8_TO_TCHAR(InterestEngine::ToString(Status)));
		return {};
	}
	// Decision.kind is Acquire here: the candidate is ahead, in range and scores 0.67.
	// Resolve Decision.selected back to your own object; 0 means no target.
	return Decision;
}
```

`Step` is the call that makes the decision, and the only one that updates an observer's memory. By
the kernel's documented contract it does not allocate; the adapter keeps no references to your data
and uses no clock of its own. If it returns anything other than `Ok`, your memory and output are
untouched. `Decision.kind` is `None`, `Acquire`, `Keep`, `Switch` or `Clear`.

## Candidate IDs and candidate types

**IDs.** The kernel works on `interest::CandidateId`, a 64-bit integer you choose. It must be
nonzero (0 is `kNoCandidate`), unique within one `Step` call and stable for as long as you want the
kernel to recognise the same thing across steps. Its temporal policy compares IDs between steps, so
reusing an ID for a different object looks like the same target. A candidate that violates this is
rejected individually (`InvalidId`, `DuplicateId`) and the rest of the step proceeds. A simple
approach is a host-owned counter plus a map to a weak object pointer:

```cpp
#include "CoreMinimal.h"
#include "InterestEngineAdapter.h"
#include "GameFramework/Actor.h"
#include "UObject/WeakObjectPtrTemplates.h"

// Maps live actors to stable, nonzero candidate IDs and back.
class FCandidateRegistry
{
public:
	interest::CandidateId IdFor(const AActor& Actor)
	{
		if (const interest::CandidateId* Existing = IdByActor.Find(&Actor))
		{
			return *Existing;
		}
		const interest::CandidateId Id = NextId++;
		IdByActor.Add(&Actor, Id);
		ActorById.Add(Id, &Actor);
		return Id;
	}

	// Null once the actor has been destroyed.
	const AActor* Resolve(interest::CandidateId Id) const
	{
		const TWeakObjectPtr<const AActor>* Found = ActorById.Find(Id);
		return Found ? Found->Get() : nullptr;
	}

	void Forget(const AActor& Actor)
	{
		interest::CandidateId Id = 0;
		if (IdByActor.RemoveAndCopyValue(&Actor, Id))
		{
			ActorById.Remove(Id);
		}
	}

private:
	interest::CandidateId NextId = 1;
	TMap<TWeakObjectPtr<const AActor>, interest::CandidateId> IdByActor;
	TMap<interest::CandidateId, TWeakObjectPtr<const AActor>> ActorById;
};
```

**Types.** `CandidateSnapshot::spatial` says how the kernel uses geometry:

| `SpatialKind` | Geometry you supply | What the kernel applies |
| --- | --- | --- |
| `Point` | `position` in world space, optional `maxDistance` | Distance falloff and cutoff, and the angle between the observer's forward and the point. |
| `Bearing` | `bearing`, a world-space direction (any nonzero length) | The angle gate only. There is no distance. |
| `NonSpatial` | none | Neither: only the candidate's scores, perception and tags matter. |

```cpp
#include "CoreMinimal.h"
#include "InterestEngineAdapter.h"

// A place in the world: scored by distance and by how far it is from the observer's facing.
interest::CandidateSnapshot MakePointCandidate(interest::CandidateId Id, const FVector& WorldLocation)
{
	interest::CandidateSnapshot Candidate;
	Candidate.id = Id;
	Candidate.spatial = interest::SpatialKind::Point;
	Candidate.position = InterestEngine::ToKernelPosition(WorldLocation);
	return Candidate;
}

// A direction without a distance, such as an off-screen sound or a threat bearing.
interest::CandidateSnapshot MakeBearingCandidate(interest::CandidateId Id, const FVector& WorldDirection)
{
	interest::CandidateSnapshot Candidate;
	Candidate.id = Id;
	Candidate.spatial = interest::SpatialKind::Bearing;
	Candidate.bearing = InterestEngine::ToKernelDirection(WorldDirection);
	return Candidate;
}

// No geometry at all, such as a scripted cue or a UI prompt.
interest::CandidateSnapshot MakeNonSpatialCandidate(interest::CandidateId Id)
{
	interest::CandidateSnapshot Candidate;
	Candidate.id = Id;
	Candidate.spatial = interest::SpatialKind::NonSpatial;
	return Candidate;
}
```

The other per-candidate inputs are all host-supplied: `attraction` (relatively persistent
appeal), `salience` (momentary prominence, for example higher while something is moving or
making noise), `perceptionStrength` (scales the candidate's score; 0 means not perceived and the
candidate is rejected), and `enabled`. Scores are in [0, 1]. A non-finite or out-of-range value rejects that candidate as
`InvalidValue`; the adapter never clamps or repairs input.

## Per-observer state and arrays

Each observer needs its own `interest::ObserverMemory`. The profile is shared configuration, and
a workspace holds scratch storage that sequential observers can reuse. `Step` takes candidates as
`std::span<const interest::CandidateSnapshot>`. Build the span explicitly from the `TArray`'s data
pointer and count, as below, rather than relying on an implicit conversion. An empty array is fine.

```cpp
#include "CoreMinimal.h"
#include "InterestEngineAdapter.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include <cstddef>
#include <span>

// One instance per attention-driven actor. Call Update from one thread at a time.
class FAttentionObserver
{
public:
	FAttentionObserver(const interest::AttentionProfile& Settings, int32 MaxCandidates)
		: Profile(Settings)
	{
		bReady = Profile.Validate() == interest::StepStatus::Ok
			&& Workspace.Prepare(MaxCandidates, /*bPrepareTrace=*/true);
	}

	// Returns the selected candidate ID, or 0 for none (also when the step could not run).
	interest::CandidateId Update(const AActor& Body, double TimeSeconds,
		const TArray<interest::CandidateSnapshot>& Candidates)
	{
		interest::TimeMs Now{};
		if (!bReady || !InterestEngine::TryToKernelTime(TimeSeconds, Now))
		{
			return 0;
		}
		interest::ObserverSnapshot Observer{};
		Observer.position = InterestEngine::ToKernelPosition(Body.GetActorLocation());
		// A body or travel direction, never the head or eye direction that this result drives.
		Observer.referenceForward = InterestEngine::ToKernelDirection(Body.GetActorForwardVector());

		const std::span<const interest::CandidateSnapshot> View(Candidates.GetData(), static_cast<std::size_t>(Candidates.Num()));
		const interest::StepStatus Status = Workspace.Step(Observer, View, Profile, Now, Memory, Last, /*bTrace=*/true);
		if (Status != interest::StepStatus::Ok)
		{
			UE_LOG(LogTemp, Warning, TEXT("Interest step failed: %s"), UTF8_TO_TCHAR(InterestEngine::ToString(Status)));
			return Memory.current; // unchanged: still the previous successful result
		}
		return Last.selected;
	}

	const interest::Decision& LastDecision() const { return Last; }
	const InterestEngine::FStepWorkspace& GetWorkspace() const { return Workspace; }

	// Call when the observer teleports, respawns or its time source restarts.
	void ResetAttention() { InterestEngine::ResetMemory(Memory); Last = {}; }

private:
	InterestEngine::FProfileOwner Profile;
	InterestEngine::FStepWorkspace Workspace;
	interest::ObserverMemory Memory{};
	interest::Decision Last{};
	bool bReady = false;
};
```

`Prepare` may allocate, so call it outside the update path. Its capacity only grows. A step with
more candidates than the prepared capacity returns `InsufficientCapacity` and changes nothing;
nothing grows automatically.

## Contexts, affinity and tag masks

A **context** is a named set of preferences for one kind of observer or mood: for example, calm
versus wary. `AttentionProfile::contexts` is an array of `interest::ContextPreference`, and
`ObserverSnapshot::context` is the index of the one in force for this step. A context decides:

- **Eligibility by tag.** A candidate must carry every bit of `requiredTags` and none of
  `excludedTags`, or it is rejected (`MissingRequiredTag`, `ExcludedTag`).
- **Affinity.** `tagAffinity[bit]`, in [0, 1], is how much this context likes tag `bit`. For a
  candidate, the affinity is the **largest** `tagAffinity` among the bits it carries that are also in
  `affinityTags`. A candidate with none of those bits gets `fallbackAffinity`. Affinity is mixed
  with `attraction` and `salience` using the profile's three weights.

**Tags are a 64-bit mask** (`interest::TagMask`); bit *n* pairs with `tagAffinity[n]`. The kernel
defines no meaning for any bit: the mapping from your game's tags to bits is yours. The snippet
below maps Gameplay Tags (add `GameplayTags` to your `Build.cs`) to bit positions, then builds a
context that is drawn to threats, mildly curious about everything else tagged, and never considers
hidden things.

```cpp
#include "CoreMinimal.h"
#include "InterestEngineAdapter.h"
#include "GameplayTagContainer.h"

#include <array>

// Host-owned: gives each gameplay tag you care about one of the 64 bit positions.
class FInterestTagMap
{
public:
	void Add(const FGameplayTag& Tag, int32 Bit)
	{
		check(Bit >= 0 && Bit < static_cast<int32>(interest::kTagCount));
		BitByTag.Add(Tag, Bit);
	}

	// A candidate carries every mapped tag its container matches (parent tags included).
	interest::TagMask ToMask(const FGameplayTagContainer& Tags) const
	{
		interest::TagMask Mask = 0;
		for (const TPair<FGameplayTag, int32>& Entry : BitByTag)
		{
			if (Tags.HasTag(Entry.Key))
			{
				Mask |= interest::TagMask{1} << Entry.Value;
			}
		}
		return Mask;
	}

private:
	TMap<FGameplayTag, int32> BitByTag;
};

namespace TagBits
{
	constexpr int32 Threat = 0;
	constexpr int32 Curiosity = 1;
	constexpr int32 Hidden = 2;
}

constexpr interest::TagMask Bit(int32 Index) { return interest::TagMask{1} << Index; }

// Context 0 is the profile default; context 1 is the wary mood.
std::array<interest::ContextPreference, 2> MakeContexts()
{
	std::array<interest::ContextPreference, 2> Contexts{};
	// Context 0: no tag rules, mild interest in anything (fallbackAffinity defaults to 0.5).

	interest::ContextPreference& Wary = Contexts[1];
	Wary.excludedTags = Bit(TagBits::Hidden);
	Wary.affinityTags = Bit(TagBits::Threat) | Bit(TagBits::Curiosity);
	Wary.tagAffinity[TagBits::Threat] = 0.9f;
	Wary.tagAffinity[TagBits::Curiosity] = 0.4f;
	Wary.fallbackAffinity = 0.2f; // anything carrying neither tag
	return Contexts;
}
```

Set `CandidateSnapshot::tags` from `FInterestTagMap::ToMask` and `ObserverSnapshot::context` to
1 to use the wary context. Contexts are copied into the `FProfileOwner` when it is built, so
changing a context means building a new owner.

## Events: timing and strength

Salience is a score you recompute whenever you like. An **event** is a short, timestamped burst
on a candidate, such as a footstep, a flash or a door slam, that adds a novelty bonus which
fades with time:

- `eventTimeMs` is when the event happened, on the **same clock** you pass as `Now`, and not in the
  future. `eventStrength` in [0, 1] is how strong it was; **0 means no event** and the time is
  ignored.
- At each step the kernel computes `strength * 2^(-age / noveltyHalfLifeMs)`, weights it by
  `noveltyBeta` and the candidate's distance and perception gating, and adds it to the score,
  capped at 1. With `noveltyBeta = 0` events have no effect (they are still validated).
- `eventDirectionality` (default 1) controls the angular part of that gating only: 1 keeps the
  usual rule that something behind the observer counts for little, 0 ignores the angle, and values
  between interpolate. Distance, range and perception still apply.
- The kernel keeps **no event history**. Resubmit the latest event every step while it is
  relevant; leave `eventStrength` at 0 and the term disappears. A positive strength with a time
  later than `Now`, or a strength or directionality outside [0, 1], rejects the candidate as
  `InvalidEvent`.

```cpp
#include "CoreMinimal.h"
#include "InterestEngineAdapter.h"

// Enable the novelty term in a profile before building the FProfileOwner.
void EnableNovelty(interest::AttentionProfile& Settings)
{
	Settings.noveltyBeta = 0.5f;          // weight of the novelty bonus, [0, 1]
	Settings.noveltyHalfLifeMs = 1500;    // an event's bonus halves every 1.5 s
}

// Attach (or refresh) an event on a candidate. EventSeconds uses the same clock as the step time.
void SetEvent(interest::CandidateSnapshot& Candidate, double EventSeconds, float Strength)
{
	interest::TimeMs EventTime{};
	if (Strength > 0.f && InterestEngine::TryToKernelTime(EventSeconds, EventTime))
	{
		Candidate.eventTimeMs = EventTime;
		Candidate.eventStrength = FMath::Clamp(Strength, 0.f, 1.f);
		Candidate.eventDirectionality = 0.5f; // a half-weighted angular restriction
	}
}
```

## Time and temporal state

The kernel reads no clock. You pass `Now` in milliseconds through `TryToKernelTime`, which
computes `floor(Seconds * 1000)` and rejects non-finite or out-of-range values, leaving its output
untouched. Choose one clock per observer, nondecreasing. `UWorld::GetTimeSeconds()` pauses with
the game; a real-time clock does not. The choice is yours. A time that goes backwards makes the
step return `TimeWentBackwards` and change nothing. Dwell is measured in this host time and
checked only at steps, so its resolution is your update interval.

Between steps, the *only* state the kernel keeps for an observer is its `interest::ObserverMemory`
(the current target, when it was selected, the last step time). Keep one per observer.

- **Create** it value-initialized: `interest::ObserverMemory Memory{};`.
- **Reset** it with `InterestEngine::ResetMemory(Memory)` (equivalent to `Memory = {}`) when the
  observer should forget its target: respawn, teleport, level change, or a restarted time source.
  The next step then behaves like the first one, and time may start over from any value.
  Resetting does not make an old candidate event new.
- **Do not edit or copy** another observer's memory, except to clone its attention deliberately.
  The kernel does not validate edited memory.
- **Destroying** an observer needs no kernel call: drop its memory.

## Reading the decision and the trace

`Decision` carries `kind`, `reason`, `selected`, `previous`, `rawScore` and `effectiveScore`.
`reason` explains why the policy did what it did, for example `HighestScore`, `MinimumDwell`
(the current target is held although something else leads), `InsufficientAdvantage` (a challenger
leads but not by enough), `BelowReleaseThreshold` or `CurrentInvalid`. When you pass `bTrace`
and the step succeeds, `Workspace.GetTrace()` explains the same computation: per candidate its
`eligibility`, gates, preference, raw and effective scores, and the leading candidate, current
target and challenger with the thresholds compared. Prepare the workspace with
`bPrepareTrace = true` to use it.

```cpp
#include "CoreMinimal.h"
#include "InterestEngineAdapter.h"

// Log a decision and, if the last successful step was traced, why each candidate scored as it did.
void LogDecision(const interest::Decision& Decision, const InterestEngine::FStepWorkspace& Workspace)
{
	UE_LOG(LogTemp, Log, TEXT("%s (%s): selected %llu, previous %llu, score %.3f"),
		UTF8_TO_TCHAR(InterestEngine::ToString(Decision.kind)),
		UTF8_TO_TCHAR(InterestEngine::ToString(Decision.reason)),
		Decision.selected, Decision.previous, Decision.effectiveScore);
	for (const interest::CandidateTrace& Entry : Workspace.GetTrace().candidates)
	{
		UE_LOG(LogTemp, Log, TEXT("  #%llu %s raw %.3f effective %.3f (distance gate %.2f, angle gate %.2f)"),
			Entry.id, UTF8_TO_TCHAR(InterestEngine::ToString(Entry.eligibility)),
			Entry.rawScore, Entry.effectiveScore, Entry.distanceGate, Entry.angleGate);
	}
}
```

The trace is borrowed from the workspace: read it before the next traced step, `Prepare`, or the
workspace's destruction. An untraced or failed step leaves it showing the previous traced result.
A decision is not proof that your presentation is right: a character turning its head toward
`Decision.selected` is your code, so check decisions against `Decision` and the trace, not against
what you see on screen.

## Threading and lifetime

- A **workspace**, an **observer memory** and the **trace** may be used by one call at a time.
  Every method needs exclusive access, including `Prepare`, `GetTrace` and destruction. Observers
  that update one after another can share a workspace; concurrent workers need a workspace each and
  their own memory, or external locking.
- An **`FProfileOwner`** is immutable, noncopyable and nonmovable. Several workers may share one
  while its lifetime is guaranteed. Any profile from `Get()` borrows the owner's storage.
- The `contexts` span in the settings you pass to the owner must be valid during construction only.
- `Step` keeps no references to your observer, candidates or profile. Those must stay valid and
  unchanged throughout the call.
- The adapter touches no `UObject`, takes no lock and starts no thread. Copy what you need out of
  actors into plain snapshots on the game thread, then step wherever you like.

## API reference

All entry points are in `InterestEngineAdapter.h`, namespace `InterestEngine`. Kernel structs
(`interest::ObserverSnapshot`, `CandidateSnapshot`, `AttentionProfile`, `Decision` and so on) are
used directly; their fields are documented in
[`attention.hpp`](ThirdParty/InterestEngine/include/interest/attention.hpp).

| Entry point | Purpose |
| --- | --- |
| `ToKernelPosition(FVector)` | Position to `Vec3d`, double precision, same axes and units. |
| `ToKernelDirection(FVector)` | Direction to `Vec3f`; not normalized, finite overflow becomes infinity so the kernel rejects it. |
| `TryToKernelTime(double, TimeMs&)` | Seconds to milliseconds; false for non-finite or out-of-range input. |
| `KernelVersion()` | Vendored kernel version string. |
| `ToString(StepStatus / DecisionKind / DecisionReason / Eligibility)` | Enumerator names; `"Unknown"` for an invalid value. |
| `ResetMemory(ObserverMemory&)` | Return one observer to "never stepped". |
| `FProfileOwner` | Owns a copy of a profile and its contexts. `Get()`, `Validate()`. |
| `FStepWorkspace` | Prepared scratch and trace storage. `Prepare`, `Step`, `GetTrace`, capacity getters. |

The kernel's free functions are **not** exported from the module. In an Editor build, where
modules are separate DLLs, calling `interest::Step`, `Reserve`, `ValidateProfile`, `ResetMemory`,
`VersionString` or `ToString` directly fails at link time, while a monolithic Game build links by
accident. Every one of them has an adapter entry point above; use those.

## Build verification

`Tests/Validate.ps1` generates a small throwaway host project, copies the plugin into it and
builds a separate C++ module that calls the exported adapter API from outside the plugin, as a
real consumer does. It compiles every C++ block in this README, runs the first one, and checks that
the runtime module was compiled with `/fp:precise` and C++20. Nothing is built in the plugin
directory.

```powershell
.\Tests\Validate.ps1 -Matrix -WorkRoot C:\IEUV     # all six configurations
.\Tests\Validate.ps1 -Target Editor -Configuration Development -Mode Unity -RunTests
.\Tests\Validate.ps1 -Target Game -Configuration Shipping -Mode NonUnity -EngineRoot "D:\UE_5.8"
```

You need Windows, PowerShell 5.1 or 7, an Unreal Engine 5.8 installation (the script finds a
launcher install, or take `-EngineRoot`) and the engine's required Visual Studio C++ toolchain. A
standalone clone of this repository is enough. **Keep `-WorkRoot` short.** Unreal's intermediate
paths nest deeply (the longest generated path is about 180 characters below the work root) and the
Windows limit is 260, so the script rejects a work root longer than 60 characters. The default,
`.local\Validate` inside the clone, therefore needs a clone path of about 44 characters or fewer;
otherwise pass something like `-WorkRoot C:\IEUV`. Logs and the automation report go to
`<WorkRoot>\<Target>-<Configuration>-<Mode>\Reports`. Each configuration leaves about 3 GB under
the work root (mostly the engine's shared precompiled headers), so the full matrix needs about
17 GB; delete the work root when you are done. If your engine build needs it, forward extra
Unreal Build Tool arguments with `-AdditionalBuildArguments`.

The eight automation tests cover conversions and time boundaries, profile lifetime, preparation
and capacity, decisions and independent observers, failure propagation, default and directional
novelty, the string and reset wrappers, and the first README example. They are compiled out of
Shipping; there, startup also executes a smoke check from the consumer module so every
configuration links and calls the adapter.

### Verified results

Plugin 0.2.1 with kernel v0.2.0, validated on 2026-10-29 from clean generated hosts with
**Unreal Engine 5.8.4 (CL 58902052)**, Windows 11, the Visual Studio 14.44.35229 toolchain
(MSVC 14.44.35207) and Windows SDK 10.0.26100.0:

| Target / configuration | Plugin unity | Plugin non-unity | Execution |
| --- | --- | --- | --- |
| Editor Development | Build and link passed | Build and link passed | Startup check and 8 of 8 automation tests passed in each mode, no test warnings |
| Game Development | Build and link passed | Build and link passed | Compile and link only |
| Game Shipping | Build and link passed | Build and link passed | Compile and link only; automation compiled out |

All six configurations built with no compiler warnings, and the README examples compile in each.
The matrix ran under PowerShell 7; Editor Development with the tests also ran under Windows
PowerShell 5.1. In every configuration the runtime module's compile options include `/fp:precise`
and `/std:c++20`. Unity builds compile the plugin as one translation unit including the kernel
bridge; non-unity builds compile each file separately. `dumpbin /exports` on the Editor runtime
DLL lists exactly the `InterestEngine::` adapter symbols and no raw kernel functions. The five
vendored kernel files match the Git blob hashes of the pinned upstream commit.

Not verified: packaged and cooked execution, other platforms, other engine versions, sanitizers,
Unreal heap-allocation counts and performance.

If PowerShell refuses to run the script because of its execution policy, start it with
`powershell -ExecutionPolicy Bypass -File .\Tests\Validate.ps1 ...`.

## Known limitations

- Windows (Win64) and Unreal Engine 5.8 only. Other platforms are excluded by the plugin
  descriptor and untested; other engine versions are untested.
- Native C++ only: no Blueprint, reflected or replicated API. Nothing here ticks or runs on its
  own; you call `Step`.
- No packaged or cooked execution, sanitizer runs, Unreal heap-allocation counting, or
  benchmarks are part of this repository. That `Step` does not allocate is the kernel's documented
  contract; the tests check that prepared buffers are not reallocated.
- The public header exposes standard-library types and kernel structs directly, so there is no
  stable ABI. Rebuild the plugin and every consumer together; do not mix binaries from different
  kernel versions.
- The kernel selects one target per observer per step. It has no multi-target output, no
  perception model, no candidate gathering, no ID or tag mapping (the snippets above are patterns,
  not shipped API) and no animation.
- Time is integer milliseconds derived from a `double` of seconds, so very large times lose
  single-millisecond resolution.
- Version 0.x: the adapter API may change before 1.0. See [CHANGELOG.md](CHANGELOG.md).

## Kernel source and updates

The vendored code is the unchanged Interest Engine **v0.2.0** release at commit
`52e33c42be2445ef95a5024e47b570153279a7a6`; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)
for paths and blob hashes. `Private/InterestKernelSources.cpp` includes the two upstream `.cpp`
files exactly once, under the engine's macros and the module's own compiler settings
(`/fp:precise`, C++20, enforced by a private PCH and a compile-time check). Do not add them as
separate translation units or link another copy of the kernel.

To update the kernel: pick an explicit upstream release, review its public contracts, replace only
the code files and its license verbatim, update the provenance, verify the Git blob hashes against
that revision, and rerun the validation matrix.

## License and attribution

This plugin, including the adapter, tests and documentation, is licensed under the
[Apache License 2.0](LICENSE). Copyright 2026 PullsarDev. Source files carry SPDX identifiers.

`ThirdParty/InterestEngine/` is the unchanged Interest Engine kernel from
[PullsarDev/InterestEngine](https://github.com/PullsarDev/InterestEngine), also Apache-2.0
(copyright 2026 PullsarDev), with its original license and notices retained. Unreal Engine is a
trademark of Epic Games, Inc. This project is not affiliated with or endorsed by Epic Games; use
of the engine is governed by its own license.
