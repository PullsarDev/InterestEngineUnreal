# Changelog

The plugin version (`VersionName` in `InterestEngineUnreal.uplugin`) and the vendored kernel
version are separate. Entries below name both.

## 0.2.1 - first public release (kernel v0.2.0)

### Added

- Exported `InterestEngine::ToString` overloads for `interest::DecisionKind`,
  `interest::DecisionReason` and `interest::Eligibility`, and `InterestEngine::ResetMemory`. They
  forward to the kernel. Before this, an external module in an Editor build, where modules are
  separate DLLs, failed to link when it called those kernel functions directly, while a monolithic
  Game build linked. Every kernel free function now has an adapter entry point.
- Consumer tests for the new entry points and for the first README example, and a startup check
  that calls them in every configuration, including Shipping.
- `Tests/Validate.ps1`: `-EngineRoot`, `-WorkRoot`, `-Matrix` (all six configurations), `-Clean`
  and `-AdditionalBuildArguments`.

### Changed

- Licensed under the Apache License 2.0 (adapter, tests and documentation). Source files carry
  SPDX identifiers.
- `Tests/Validate.ps1` builds a copy of the plugin inside a generated host project and no longer
  builds in the plugin directory. It compiles every C++ block of the README and checks that the
  runtime module is compiled with `/fp:precise` and C++20.
- Plugin descriptor: `EngineVersion` 5.8.0, `SupportedTargetPlatforms` Win64, beta flag, a fuller
  description. Add `"SupportedTargetPlatforms": ["Win64"]` to the plugin entry in your `.uproject`
  to avoid the editor's "project requires update" notice.
- README rewritten for first-time users.

## 0.2.0 - kernel v0.2.0

- Vendored Interest Engine v0.2.0 unchanged (commit `52e33c42be2445ef95a5024e47b570153279a7a6`).
- The kernel's public structs changed, which the adapter exposes directly:
  `CandidateSnapshot::eventDirectionality` is a new trailing field defaulting to `1.f`, and
  `CandidateTrace::noveltyGate` is inserted before `eventNovelty`. Aggregate initializers of
  `CandidateSnapshot` may omit the new field, but positional initializers of `CandidateTrace`
  must be updated; use named member access. Upstream verified that the new trace field occupies
  existing x64 padding, so the trace's size and earlier offsets are unchanged.
- The default directionality reuses the legacy rounded gate exactly, so scoring and temporal policy
  are unchanged unless a host sets a lower value. Directionality 0 removes only novelty's angular
  restriction; distance falloff, range cutoffs and perception strength still apply. An invalid
  directionality rejects the candidate as `InvalidEvent`, even for absent events.
- Rebuild the plugin and every consumer together; do not mix binaries from different kernel
  versions.

## 0.1.0 - kernel v0.1.0

- Initial exported adapter: conversions, `FProfileOwner`, `FStepWorkspace`, and an external consumer
  test module. Not published.
