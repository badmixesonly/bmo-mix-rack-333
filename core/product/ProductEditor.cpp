#include "ProductEditor.h"

namespace bmo
{

ProductEditor::ProductEditor (SingleModuleProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p),
      header (p.getInfo().name, p.getModule().accent),
      presetBar (p.getPresets()),
      designWidth (p.getModule().designWidth)
{
    ui::pollTheme();
    lookAndFeel.refreshColours();
    setLookAndFeel (&lookAndFeel);

    panel = proc.getModule().createPanel (proc.makeContext());

    plate.setBounds (0, 0, designWidth, kDesignHeight);
    header.setBounds (0, 0, designWidth, ui::ProductHeader::kHeight);
    presetBar.setBounds (0, ui::ProductHeader::kHeight, designWidth, 24);
    panel->setBounds (0, ui::ProductHeader::kHeight + 24, designWidth, ui::ModulePanel::kContentHeight);

    plate.addAndMakeVisible (header);
    plate.addAndMakeVisible (presetBar);
    plate.addAndMakeVisible (*panel);
    addAndMakeVisible (plate);

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) designWidth / (double) kDesignHeight);
    setResizeLimits (designWidth * 2 / 3, kDesignHeight * 2 / 3,
                     designWidth * 2,     kDesignHeight * 2);
    setSize (designWidth, kDesignHeight);

    // The theme file is watched, not loaded once: editing it with the plugin
    // open recolours the panel.
    startTimer (1000);
}

ProductEditor::~ProductEditor()
{
    setLookAndFeel (nullptr);
}

void ProductEditor::timerCallback()
{
    if (ui::pollTheme())
    {
        lookAndFeel.refreshColours();
        repaint();
    }
}

void ProductEditor::resized()
{
    // One uniform scale, so everything keeps its proportions.
    plate.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) designWidth));
}

} // namespace bmo
