// CI verification of the parametric-control contract the NAMKnobs plugin depends on.
//
// The plugin (via ResamplingNAM) feeds a parametric NAM model a mono audio channel plus K constant control
// channels: process([audio ; c0 ; c1 ; ...]). This test loads a bundled parametric .nam with the SAME pinned
// NeuralAmpModelerCore the plugin builds against, then asserts:
//   1. it loads without throwing and reports NumInputChannels() == 1 + K with K >= 1 (a real parametric model);
//   2. output is mono and finite (never NaN/Inf -> never poisons a DAW chain);
//   3. if the model file declares metadata.controls, its count equals K (labels match the channel count);
//   4. EVERY control is individually wired and effective: sweeping control k alone (low vs high) while holding
//      the others at a fixed baseline measurably changes the output. Sweeping all controls together is not
//      enough -- a model could ignore one knob and still pass -- so each control is swept independently.
//
// This is the value-level assertion pluginval cannot make (pluginval never loads a .nam). Pure NAM core, no
// iPlug2. Exit 0 = pass; any non-zero = fail (fails the CI job). Deterministic (no RNG) for cross-runner parity.
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "NAM/dsp.h"
#include "NAM/get_dsp.h"
#include "nlohmann/json.hpp"

namespace {
constexpr double kPI = 3.14159265358979323846;
constexpr double kBaseline = 0.5; // hold non-swept controls here
constexpr double kMinRelDiff = 0.01; // each control must move the output at least this much (relative)

double Rms(const std::vector<NAM_SAMPLE>& v)
{
  double s = 0.0;
  for (NAM_SAMPLE x : v)
    s += (double)x * (double)x;
  return std::sqrt(s / (double)v.size());
}

bool AllFinite(const std::vector<NAM_SAMPLE>& v)
{
  for (NAM_SAMPLE x : v)
    if (!std::isfinite((double)x))
      return false;
  return true;
}

// Render the model over a fixed test signal with each control held at controlValues[k]. Returns the mono output.
std::vector<NAM_SAMPLE> Render(nam::DSP& model, const std::vector<NAM_SAMPLE>& sig, const std::vector<double>& controlValues,
                               int inCh, double sr, int block)
{
  const int N = (int)sig.size();
  model.Reset(sr, block);
  std::vector<std::vector<NAM_SAMPLE>> in((size_t)inCh, std::vector<NAM_SAMPLE>((size_t)block, (NAM_SAMPLE)0));
  std::vector<NAM_SAMPLE*> inPtr((size_t)inCh);
  for (int c = 0; c < inCh; ++c)
    inPtr[(size_t)c] = in[(size_t)c].data();
  std::vector<NAM_SAMPLE> out((size_t)N, (NAM_SAMPLE)0), obuf((size_t)block, (NAM_SAMPLE)0);
  NAM_SAMPLE* outPtr = obuf.data();
  int pos = 0;
  while (pos < N)
  {
    const int n = std::min(block, N - pos);
    for (int i = 0; i < n; ++i)
      in[0][(size_t)i] = sig[(size_t)(pos + i)];
    for (int i = n; i < block; ++i)
      in[0][(size_t)i] = (NAM_SAMPLE)0;
    for (int k = 0; k + 1 < inCh; ++k)
      for (int i = 0; i < block; ++i)
        in[(size_t)(1 + k)][(size_t)i] = (NAM_SAMPLE)controlValues[(size_t)k];
    model.process(inPtr.data(), &outPtr, n);
    for (int i = 0; i < n; ++i)
      out[(size_t)(pos + i)] = obuf[(size_t)i];
    pos += n;
  }
  return out;
}

double RelDiff(const std::vector<NAM_SAMPLE>& a, const std::vector<NAM_SAMPLE>& b)
{
  const int N = (int)std::min(a.size(), b.size());
  double sd = 0.0;
  for (int i = 0; i < N; ++i)
  {
    const double d = (double)a[(size_t)i] - (double)b[(size_t)i];
    sd += d * d;
  }
  sd = std::sqrt(sd / (double)N);
  return sd / (std::max(Rms(a), Rms(b)) + 1e-12);
}
} // namespace

