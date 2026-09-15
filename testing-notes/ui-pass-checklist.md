# The UI pass — checklist

Written on **AURORA**, 2026-09-14, from renders of every panel at `cbd0939`
in both appearances (`snapshot` and `bmo-tune-snapshot`, with `signal=-18`
so meters read). Stage 3 of `WORKFLOWS.md`: runs alone, after the per-module
listening pass, on branch `ui-pass` off `integration`. Read
`docs/ui-workflow-brief.md` and `testing-notes/ui-editor-handoff.md` first;
the rules a panel inherits are in `modules/AGENTS.md`.

**The loop:** `scripts/build.sh --snapshots`, or one render with
`build/tools/Debug/snapshot.exe <id> out.png appearance=dark|light`, then
`tools/inspect` for any colour or alignment claim. Both appearances, every
time. Hash renders before and after any change that should move nothing.
`ui_layout_tests` must stay green; it pins the shared rows.

## A. Suite-wide, in this order

- [ ] **BMO EQ → BMO CEQ rename.** `products/eq/Product.h` name and folder,
      `products/eq/CMakeLists.txt` PRODUCT_NAME, `modules/eq/params.h`
      kModuleName, the identity and accent tables, README, packager
      README. `Fsty`, the bundle id, module id `eq` and the schema stay.
      Needs the second preset-migration hop: `PresetInfo` grows a list of
      legacy (folder, extension) pairs walked oldest first, and
      `EqTests.cpp` updated. Users delete the old `BMO EQ.vst3`.
- [ ] **`utilGain` placeholder** (`core/ui/Tokens.h`, `#9c71c3`): Util's
      VOLUME reads as a Dimension control in a rack (hue 272° against
      Dimension's 271.6°) and measures about 3.6:1 on the dark plate, under
      every shipped accent. Options with numbers: (1) Util's own green
      `#7fc98a`, so VOLUME is simply the module's colour like every other
      module's headline knob; (2) the utility azure `#4fb8e8`, the colour
      every panel's trim knobs already wear, arguing VOLUME is a trim;
      (3) a fresh hue in the 40–70° gap (gold `#e8c95a`, 8.33:1 dark) that
      nothing else uses. Frosty's call; (1) is the one that needs no new
      colour.
- [ ] **Contrast assertions** (`docs/ui-workflow-brief.md` §4): a pure
      function of the tokens, no rendering. Every (ink, ground) pair the
      tokens permit gets a floor. Cheapest open item in the brief.
- [ ] **Rest-dot check** on every knob whose default is not at an end.
- [ ] **Header bars and accents** read as different modules side by side:
      DEQ teal next to the azure trims, Vcomp periwinkle next to Dimension
      lavender, Tune lime on the pale plate (1.29:1, Frosty kept it).
- [ ] Every caption fits in both appearances (`ui_layout_tests`), and no
      hex outside `Tokens.h`.

## B. Per panel, what the renders showed

### BMO Util (320 wide)
- [ ] VOLUME colour (above).
- [ ] Two large empty bands, between the rule under VOLUME and PAN, and
      between MONO and the lower rule. Either the knobs grow or the panel
      says why it is spaced this way. Compare with EQ's column, which has
      no empty band over 16 px.

### BMO EQ / CEQ (280)
- [ ] Settled; the most finished panel in the suite. Only the rename.
- [ ] Four parameters have no control (High Cut, Mix, Auto Gain,
      Oversampling). Decide on the record whether that is intended.
      Telephone and Mix Bus Sheen depend on two of them.
- [ ] Low-cut crowding is parked at Frosty's request; leave it.

### BMO Saturator (260)
- [ ] Settled. DRIVE, TONE, MIX, three switches, both sections taken.

### BMO Opto (220)
- [ ] Settled by the 2026-09-11 decisions: TELE / ELD / COLOR names, LINK
      stays. Engaged colour red in Tele, amber in Stressed, both renders
      confirm.
- [x] ~~Takes no output section on purpose … decide once whether that is
      right now that Vcomp also has an output knob.~~ **Settled 2026-09-14:
      it is right, and Opto does not move.** LTV Comp's knob turned out to be
      a makeup stage rather than an output trim, so it is captioned MAKEUP and
      keeps off the shared line for the same reason this one does. The two
      compressors agree, and neither takes the section. See the LTV Comp entry
      for the rejected candidate and its numbers.

### BMO Dimension (220)
- [ ] `dim-ui-pass-plan.md` §1: L / R end marks on ROTATE and ASYM instead
      of − / +. Not implemented; render shows − / +.
- [ ] DETUNE switch sits over CENTS and DIFFUSE but gates only CENTS. Move
      it over CENTS alone, or dim CENTS when off (`PlainKnob::setKnobEnabled`
      exists and is unused suite-wide; this is its case).
- [ ] FREQ caption is generic: it is SHUFFLE's corner. Candidate to fix at
      650–700 Hz like RATE and DEPTH were, if the pass finds nobody moves it.
- [ ] No output trim and no meter. The DSP review measured +3.5 dB peak
      on Wide Vocal and +19 dB reachable; a trim is a schema append (new
      parameter at the end of `specs()`, default 0, `kVersionHint` bump),
      so it is a product decision, not a panel one.
- [ ] Light mode: the lavender captions measure 1.73:1 on the pale plate.
      Still undecided, still being raised.

### BMO DEQ (320 compact / 600 expanded)
- [ ] Solo and the analyser exist in the engine and are tested, and the
      panel has neither. `spec/decisions.md` reads as if both shipped.
      Either wire them in this pass or say in the decisions file that they
      wait.
- [ ] GR bar shows the deepest cut across bands; an upward band shows
      nothing. Label or redesign.
- [ ] The response view draws the 48 kHz design whatever the rate; up to
      about 1 dB off in the top octave at 44.1 or 96 k. Needs the rate in
      `ModuleContext` or a note.
- [ ] Compact view: band tabs 7–12 in a second row, the dynamics block
      greyed when DYN is off. Reads well; check the 320 width in a rack
      next to EQ.

### LTV Comp (260)
- [x] ~~**Takes neither section.** OUTPUT floats mid-panel … take the output
      section (the knob is a trim by function).~~ **Settled 2026-09-14, and
      the item's premise was wrong twice over.**

      The knob is **not a trim**: it is makeup on top of AMOUNT's automatic
      makeup, which the parameter's own spec comment always said and the
      caption never did. It is captioned **MAKEUP** now — the same control as
      BMO Opto's, down to range, step and default. So neither compressor takes
      the shared output line, and they agree for a stated reason instead of by
      accident. The second wrong premise was "Opto and Vcomp disagree": the
      layout dump shows neither has ever had a rule at 566 or a knob at
      602..679.

      The both-take-it candidate was built and rendered before this came out,
      and is on the record as rejected on its own numbers. It gives the rack a
      real bottom rail — three rules, three switch rows, three knobs on one
      line — but it moves this panel's empty band out of the foot (190 px,
      under COMPLEX, which explains it) and into the middle (**262 px**, where
      nothing does), and it costs BMO Opto the head/foot mirror, since
      LINK/COLOR stacked want 60 px and the shared switch row is 28.

      **Do not re-raise "give the freed height to the meter block."**
      `VcompPanel.h` records that re-flowing standard mode into that space was
      considered and rejected: it is the price of controls that stay put when
      COMPLEX toggles, the same reasoning as BMO Opto's old hide-and-shuffle
      COLOR switch.
