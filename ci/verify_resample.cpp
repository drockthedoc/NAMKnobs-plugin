// CI verification that knob controls survive host-rate resampling — the concern that the mono
// ResamplingContainer<NAM_SAMPLE,1,12> would drop the extra control channels at non-model sample rates.
//
// This mirrors ResamplingNAM's EXACT runtime path: a mono dsp::ResamplingContainer configured for the model's
// internal rate, with the K constant control channels injected INSIDE the block callback at model rate (the
// resampler only ever sees mono audio). It runs the model at several host sample rates (44.1/48/88.2/96 kHz) and,
// at each rate, sweeps control 0 low vs high (others at 0.5) and asserts the output is finite and MEASURABLY
// different. If resampling dropped the control channels, low and high would be identical -> relDiff ~ 0 -> fail.
//
// Pure NAM core + AudioDSPTools ResamplingContainer; no iPlug2. Exit 0 = pass; non-zero = fail. Deterministic.
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "NAM/dsp.h"
#include "NAM/get_dsp.h"

// AudioDSPTools' ResamplingContainer/LanczosResampler have two soft iPlug2 dependencies (iplug::PI and
// DEFAULT_BLOCK_SIZE). This is a standalone test binary (no iPlug2 linked), so provide them minimally. In the
// real plugin these come from iPlug2; there is no conflict because this file is compiled into its own executable.
#ifndef DEFAULT_BLOCK_SIZE
#define DEFAULT_BLOCK_SIZE 1024
#endif
namespace iplug {
constexpr double PI = 3.14159265358979323846;
}
#include "ResamplingContainer/ResamplingContainer.h"