int main(int argc, char** argv)
{
  if (argc < 2)
  {
    std::fprintf(stderr, "usage: verify_parametric <model.nam>\n");
    return 64;
  }
  const std::string path = argv[1];

  std::unique_ptr<nam::DSP> model;
  try
  {
    model = nam::get_dsp(std::filesystem::path(path));
  }
  catch (const std::exception& e)
  {
    std::fprintf(stderr, "FAIL[%s]: model load threw: %s\n", path.c_str(), e.what());
    return 2;
  }
  if (!model)
  {
    std::fprintf(stderr, "FAIL[%s]: model load returned null\n", path.c_str());
    return 2;
  }

  const int inCh = model->NumInputChannels();
  const int outCh = model->NumOutputChannels();
  const int K = inCh - 1;
  std::printf("model=%s  inCh=%d  outCh=%d  K(controls)=%d\n", path.c_str(), inCh, outCh, K);

  if (K < 1)
  {
    std::fprintf(stderr, "FAIL[%s]: not a parametric model (need >=1 control channel, got K=%d)\n", path.c_str(), K);
    return 3;
  }
  if (outCh != 1)
  {
    std::fprintf(stderr, "FAIL[%s]: expected mono output, got %d channels\n", path.c_str(), outCh);
    return 3;
  }

  // If the model file declares control names, their count must match K (labels match the channel count).
  try
  {
    std::ifstream f(path);
    if (f.good())
    {
      nlohmann::json j;
      f >> j;
      if (j.contains("metadata") && j["metadata"].contains("controls") && j["metadata"]["controls"].is_array())
      {
        const int named = (int)j["metadata"]["controls"].size();
        std::printf("  metadata.controls declares %d names\n", named);
        if (named != K)
        {
          std::fprintf(stderr, "FAIL[%s]: metadata.controls count %d != K %d\n", path.c_str(), named, K);
          return 8;
        }
      }
    }
  }
  catch (...)
  {
    // Best-effort: a model without parseable metadata.controls still gets the per-control sweep below.
  }

  const double sr = model->GetExpectedSampleRate() > 0.0 ? model->GetExpectedSampleRate() : 48000.0;
  const int block = 64;
  const int N = (int)sr; // ~1 second

  // Deterministic broadband test signal (three partials with a decay). No RNG -> identical on every runner.
  std::vector<NAM_SAMPLE> sig((size_t)N);
  for (int i = 0; i < N; ++i)
  {
    const double t = (double)i / sr;
    double x = 0.5 * std::sin(2.0 * kPI * 110.0 * t) + 0.3 * std::sin(2.0 * kPI * 440.0 * t)
               + 0.2 * std::sin(2.0 * kPI * 1500.0 * t);
    x *= std::exp(-2.0 * t);
    sig[(size_t)i] = (NAM_SAMPLE)(0.6 * x);
  }

  // Sweep EACH control independently: hold all at baseline, then move only control k low vs high.
  bool ok = true;
  for (int k = 0; k < K; ++k)
  {
    std::vector<double> lo((size_t)K, kBaseline), hi((size_t)K, kBaseline);
    lo[(size_t)k] = 0.0;
    hi[(size_t)k] = 1.0;
    std::vector<NAM_SAMPLE> outLo, outHi;
    try
    {
      outLo = Render(*model, sig, lo, inCh, sr, block);
      outHi = Render(*model, sig, hi, inCh, sr, block);
    }
    catch (const std::exception& e)
    {
      std::fprintf(stderr, "FAIL[%s]: process threw on control %d: %s\n", path.c_str(), k, e.what());
      return 4;
    }
    if (!AllFinite(outLo) || !AllFinite(outHi))
    {
      std::fprintf(stderr, "FAIL[%s]: non-finite output on control %d\n", path.c_str(), k);
      return 5;
    }
    const double rl = Rms(outLo), rh = Rms(outHi);
    const double rel = RelDiff(outLo, outHi);
    std::printf("  control %d: rms(lo)=%.6f rms(hi)=%.6f relDiff=%.4f\n", k, rl, rh, rel);
    if (rl < 1e-6 && rh < 1e-6)
    {
      std::fprintf(stderr, "FAIL[%s]: control %d silent at both extremes\n", path.c_str(), k);
      ok = false;
    }
    else if (rel < kMinRelDiff)
    {
      std::fprintf(stderr, "FAIL[%s]: control %d ineffective (relDiff=%.4f < %.4f)\n", path.c_str(), k, rel,
                   kMinRelDiff);
      ok = false;
    }
  }

  if (!ok)
    return 7;

  std::printf("PASS[%s]: all %d controls individually wired and effective\n", path.c_str(), K);
  return 0;
}
