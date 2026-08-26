#pragma once

// Opaque handle to a backend-owned effect chain spliced between a voice and its
// bus (one per CAudioSource voice that has an effect graph assigned). Callers treat
// it as a token — only the concrete backend knows it is a MiniaudioEffectChain*.
//
// Dependency-free (like SoundHandle.h) so components/modules name it without the
// backend abstraction. A null handle means "no chain".
using EffectChainHandle = void*;
