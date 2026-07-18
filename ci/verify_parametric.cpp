// CI verification of the parametric-control contract the NAMKnobs plugin depends on.
//
// The plugin (via ResamplingNAM) feeds a parametric NAM model a mono audio channel plus K constant control
// channels: process([audio ; c0 ; c1 ; ...]). This test loads a bundled parametric .nam with the SAME pinned
// NeuralAmpModelerCore the plugin builds against, then asserts:
//   1. it loads without throwing and reports NumInputChannels() == 1 + K with K >= 1 (a real parametric model);
//   2. output is mono and finite (never NaN/Inf -> never poisons a DAW chain);
//   3. holding every control low vs. high yields a MEASURABLY different output -> the control channels actually
//      reach and steer the model (not silently dropped, not ignored).
//
// This is the value-level assertion pluginval cannot make (pluginval never loads a .nam). Pure NAM core, no
// iPlug2. Exit 0 = pass; any non-zero = fail (fails the CI job). Deterministic (no RNG) for cross-runner parity.
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "NAM/dsp.h"
#include "NAM/get_dsp.h"

namespace {
constexpr double kPI = 3.14159265358979323846;

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
} // namespace

int main(int argc, char** argv)
{
  if (argc < 2)
  {
    std::fprintf(stderr, "usage: verify_parametric <model.nam>\n");
    return 64;
  }

  std::unique_ptr<nam::DSP> model;
  try
  {
    model = nam::get_dsp(std::filesystem::path(argv[1]));
  }
  catch (const std::exception& e)
  {
    std::fprintf(stderr, "FAIL: model load threw: %s\n", e.what());
    return 2;
  }
  if (!model)
  {
    std::fprintf(stderr, "FAIL: model load returned null\n");
    return 2;
  }

  const int inCh = model->NumInputChannels();
  const int outCh = model->NumOutputChannels();
  const int K = inCh - 1;
  std::printf("model=%s  inCh=%d  outCh=%d  K(controls)=%d\n", argv[1], inCh, outCh, K);

  if (K < 1)
  {
    std::fprintf(stderr, "FAIL: not a parametric model (need >=1 control channel, got K=%d)\n", K);
    return 3;
  }
  if (outCh != 1)
  {
    std::fprintf(stderr, "FAIL: expected mono output, got %d channels\n", outCh);
    return 3;
  }

  const double sr = model->GetExpectedSampleRate() > 0.0 ? model->GetExpectedSampleRate() : 48000.0;
  const int block = 64;
  const int N = (int)sr; // ~1 second

  // Deterministic broadband test signal (three partials with a decay), so a distortion/filter/level control has
  // something to change. No RNG -> identical on every runner.
  std::vector<NAM_SAMPLE> sig((size_t)N);
  for (int i = 0; i < N; ++i)
  {
    const double t = (double)i / sr;
    double x = 0.5 * std::sin(2.0 * kPI * 110.0 * t) + 0.3 * std::sin(2.0 * kPI * 440.0 * t)
               + 0.2 * std::sin(2.0 * kPI * 1500.0 * t);
    x *= std::exp(-2.0 * t);
    sig[(size_t)i] = (NAM_SAMPLE)(0.6 * x);
  }

  auto renderAtControl = [&](double ctl) -> std::vector<NAM_SAMPLE> {
    model->Reset(sr, block);
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
      for (int k = 0; k < K; ++k)
        for (int i = 0; i < block; ++i)
          in[(size_t)(1 + k)][(size_t)i] = (NAM_SAMPLE)ctl;
      model->process(inPtr.data(), &outPtr, n);
      for (int i = 0; i < n; ++i)
        out[(size_t)(pos + i)] = obuf[(size_t)i];
      pos += n;
    }
    return out;
  };

  std::vector<NAM_SAMPLE> lo, hi;
  try
  {
    lo = renderAtControl(0.0);
    hi = renderAtControl(1.0);
  }
  catch (const std::exception& e)
  {
    std::fprintf(stderr, "FAIL: process threw: %s\n", e.what());
    return 4;
  }

  if (!AllFinite(lo) || !AllFinite(hi))
  {
    std::fprintf(stderr, "FAIL: non-finite (NaN/Inf) output sample produced\n");
    return 5;
  }

  const double rl = Rms(lo), rh = Rms(hi);
  double sd = 0.0;
  for (int i = 0; i < N; ++i)
  {
    const double d = (double)lo[(size_t)i] - (double)hi[(size_t)i];
    sd += d * d;
  }
  sd = std::sqrt(sd / (double)N);
  const double relDiff = sd / (std::max(rl, rh) + 1e-12);
  std::printf("rms(ctl=0)=%.6f  rms(ctl=1)=%.6f  diffRMS=%.6f  relDiff=%.4f\n", rl, rh, sd, relDiff);

  if (rl < 1e-6 && rh < 1e-6)
  {
    std::fprintf(stderr, "FAIL: model is silent at both control extremes\n");
    return 6;
  }
  if (relDiff < 0.01)
  {
    std::fprintf(stderr, "FAIL: controls barely change output (relDiff=%.4f < 0.01)\n", relDiff);
    return 7;
  }

  std::printf("PASS: parametric control contract holds (K=%d, relDiff=%.4f)\n", K, relDiff);
  return 0;
}
