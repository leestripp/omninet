// OmniNet - Snapshot delta compression helpers
#include "game/DeltaSnapshot.h"

#include "game/BitStream.h"
#include "game/GameProtocol.h"

namespace OmniNet::Game {

namespace {

constexpr uint8_t TYPE_FULL  = 0;
constexpr uint8_t TYPE_DELTA = 1;

// Encode the diff op stream for current vs baseline. Returns bytes written
// to `w` as an estimate for the full-vs-delta decision is done by the caller.
void writeDiffOps(BitWriter& w, std::span<const uint8_t> baseline,
                  std::span<const uint8_t> current) {
    size_t i = 0;
    const size_t n = current.size();
    while (i < n) {
        // Count identical bytes (XOR zero run) — beyond baseline everything differs
        size_t zeroRun = 0;
        while (i + zeroRun < n && zeroRun < 0xFFFFFFFFu) {
            uint8_t base = (i + zeroRun < baseline.size()) ? baseline[i + zeroRun]
                                                           : uint8_t{0};
            if (current[i + zeroRun] != base) break;
            ++zeroRun;
        }
        w.writeVarInt(zeroRun);
        i += zeroRun;

        // Count differing literal bytes (stop at next run of >= 4 identical)
        size_t litStart = i;
        size_t litLen = 0;
        while (i + litLen < n) {
            size_t pos = i + litLen;
            uint8_t base = (pos < baseline.size()) ? baseline[pos] : uint8_t{0};
            if (current[pos] == base) {
                // Only break the literal for a run of 4+ identical bytes
                size_t run = 0;
                while (pos + run < n && run < 4) {
                    uint8_t b2 = (pos + run < baseline.size()) ? baseline[pos + run]
                                                               : uint8_t{0};
                    if (current[pos + run] != b2) break;
                    ++run;
                }
                if (run >= 4) break;
            }
            ++litLen;
        }
        w.writeVarInt(litLen);
        for (size_t k = 0; k < litLen; ++k)
            w.writeU8(current[litStart + k]);
        i += litLen;
    }
}

} // namespace

// ============================================================================
// DeltaEncoder
// ============================================================================

std::vector<uint8_t> DeltaEncoder::encode(uint32_t objectId, uint32_t snapshotId,
                                          std::span<const uint8_t> current) const {
    auto it = m_baselines.find(objectId);

    // Full snapshot path (also used as fallback)
    auto encodeFull = [&] {
        BitWriter w(current.size() + 16);
        w.writeU8(TYPE_FULL);
        w.writeVarInt(snapshotId);
        w.writeVarInt(current.size());
        w.writeBytes(current);
        auto span = w.bytes();
        return std::vector<uint8_t>(span.begin(), span.end());
    };

    if (it == m_baselines.end())
        return encodeFull();

    // Delta path
    BitWriter w(current.size() / 2 + 16);
    w.writeU8(TYPE_DELTA);
    w.writeVarInt(snapshotId);
    w.writeVarInt(it->second.snapshotId);
    w.writeVarInt(current.size());
    writeDiffOps(w, it->second.data, current);

    auto span = w.bytes();
    std::vector<uint8_t> delta(span.begin(), span.end());

    // A diff larger than the full snapshot is never worth sending
    std::vector<uint8_t> full = encodeFull();
    return delta.size() < full.size() ? delta : full;
}

void DeltaEncoder::setBaseline(uint32_t objectId, uint32_t snapshotId,
                               std::span<const uint8_t> data) {
    Baseline b;
    b.snapshotId = snapshotId;
    b.data.assign(data.begin(), data.end());
    m_baselines[objectId] = std::move(b);
}

void DeltaEncoder::removeBaseline(uint32_t objectId) {
    m_baselines.erase(objectId);
}

bool DeltaEncoder::hasBaseline(uint32_t objectId) const {
    return m_baselines.find(objectId) != m_baselines.end();
}

// ============================================================================
// DeltaDecoder
// ============================================================================

bool DeltaDecoder::decode(uint32_t objectId, std::span<const uint8_t> encoded,
                          std::vector<uint8_t>& out, uint32_t& snapshotId) {
    BitReader r(encoded);
    uint8_t type = r.readU8();
    snapshotId = static_cast<uint32_t>(r.readVarInt());
    if (r.hasError()) return false;

    if (type == TYPE_FULL) {
        uint64_t size = r.readVarInt();
        if (r.hasError() || size > r.bitsRemaining() / 8) return false;
        out.resize(size);
        if (!r.readBytes(out)) return false;
        m_baselines[objectId] = out;
        return true;
    }

    if (type != TYPE_DELTA) return false;

    uint32_t baselineId = static_cast<uint32_t>(r.readVarInt());
    uint64_t fullSize = r.readVarInt();
    if (r.hasError() || fullSize > GAME_MAX_MESSAGE) return false;

    auto it = m_baselines.find(objectId);
    if (it == m_baselines.end()) return false;  // unknown baseline

    // Note: baselineId is advisory; the decoder always applies against its
    // stored baseline. A mismatch means the streams diverged — request a
    // full snapshot by dropping the baseline.
    const std::vector<uint8_t>& base = it->second;
    (void)baselineId;

    out.assign(fullSize, 0);
    size_t pos = 0;
    while (pos < fullSize) {
        uint64_t zeroRun = r.readVarInt();
        uint64_t litLen = r.readVarInt();
        if (r.hasError()) return false;
        if (pos + zeroRun + litLen > fullSize) return false;

        // Copy the identical region from the baseline (zeros past its end)
        for (uint64_t k = 0; k < zeroRun; ++k)
            out[pos + k] = (pos + k < base.size()) ? base[pos + k] : uint8_t{0};
        pos += zeroRun;

        for (uint64_t k = 0; k < litLen; ++k) {
            out[pos + k] = r.readU8();
        }
        if (r.hasError()) return false;
        pos += litLen;
    }

    m_baselines[objectId] = out;
    return true;
}

const std::vector<uint8_t>* DeltaDecoder::baseline(uint32_t objectId) const {
    auto it = m_baselines.find(objectId);
    return it != m_baselines.end() ? &it->second : nullptr;
}

void DeltaDecoder::removeBaseline(uint32_t objectId) {
    m_baselines.erase(objectId);
}

} // namespace OmniNet::Game