- [ ] The meter block: three bars with 12 dB ticks. Decide whether GR
      right-to-left reads, and whether the gate handle on IN needs a
      printed threshold (there is no numeric readout of it anywhere).
- [ ] Clicking the IN caption sets the gate to −60 (hit-test the well, not
      the row).
- [ ] Names are first drafts: AMOUNT, LOW THRU, HIGH THRU, SC HPF, COMPLEX,
      and the product name. Accent `#a2a8ff` not signed off.

### BMO Tune RT (360)
- [ ] The middle third of the panel is empty: keyboard and selectors at
      the top, three knobs at the foot, a rule and nothing between. Either
      the knobs come up, or a note / pitch readout goes there (the DSP has
      nothing exposed for one today; it would be a sixth callback and a
      float from `TuneCore`).
- [ ] The keyboard's dot on C is the key indicator; check it reads as such
      in both appearances and at a glance.
- [ ] Lime on the pale plate, 1.29:1: Frosty kept it, so only re-open with
      a render that reads badly.

### The rack
- [ ] Input knobs, switch rows and output knobs on the shared rows for
      every module that takes them; a count of who does and does not, on
      the record.
- [ ] Slot bar: the DEQ expand button, the `<` `>` `x` controls, module
      names in the accent. The rack's own header is pink (EQ's accent);
      decide whether the rack should have its own.

## C. Done means
- [ ] `scripts/build.sh --snapshots` green, snapshots looked at in both
      appearances.
- [ ] Every ratio quoted against a named ground, from `tools/inspect`.
- [ ] `testing-notes/ui-pass-<date>.md` with before/after hashes and
      Frosty's calls recorded at the call sites.
