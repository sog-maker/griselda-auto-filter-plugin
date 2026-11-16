// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Schlensog Audio Lab

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class GriseldaAutoFilterAudioProcessorEditor : public juce::AudioProcessorEditor,
                                               private juce::Timer
{
public:
    explicit GriseldaAutoFilterAudioProcessorEditor (GriseldaAutoFilterAudioProcessor&);
    ~GriseldaAutoFilterAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    enum class Kind { knob, toggle, inverted, choice };

    struct Binding
    {
        juce::String uiId, parameterId;
        Kind kind;
        double displayScale;
        int decimals;
        float lastSent;
    };

    GriseldaAutoFilterAudioProcessor& audioProcessor;
    std::vector<Binding> bindings;
    std::unique_ptr<juce::WebBrowserComponent> browser;
    std::unique_ptr<juce::FileChooser> chooser;
    bool pageReady = false;
    bool liveSpectrumShown = false;
    int fitAttemptsLeft = 3;

    void timerCallback() override;
    void handleMessage (const juce::var& message);
    void setFromUi (const juce::String& uiId, float value);
    void sendBinding (Binding& binding);
    void sendSpectrum();
    void updateBrowserBounds();
    double getHostStretch() const;
    void fitWindowToDesign();
    void applyPreset (int presetId);
    void setParam (const juce::String& id, float realValue);
    void setChoice (const juce::String& id, int zeroBasedIndex);
    void savePresetToXml();
    void loadPresetFromXml();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GriseldaAutoFilterAudioProcessorEditor)
};