namespace {
constexpr double kPI = 3.14159265358979323846;
constexpr int kMaxEncapBlock = 8192; // control scratch capacity (model-rate frames per callback)

double Rms(const std::vector<NAM_SAMPLE>& v)
{
  double s = 0.0;
  for (NAM_SAMPLE x : v)
    s += (double)x * (double)x;
  return v.empty() ? 0.0 : std::sqrt(s / (double)v.size());
}

bool AllFinite(const std::vector<NAM_SAMPLE>& v)
{
  for (NAM_SAMPLE x : v)
    if (!std::isfinite((double)x))
      return false;
  return true;
}

// Render `sig` (host-rate mono) through the model at hostRate, holding controls at controlValues, exactly as
// ResamplingNAM does: mono resampler to model rate, controls injected in the callback, back to host rate.
std::vector<NAM_SAMPLE> RenderAtHostRate(nam::DSP& model, double modelRate, double hostRate,
                                         const std::vector<NAM_SAMPLE>& sig, const std::vector<double>& controlValues)
{
  const int inCh = model.NumInputChannels();
  const int K = inCh - 1;
  const int hostBlock = 512;

  model.Reset(modelRate, kMaxEncapBlock);

  dsp::ResamplingContainer<NAM_SAMPLE, 1, 12> resampler(modelRate);
  resampler.Reset(hostRate, hostBlock);

  // Control scratch (model rate). Preallocated once, filled per callback.
  std::vector<std::vector<NAM_SAMPLE>> ctrl((size_t)K, std::vector<NAM_SAMPLE>((size_t)kMaxEncapBlock, (NAM_SAMPLE)0));
  std::vector<NAM_SAMPLE*> multiIn((size_t)inCh, nullptr);

  auto func = [&](NAM_SAMPLE** input, NAM_SAMPLE** output, int nFrames) {
    multiIn[0] = input[0];
    for (int k = 0; k < K; ++k)
    {
      NAM_SAMPLE* c = ctrl[(size_t)k].data();
      const NAM_SAMPLE v = (NAM_SAMPLE)controlValues[(size_t)k];
      for (int i = 0; i < nFrames; ++i)
        c[i] = v;
      multiIn[(size_t)(1 + k)] = c;
    }
    model.process(multiIn.data(), output, nFrames);
  };

  const int N = (int)sig.size();
  std::vector<NAM_SAMPLE> out((size_t)N, (NAM_SAMPLE)0);
  std::vector<NAM_SAMPLE> inBuf((size_t)hostBlock, (NAM_SAMPLE)0), outBuf((size_t)hostBlock, (NAM_SAMPLE)0);
  NAM_SAMPLE* inPtr = inBuf.data();
  NAM_SAMPLE* outPtr = outBuf.data();
  int pos = 0;
  while (pos < N)
  {
    const int n = std::min(hostBlock, N - pos);
    for (int i = 0; i < n; ++i)
      inBuf[(size_t)i] = sig[(size_t)(pos + i)];
    for (int i = n; i < hostBlock; ++i)
      inBuf[(size_t)i] = (NAM_SAMPLE)0;
    resampler.ProcessBlock(&inPtr, &outPtr, n, func);
    for (int i = 0; i < n; ++i)
      out[(size_t)(pos + i)] = outBuf[(size_t)i];
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
  sd = std::sqrt(sd / (double)std::max(N, 1));
  return sd / (std::max(Rms(a), Rms(b)) + 1e-12);
}
} // namespace

int main(int argc, char** argv)
{
  if (argc < 2)
  {
    std::fprintf(stderr, "usage: verify_resample <model.nam>\n");
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
    std::fprintf(stderr, "FAIL[%s]: load threw: %s\n", path.c_str(), e.what());
    return 2;
  }
  if (!model || model->NumInputChannels() < 2)
  {
    std::fprintf(stderr, "FAIL[%s]: not a parametric model\n", path.c_str());
    return 3;
  }
  const int K = model->NumInputChannels() - 1;
  const double modelRate = model->GetExpectedSampleRate() > 0.0 ? model->GetExpectedSampleRate() : 48000.0;
  std::printf("model=%s  K=%d  modelRate=%.0f\n", path.c_str(), K, modelRate);

  const double hostRates[] = {44100.0, 48000.0, 88200.0, 96000.0};
  bool ok = true;
  for (double hostRate : hostRates)
  {
    const int N = (int)hostRate; // ~1s at host rate
    std::vector<NAM_SAMPLE> sig((size_t)N);
    for (int i = 0; i < N; ++i)
    {
      const double t = (double)i / hostRate;
      double x = 0.5 * std::sin(2.0 * kPI * 110.0 * t) + 0.3 * std::sin(2.0 * kPI * 440.0 * t)
                 + 0.2 * std::sin(2.0 * kPI * 1500.0 * t);
      x *= std::exp(-2.0 * t);
      sig[(size_t)i] = (NAM_SAMPLE)(0.6 * x);
    }

    std::vector<double> lo((size_t)K, 0.5), hi((size_t)K, 0.5);
    lo[0] = 0.0;
    hi[0] = 1.0;
    std::vector<NAM_SAMPLE> outLo, outHi;
    try
    {
      outLo = RenderAtHostRate(*model, modelRate, hostRate, sig, lo);
      outHi = RenderAtHostRate(*model, modelRate, hostRate, sig, hi);
    }
    catch (const std::exception& e)
    {
      std::fprintf(stderr, "FAIL[%s]: process threw at %.0f Hz: %s\n", path.c_str(), hostRate, e.what());
      return 4;
    }
    if (!AllFinite(outLo) || !AllFinite(outHi))
    {
      std::fprintf(stderr, "FAIL[%s]: non-finite output at %.0f Hz\n", path.c_str(), hostRate);
      return 5;
    }
    const double rel = RelDiff(outLo, outHi);
    std::printf("  %.0f Hz: rms(lo)=%.5f rms(hi)=%.5f relDiff=%.4f\n", hostRate, Rms(outLo), Rms(outHi), rel);
    if (rel < 0.01)
    {
      std::fprintf(stderr, "FAIL[%s]: control 0 ineffective at %.0f Hz (relDiff=%.4f) -> resampling dropped it\n",
                   path.c_str(), hostRate, rel);
      ok = false;
    }
  }

  if (!ok)
    return 7;
  std::printf("PASS[%s]: controls survive resampling at 44.1/48/88.2/96 kHz\n", path.c_str());
  return 0;
}
