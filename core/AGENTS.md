# core/

Shared code. Four layers, each depending only on the ones above it:

```
dsp/      JUCE-free. ModuleDsp (the interface every module's DSP implements),
          Oversampler, Meter. Nothing here includes <juce_*>.
state/    ParamSpec (a parameter described without JUCE), ParamSet (a spec
          list bound to live juce parameters), Parameters.h (APVTS layout
          from specs), PresetManager (files, factory lists, migration).
ui/       Tokens (colours; theme JSON hot-reload), Fonts, LookAndFeel,
          Controls (PlainKnob, ConcentricBand, SwitchButton, OutputMeter),
          PresetBar, ProductHeader, ModulePanel (the base every panel extends).
product/  ModuleDef (what a module exposes), ModuleEngine (spec values ->
          DSP), SingleModuleProcessor + ProductEditor (a module as a plugin).
rack/     SlotParameter (one generic host parameter, remapped live),
          SlotOverflow (a module's parameters past the 32nd, off the host
          grid), RackProcessor (8 engines in series), RackEditor.
```

## Rules

- `dsp/` must build with `BMO_DSP_ONLY=ON`. If you need JUCE, it does not
  belong here.
- `ParamSpec::toNormalised/fromNormalised` must agree with
  `juce::NormalisableRange` for the same range: standalone products use
  JUCE's, the rack uses ours, and `RackTests` checks they match. Do not add
  skew or non-linear ranges to one without the other.
- `SlotParameter::assign` keeps a pointer into the module's static
  `specs()` vector. Never hand it a temporary.
- A slot's `SlotOverflow` is an `AudioProcessor` only so that its
  parameters have an index; JUCE asserts on a gesture without one. Never add
  it to a host, a graph or an editor. It is created and destroyed in
  `rebuild`, after the slot's engine has gone, and a slot's `overflow` member
  is declared before `engine` for the same reason.
- Anything that changes a `ModuleEngine` in the rack goes through
  `RackProcessor::rebuild`, which calls `rackChainWillChange` before and
  `rackChainChanged` after, synchronously, so the editor drops its panels
  before their engines die. Keep it that way.
- `processBlock` in the rack takes a `ScopedTryLock` and passes audio
  through if the message thread is mid-rebuild. Never block the audio
  thread on the chain lock.
- Tokens are the only place colours live. A panel that needs a colour
  takes it from `ui::tokens()` or from its module's `accent`.
