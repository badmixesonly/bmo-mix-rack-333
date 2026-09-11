# BMO DEQ

A parametric EQ whose bands can move their own gain with the signal — a
dynamic EQ — at **zero samples of latency**, with the filter accuracy near
Nyquist that other plugins buy with oversampling (and pay for in latency).

**Status: DSP only.** There is nothing to load in a DAW yet. The filter core,
the dynamics and the M/S handling are written and tested, with bands in series.
The controls and the panel wait for the rename to BMO DEQ, and its
parameter-limit exemption, to land on `main`. Serial vs parallel can already be
compared by ear: see `testing-notes/deq-topology-listening.md`.

## What is here

```
dsp/          the audio path (JUCE-free)
  Prototype.h   the analogue filters every band is measured against
  Design.*      matched-Z coefficient design
  Svf.h         the filter structure that runs them
  Dynamics.h    detector and gain computer
  DspCore.*     bands, M/S, topology, smoothing
reference/    test-only: the cookbook designs and measurement helpers
spec/         the spec as received, and its review
```

## Trying it

```
cmake -B build-dsp -DBMO_DSP_ONLY=ON
cmake --build build-dsp --config Release
ctest --test-dir build-dsp -C Release -R deq
build-dsp/tools/Release/measure_deq cramp        # accuracy against the analogue ideal
build-dsp/tools/Release/measure_deq topology     # how bands combine
build-dsp/tools/Release/measure_deq curve bell 16000 4 12
build-dsp/tools/Release/measure_deq render source.wav out --blind 7   # serial vs parallel, by ear
```
