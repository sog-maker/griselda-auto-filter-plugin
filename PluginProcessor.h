// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Schlensog Audio Lab

#pragma once
#include <JuceHeader.h>
#include <array>

class GriseldaAutoFilterAudioProcessor : public juce::AudioProcessor
{
public:
    GriseldaAutoFilterAudioProcessor();
    ~GriseldaAutoFilterAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                    { return true; }
    const juce::String getName() const override       { return JucePlugin_Name; }
    bool acceptsMidi() const override                 { return false; }
    bool producesMidi() const override                { return false; }
    bool isMidiEffect() const override                { return false; }
    double getTailLengthSeconds() const override      { return 0.0; }

    int getNumPrograms() override                     { return 1; }
    int getCurrentProgram() override                  { return 0; }
    void setCurrentProgram (int) override             {}
    const juce::String getProgramName (int) override  { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    using APVTS = juce::AudioProcessorValueTreeState;
    APVTS apvts;
    static APVTS::ParameterLayout createParameterLayout();

    static constexpr int analyzerFFTOrder = 11;
    static constexpr int analyzerFFTSize  = 1 << analyzerFFTOrder;
    using AnalyzerArray = std::array<float, analyzerFFTSize / 2>;

    AnalyzerArray getAnalyzerSnapshot() const;
    float getEnvelopeValue() const noexcept { return envelopeVisual.load(); }

private:
    using IIRFilter = juce::dsp::IIR::Filter<float>;
    using Coeff     = juce::dsp::IIR::Coefficients<float>;
    using SVF       = juce::dsp::StateVariableTPTFilter<float>;

    static constexpr int maxFilterStages   = 4;
    static constexpr int maxChannels       = 2;
    static constexpr int pitchBufferSize   = 1 << 15;
    static constexpr int stretchBufferSize = 1 << 16;
    using FilterBank = std::array<IIRFilter, maxChannels>;

    std::array<FilterBank, maxFilterStages> highPassStages;
    std::array<FilterBank, maxFilterStages> lowPassStages;
    FilterBank lowShelf;
    FilterBank midPeak;
    FilterBank highShelf;
    std::array<SVF, maxChannels> distortionToneFilters;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inputGainSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGainSmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> dryWetSmoothed;

    struct PitchShifter
    {
        void prepare (double newSampleRate, int channels);
        void reset();
        float processSample (int channel, float x, float semitones, float fineCents, float mix);
    private:
        std::array<std::vector<float>, 2> ringBuffers;
        std::array<int, 2> writePositions {};
        double sampleRate = 44100.0;
        float phase = 0.0f;
    };

    struct GrainStretch
    {
        void prepare (double newSampleRate, int channels);
        void reset();
        float processSample (int channel, float x, float amountPct, float grainMs, float smoothPct, float mix);
    private:
        struct ChannelState
        {
            std::vector<float> ringBuffer;
            int writePosition = 0;
            float phasor = 0.0f;
            float toneState = 0.0f;
        };

        std::array<ChannelState, 2> channels;
        double sampleRate = 44100.0;
    };

    PitchShifter pitchShifter;
    GrainStretch grainStretch;

    double currentSampleRate = 44100.0;
    float lfoPhase = 0.0f;
    float randomLfoValue = 0.0f;
    float lastRandomLfoTrigger = 0.0f;
    float envelopeFollower = 0.0f;
    std::atomic<float> envelopeVisual { 0.0f };

    juce::dsp::FFT forwardFFT { analyzerFFTOrder };
    juce::dsp::WindowingFunction<float> fftWindow { analyzerFFTSize, juce::dsp::WindowingFunction<float>::hann, true };
    std::array<float, analyzerFFTSize> analyzerFifo {};
    std::array<float, analyzerFFTSize * 2> fftData {};
    int analyzerFifoIndex = 0;
    mutable juce::SpinLock analyzerLock;
    AnalyzerArray analyzerMagnitudes {};

    void updateFilterCoefficients (float hpHz, float lpHz, float resonance, int activeStages);
    void updateEqCoefficients();
    void updateToneFilters (float tonePct);
    float processDistortionSample (float x, int type, float drivePct) const;
    static float foldback (float x, float threshold);
    static float fastTanh (float x);
    void pushNextAnalyzerSample (float sample) noexcept;
    void computeAnalyzerFrame() noexcept;
    float getLfoValue (float rateHz, int shapeIndex, bool sync, double hostBpm, int numSamples) noexcept;
    double getHostBpmOrDefault() const;
    static float readInterpolated (const std::vector<float>& buffer, float index) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GriseldaAutoFilterAudioProcessor)
};
