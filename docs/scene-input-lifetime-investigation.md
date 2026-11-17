# Warframe scene-input lifetime investigation

## Decision

Do not publish another pool-size candidate. Keep the Warframe compositor split opt-in and experimental. The local investigation reproduces the reported allocation failure without GPU backlog. The current independent-image design ties storage to the lifetime of closed command recordings, not just to their GPU execution time. A finite image pool therefore cannot guarantee coverage for an engine that keeps more recordings valid than that pool can hold.

This is a confirmed limitation of the current design and its use of the supported submission callbacks. It is not proof that a different implementation is impossible. Resolving it needs a separate submission/storage redesign and local evidence before another reporter build.

## Evidence

The [Preview 6 report](https://github.com/Pav-Osmolski/ReshadeEffectShaderToggler/issues/22#issuecomment-6897920548) shows all eight pairs compatible and active, zero available/incompatible/unsafe pairs, 1275 MiB private storage and 15339/5054 MiB GPU budget/usage. Its failure is the eight-image count bound, not a failed format match or insufficient GPU headroom. The reporter also confirms that flicker is much less frequent.

`tests/SceneInputLifecycleTests.cpp` uses the production ownership and admission helpers with native D3D12 WARP resources. The GPU buffers are deliberately small; the admission check separately uses the reported memory counters. This is a controlled reproduction of the ownership failure, not a replay of Warframe or a validation of the complete compositor renderer.

| Local case | Observed result |
| --- | --- |
| Eight closed recordings submitted as one batch; wait until GPU idle | All eight retain ownership; ninth request reports active image capacity despite memory headroom. |
| Submit the same closed recordings again after completion, without Reset | Native GPU output remains exact. Completion did not invalidate those recordings. |
| Retire one old owner and reset its actual native command list | Exactly one image becomes available; a replacement recording produces the expected output. |
| Repeat retirement using an old owner | Replacement ownership is preserved. |
| Signal per-list markers before native batch submission; hold the batch behind a GPU gate | Both markers complete before the batch's completion fence. They cannot prove completion of that batch. |
| Reset a command-list object while old execution is pending, using a fresh allocator | Image ownership can pass on, but its prior GPU completion dependency remains pending until the gate is released. |
| Acquire and retire without submission; repeat retirement | Inherited dependencies survive; ownership is not leaked. |

The existing 96-submission/two-queue test continues to cover delayed GPU handoffs. The new test additionally checks D3D12 debug messages and rejects errors/corruption. It does not execute REST's full descriptor cloning, rendering or callback registration code; those are covered separately or remain source-review findings.

## Supported event ordering

The pinned ReShade dependency is `18deaa52de0c425a78b329e9cb3c497281cd00ec` (6.8.0).

- [`D3D12CommandQueue::ExecuteCommandLists`](https://github.com/crosire/reshade/blob/18deaa52de0c425a78b329e9cb3c497281cd00ec/source/d3d12/d3d12_command_queue.cpp) invokes `execute_command_list` once for each list, then calls native `ExecuteCommandLists` with the batch. The callback returns `void`; it neither supplies the whole batch nor permits replacement/suppression of submission.
- [`reshade_events.hpp`](https://github.com/crosire/reshade/blob/18deaa52de0c425a78b329e9cb3c497281cd00ec/include/reshade_events.hpp) documents that event as pre-submission. It does not expose a matching whole-batch post-submission callback. Present/effect-completion callbacks are different boundaries and cannot substitute for arbitrary queue submission ordering.
- [`D3D12GraphicsCommandList::Reset`](https://github.com/crosire/reshade/blob/18deaa52de0c425a78b329e9cb3c497281cd00ec/source/d3d12/d3d12_command_list.cpp) invokes the reset callback before the native Reset call. REST currently retires its leases there. The native test models successful, valid resets, not native Reset failures.
- [Microsoft's execution documentation](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-id3d12commandqueue-executecommandlists) distinguishes separate submission calls from lists passed in one batch. Same-queue batching is not a substitute for explicit resource dependencies. Cross-queue sharing additionally needs correct synchronization.

REST's current retirement signals are issued at reset/destruction, after prior submissions, rather than at the pre-submit event. The local tests found no ownership leak in the exercised normal reset, inherited-dependency or duplicate-retirement cases. They do not establish that every lifecycle path in the game has been observed.

## Why the simple shortcuts do not establish a fix

- **More images:** moves the failure to a larger number of valid recordings and increases memory use. Preview 6 already reaches about 1.25 GiB at 4K.
- **Free an image on GPU completion:** a still-valid recording can be submitted again and still contains native resource/render-target references to that image. Dropping references or reassigning ownership alone does not rewrite those commands.
- **Signal from `execute_command_list`:** signals before the work. The gated native test proves the ordering problem.
- **One shared image because only one graphics queue was observed:** does not establish safety for batching, changed recording order, future queue creation or cross-queue submission. Recorded barriers also need compatible boundaries.
- **Change descriptor tables when resubmitted:** cannot redirect every resource reference already baked into a native command list, including its render-target bindings and copy operations.

## Additional release blockers from source review

The native ownership tests intentionally return their scratch buffers to a canonical state. They must not be presented as proof that REST's actual compositor command sequence is replay-safe. Source review found two further state assumptions to address in a redesign:

1. Fresh compositor images begin as render targets and successful processing leaves them shader-readable. The first recording may omit a shader-read-to-render-target barrier because the CPU-side `shaderReadable`/`alphaReadable` flags were initially false. Replaying that exact recording can therefore start from a different state. Native compositor replay validation is required.
2. Retirement caches the final recorded state even when a recording was never submitted. Ownership/dependency tests do not prove that this cached state matches the GPU's actual state. Abandoned-recording state handling needs its own renderer-level regression.

These are source-review findings, not an explanation established for the reporter's rare flicker: their captured failure explicitly reports image capacity. They strengthen the decision against an early-release shortcut or stable promotion.

## Next implementation boundary

A candidate architecture must keep descriptors/resources alive for valid recordings while allowing bounded physical scratch reuse through explicit ordering and canonical resource states. The missing proof is safe ordering of whole submissions, including batched, concurrent and cross-queue calls and later resubmission. A batch-aware submission bridge (upstream ReShade support or a separately assessed native interception layer) is one route to investigate; a post-submit notification alone is not the complete fix.

Before any further public candidate, require a local prototype to pass:

1. More valid recordings than physical scratch slots, with fixed memory use and no skipped eligible draws.
2. Arbitrary legal submission order, same-call batches and resubmission after GPU completion.
3. Simultaneous CPU submission to distinct queues, with no early completion tickets, races or deadlocks.
4. Reset during pending execution, destruction, abandoned recordings and failure paths with correct resource states and lifetime.
5. The actual compositor renderer and saved-frame/HDR checks, not only synthetic ownership helpers.

No new reporter capture is needed to establish the present capacity problem. No further pool expansion or stable promotion follows from this investigation.

## Restricted submission proof — 17 November 2026

The [standalone bounded-scratch prototype](scene-submission-prototype.md) now passes 32 live recordings with one FP16 scratch image on one known queue. Whole-batch ordering, canonical states, concurrent CPU submissions on that queue and retained immutable versions are exercised. This does not pass the cross-queue or actual compositor acceptance criteria above and is not integrated into REST.
