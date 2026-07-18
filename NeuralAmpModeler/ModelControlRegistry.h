// ModelControlRegistry — maps a loaded neural model's declared controls onto a FIXED pool of generic,
// compile-time plugin parameter slots. iPlug2 (like VST3/AU) fixes the parameter count at construction
// (MakeConfig(kNumParams,...) in NeuralAmpModeler.cpp:79), so we do NOT fake dynamic parameters; we
// declare N generic "Model Control" slots up front and remap metadata (name/range/default/visibility)
// onto them when a model loads. See docs/CONDITIONED_NAM_IMPLEMENTATION.md for the wiring.
//
// Thread model: the UI thread writes normalized targets (atomic, lock-free). The audio thread reads the
// target and advances a one-pole smoother once per block, then hands the smoothed values to the NAM
// core via SetControls() BEFORE DSP::process(). No allocation/lock on the audio thread.
#pragma once
#include <atomic>
#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace nam_ctrl {

static constexpr int kMaxModelControls = 8;   // fixed generic slots (first version)

struct SlotMeta {   // immutable-per-load description mirrored from the model file
  bool active = false;
  char id[32] = {0};
  char name[48] = {0};
  char unit[8] = {0};
  double normMin = 0.0, normMax = 1.0;
  double dispMin = 0.0, dispMax = 1.0;
  double defaultNorm = 0.0;
  double smoothingMs = 10.0;
  bool automatable = true;
};

// One control declared by the model (as read from the .nam metadata by the core's read-only API).
struct ModelControlDesc {
  std::string id, name, unit;
  double normMin, normMax, dispMin, dispMax, defaultNorm, smoothingMs;
  bool automatable;
};

class ModelControlRegistry {
public:
  ModelControlRegistry() { for (auto& t : mTarget) t.store(0.0); }

  // ---- load-time (message/UI thread) : map model metadata onto the fixed slots ----
  void MapFromModel(const std::vector<ModelControlDesc>& ctrls) {
    mActiveCount = 0;
    for (int i = 0; i < kMaxModelControls; ++i) mMeta[i] = SlotMeta{};   // clear
    const int n = (int)std::min<size_t>(ctrls.size(), kMaxModelControls);
    for (int i = 0; i < n; ++i) {
      const auto& c = ctrls[i];
      SlotMeta& s = mMeta[i];
      s.active = true;
      std::strncpy(s.id, c.id.c_str(), sizeof(s.id) - 1);
      std::strncpy(s.name, c.name.c_str(), sizeof(s.name) - 1);
      std::strncpy(s.unit, c.unit.c_str(), sizeof(s.unit) - 1);
      s.normMin = c.normMin; s.normMax = c.normMax;
      s.dispMin = c.dispMin; s.dispMax = c.dispMax;
      s.defaultNorm = c.defaultNorm; s.smoothingMs = c.smoothingMs; s.automatable = c.automatable;
      mTarget[i].store(c.defaultNorm);
      mSmoothed[i] = c.defaultNorm;
    }
    mActiveCount = n;
  }

  void Clear() { for (int i = 0; i < kMaxModelControls; ++i) { mMeta[i] = SlotMeta{}; } mActiveCount = 0; }

  int activeCount() const { return mActiveCount; }
  const SlotMeta& meta(int slot) const { return mMeta[slot]; }
  bool slotActive(int slot) const { return slot >= 0 && slot < kMaxModelControls && mMeta[slot].active; }

  // ---- UI/automation thread : set a slot's normalized value (lock-free) ----
  void SetNormalized(int slot, double v) {
    if (slot < 0 || slot >= kMaxModelControls || !mMeta[slot].active) return;
    const SlotMeta& s = mMeta[slot];
    if (!std::isfinite(v)) return;
    v = std::min(s.normMax, std::max(s.normMin, v));
    mTarget[slot].store(v, std::memory_order_relaxed);
  }
  // convert a host [0,1] param position or a display value to normalized
  double DisplayToNormalized(int slot, double disp) const {
    const SlotMeta& s = mMeta[slot];
    double f = (disp - s.dispMin) / (s.dispMax - s.dispMin + 1e-12);
    return s.normMin + f * (s.normMax - s.normMin);
  }

  // ---- audio thread : advance smoothers once per block, fill an ordered control vector ----
  // Returns the number of active controls written to out[] (out is normalized values, index = slot).
  int AdvanceAndFill(double sampleRate, int blockSize, double* out, bool smoothingEnabled) {
    for (int i = 0; i < mActiveCount; ++i) {
      const double tgt = mTarget[i].load(std::memory_order_relaxed);
      if (!smoothingEnabled) { mSmoothed[i] = tgt; }
      else {
        // one-pole toward target over the block (per-slot time constant)
        const double tau = std::max(0.1, mMeta[i].smoothingMs) * 1e-3 * sampleRate;
        const double a = 1.0 - std::exp(-(double)blockSize / tau);   // per-block coefficient
        mSmoothed[i] += a * (tgt - mSmoothed[i]);
      }
      out[i] = mSmoothed[i];
    }
    return mActiveCount;
  }

  // ---- persistence : slots are serialized by stable id, not by index ----
  // (host session / preset). The plugin writes {id: value} pairs; on restore, match by id.
  double smoothedValue(int slot) const { return mSmoothed[slot]; }
  double targetValue(int slot) const { return mTarget[slot].load(std::memory_order_relaxed); }

private:
  std::array<SlotMeta, kMaxModelControls> mMeta{};
  std::array<std::atomic<double>, kMaxModelControls> mTarget{};
  std::array<double, kMaxModelControls> mSmoothed{};
  int mActiveCount = 0;
};

} // namespace nam_ctrl
