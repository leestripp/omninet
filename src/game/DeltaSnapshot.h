// OmniNet - Snapshot delta compression helpers (Quake 3 / Source style)
//
// The server keeps a baseline snapshot per replicated object; each update is
// encoded as a compact XOR/RLE diff against the baseline the client is known
// to have. The client reconstructs the full snapshot and confirms receipt;
// the server then moves its baseline forward (setBaseline).
//
// Wire format per encoded snapshot (via BitWriter):
//   u8        type        0 = full, 1 = delta
//   varint    snapshotId
//   [varint   baselineId]        delta only
//   varint    fullSize           size of the reconstructed snapshot
//   full:     raw bytes
//   delta:    op stream of { varint zeroRun, varint literalLen, literal bytes }
//             where zeroRun skips bytes identical to the baseline (XOR = 0)
#ifndef OMNINET_GAME_DELTA_SNAPSHOT_H
#define OMNINET_GAME_DELTA_SNAPSHOT_H
#pragma once

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace OmniNet::Game {

// ============================================================================
// DeltaEncoder (sender side)
// ============================================================================

class DeltaEncoder {
public:
    // Encode `current` for objectId. Emits a full snapshot when no baseline
    // exists (or when the diff would be larger than the full snapshot),
    // otherwise a delta against the stored baseline.
    // Returns the encoded bytes. Does NOT move the baseline: call
    // setBaseline() once the peer has acknowledged this snapshotId.
    std::vector<uint8_t> encode(uint32_t objectId, uint32_t snapshotId,
                                std::span<const uint8_t> current) const;

    // Move the baseline for objectId to an acknowledged snapshot.
    void setBaseline(uint32_t objectId, uint32_t snapshotId,
                     std::span<const uint8_t> data);
    void removeBaseline(uint32_t objectId);
    bool hasBaseline(uint32_t objectId) const;

private:
    struct Baseline {
        uint32_t snapshotId = 0;
        std::vector<uint8_t> data;
    };
    std::unordered_map<uint32_t, Baseline> m_baselines;
};

// ============================================================================
// DeltaDecoder (receiver side)
// ============================================================================

class DeltaDecoder {
public:
    // Decode one encoded snapshot. On success returns true, fills out with
    // the reconstructed bytes and snapshotId with its id, and stores the
    // result as the new baseline for objectId.
    // Returns false on malformed input or an unknown/mismatched baseline.
    bool decode(uint32_t objectId, std::span<const uint8_t> encoded,
                std::vector<uint8_t>& out, uint32_t& snapshotId);

    const std::vector<uint8_t>* baseline(uint32_t objectId) const;
    void removeBaseline(uint32_t objectId);

private:
    std::unordered_map<uint32_t, std::vector<uint8_t>> m_baselines;
};

} // namespace OmniNet::Game

#endif // OMNINET_GAME_DELTA_SNAPSHOT_H
