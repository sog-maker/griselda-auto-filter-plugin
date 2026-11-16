// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2026 Schlensog Audio Lab

#include "PluginEditor.h"
#include "BinaryData.h"

#if JUCE_WINDOWS
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
#endif

namespace
{
    constexpr int windowWidth = 704, windowHeight = 461;
    constexpr size_t barCount = 132;

    juce::String mimeTypeFor (const juce::String& fileName)
    {
        const auto extension = fileName.fromLastOccurrenceOf (".", false, false).toLowerCase();
        if (extension == "html")  return "text/html";
        if (extension == "js")    return "text/javascript";
        if (extension == "css")   return "text/css";
        if (extension == "woff2") return "font/woff2";
        if (extension == "svg")   return "image/svg+xml";
        return "application/octet-stream";
    }

    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url)
    {
        const auto fileName = url == "/" ? juce::String ("index.html")
                                         : url.upToFirstOccurrenceOf ("?", false, false).fromLastOccurrenceOf ("/", false, false);

        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            if (fileName == BinaryData::originalFilenames[i])
            {
                int size = 0;
                const auto* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
                const auto* bytes = reinterpret_cast<const std::byte*> (data);
                return juce::WebBrowserComponent::Resource { { bytes, bytes + size }, mimeTypeFor (fileName) };
            }
        }

        return std::nullopt;
    }

    const char* const bridgeScript = R"(
        window.__juce__ = {
            postMessage: function (message) { window.__JUCE__.backend.emitEvent ("griselda", message); }
        };
    )";
}

GriseldaAutoFilterAudioProcessorEditor::GriseldaAutoFilterAudioProcessorEditor (GriseldaAutoFilterAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    const auto knob = [this] (const char* uiId, const char* parameterId, double scale, int decimals)
    {
        bindings.push_back ({ uiId, parameterId, Kind::knob, scale, decimals, -1.0f });
    };
    const auto other = [this] (const char* uiId, const char* parameterId, Kind kind)
    {
        bindings.push_back ({ uiId, parameterId, kind, 1.0, 0, -1.0f });
    };

    knob ("cutoff",   "LP_CUTOFF",   1.0,  2); knob ("highpass", "HP_CUTOFF",   1.0,  2);
    knob ("reso",     "RESONANCE",   1.0,  2); knob ("fmix",     "FILTER_MIX",  0.01, 2);
    knob ("damount",  "DIST_DRIVE",  0.01, 2); knob ("dtone",    "DIST_TONE",   0.01, 2); knob ("dmix", "DIST_MIX", 0.01, 2);
    knob ("stretch",  "STRETCH",     0.01, 2); knob ("grain",    "GRAIN_SIZE",  1.0,  2);
    knob ("smooth",   "SMOOTH",      0.01, 2); knob ("tmix",     "STRETCH_MIX", 0.01, 2);
    knob ("rate",     "LFO_RATE",    1.0,  3); knob ("depth",    "LFO_DEPTH",   0.01, 2);
    knob ("semitone", "PITCH_SEMI",  1.0,  0); knob ("fine",     "PITCH_FINE",  1.0,  2); knob ("pmix", "PITCH_MIX", 0.01, 2);
    knob ("input",    "INPUT_GAIN",  1.0,  2); knob ("output",   "OUTPUT_GAIN", 1.0,  2); knob ("drywet", "DRY_WET", 0.01, 2);
    knob ("eqlow",    "LOW_GAIN",    1.0,  2); knob ("eqmid",    "MID_GAIN",    1.0,  2); knob ("eqhigh", "HIGH_GAIN", 1.0, 2);

    other ("on_filter",      "FILTER_ON",  Kind::toggle);
    other ("on_distortion",  "DIST_ON",    Kind::toggle);
    other ("on_timestretch", "STRETCH_ON", Kind::toggle);
    other ("on_lfo",         "MOD_ON",     Kind::toggle);
    other ("on_pitch",       "PITCH_ON",   Kind::toggle);
    other ("on_eq",          "EQ_ON",      Kind::toggle);
    other ("power",          "BYPASS",     Kind::inverted);
    other ("dd_dist",        "DIST_TYPE",  Kind::choice);
    other ("dd_shape",       "LFO_SHAPE",  Kind::choice);
    other ("dd_target",      "LFO_TARGET", Kind::choice);

    using Browser = juce::WebBrowserComponent;
    browser = std::make_unique<Browser> (Browser::Options {}
        .withBackend (Browser::Options::Backend::webview2)
        .withWinWebView2Options (Browser::Options::WinWebView2 {}
            .withUserDataFolder (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("GriseldaAutoFilterWebView"))
            .withBackgroundColour (juce::Colour (0xff050c0a))
            .withStatusBarDisabled())
        .withNativeIntegrationEnabled()
        .withKeepPageLoadedWhenBrowserIsHidden()
        .withUserScript (bridgeScript)
        .withEventListener ("griselda", [this] (const juce::var& message) { handleMessage (message); })
        .withResourceProvider ([] (const juce::String& url) { return getResource (url); }));
    addAndMakeVisible (*browser);

    setResizable (false, false);
    setSize (windowWidth, windowHeight);

    browser->goToURL (Browser::getResourceProviderRoot());
    startTimerHz (30);
}

GriseldaAutoFilterAudioProcessorEditor::~GriseldaAutoFilterAudioProcessorEditor()
{
    stopTimer();
}

void GriseldaAutoFilterAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff050c0a));
}

void GriseldaAutoFilterAudioProcessorEditor::resized()
{
    updateBrowserBounds();
}

double GriseldaAutoFilterAudioProcessorEditor::getHostStretch() const
{
   #if JUCE_WINDOWS
    if (auto* peer = getPeer())
    {
        const auto window = (HWND) peer->getNativeHandle();
        const auto isUnaware = [] (DPI_AWARENESS_CONTEXT context)
        {
            return GetAwarenessFromDpiAwarenessContext (context) == DPI_AWARENESS_UNAWARE;
        };

        if (isUnaware (GetWindowDpiAwarenessContext (window)) && ! isUnaware (GetDpiAwarenessContextForProcess (nullptr)))
        {
            RECT stretched {}, unscaled {};
            const auto previous = SetThreadDpiAwarenessContext (DPI_AWARENESS_CONTEXT_UNAWARE);
            GetWindowRect (window, &unscaled);
            SetThreadDpiAwarenessContext (DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            GetWindowRect (window, &stretched);
            SetThreadDpiAwarenessContext (previous);

            if (unscaled.right > unscaled.left)
                return (double) (stretched.right - stretched.left) / (double) (unscaled.right - unscaled.left);
        }
    }
   #endif

    return 1.0;
}

void GriseldaAutoFilterAudioProcessorEditor::fitWindowToDesign()
{
   #if JUCE_WINDOWS
    auto* peer = getPeer();
    if (fitAttemptsLeft <= 0 || peer == nullptr)
        return;

    const auto window = (HWND) peer->getNativeHandle();
    if (GetAwarenessFromDpiAwarenessContext (GetWindowDpiAwarenessContext (window)) == DPI_AWARENESS_UNAWARE)
        return;

    RECT client {};
    const auto previous = SetThreadDpiAwarenessContext (DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    GetClientRect (window, &client);
    const auto targetWidth = (double) windowWidth * (double) GetDpiForWindow (window) / 96.0;
    SetThreadDpiAwarenessContext (previous);

    if (client.right <= 0)
        return;

    if (std::abs ((double) client.right - targetWidth) <= 3.0)
    {
        fitAttemptsLeft = 0;
        return;
    }

    --fitAttemptsLeft;
    const auto correction = targetWidth / (double) client.right;
    setSize (juce::roundToInt (getWidth() * correction), juce::roundToInt (getHeight() * correction));
   #endif
}

void GriseldaAutoFilterAudioProcessorEditor::updateBrowserBounds()
{
    const auto stretch = getHostStretch();
    browser->setBounds (0, 0, juce::roundToInt (getWidth() * stretch), juce::roundToInt (getHeight() * stretch));
}

void GriseldaAutoFilterAudioProcessorEditor::handleMessage (const juce::var& message)
{
    const auto json = juce::JSON::parse (message.toString());
    const auto type = json["type"].toString();

    if (type == "READY")
    {
        pageReady = true;
        for (auto& binding : bindings)
            binding.lastSent = -1.0f;
        updateBrowserBounds();
    }
    else if (type == "SET_PARAM")
    {
        setFromUi (json["id"].toString(), (float) (double) json["value"]);
    }
}

void GriseldaAutoFilterAudioProcessorEditor::setFromUi (const juce::String& uiId, float value)
{
    if (uiId == "export") { savePresetToXml(); return; }
    if (uiId == "import") { loadPresetFromXml(); return; }
    if (uiId == "dd_preset") { applyPreset (juce::roundToInt (value * 4.0f) + 1); return; }

    for (const auto& binding : bindings)
    {
        if (binding.uiId != uiId)
            continue;

        if (auto* p = audioProcessor.apvts.getParameter (binding.parameterId))
        {
            auto normalised = value;
            if (binding.kind == Kind::inverted) normalised = value > 0.5f ? 0.0f : 1.0f;
            if (binding.kind == Kind::toggle)   normalised = value > 0.5f ? 1.0f : 0.0f;

            p->beginChangeGesture();
            p->setValueNotifyingHost (normalised);
            p->endChangeGesture();
        }
        return;
    }
}

void GriseldaAutoFilterAudioProcessorEditor::sendBinding (Binding& binding)
{
    auto* p = audioProcessor.apvts.getParameter (binding.parameterId);
    if (p == nullptr)
        return;

    const auto normalised = p->getValue();
    if (std::abs (normalised - binding.lastSent) < 1.0e-6f)
        return;
    binding.lastSent = normalised;

    juce::DynamicObject::Ptr detail (new juce::DynamicObject());
    detail->setProperty ("id", binding.uiId);

    switch (binding.kind)
    {
        case Kind::knob:
        {
            auto shown = (double) p->convertFrom0to1 (normalised) * binding.displayScale;
            if (std::abs (shown) < 0.5 * std::pow (10.0, -binding.decimals)) shown = 0.0;
            detail->setProperty ("value", normalised);

            juce::DynamicObject::Ptr text (new juce::DynamicObject());
            text->setProperty ("id", binding.uiId);
            text->setProperty ("text", juce::String (shown, binding.decimals));
            browser->evaluateJavascript ("window.dispatchEvent(new CustomEvent('paramText',{detail:"
                                         + juce::JSON::toString (juce::var (text.get()), true) + "}))");
            break;
        }
        case Kind::toggle:   detail->setProperty ("value", normalised > 0.5f ? 1 : 0); break;
        case Kind::inverted: detail->setProperty ("value", normalised > 0.5f ? 0 : 1); break;
        case Kind::choice:   detail->setProperty ("value", normalised); break;
    }

    browser->evaluateJavascript ("window.dispatchEvent(new CustomEvent('paramUpdate',{detail:"
                                 + juce::JSON::toString (juce::var (detail.get()), true) + "}))");
}

void GriseldaAutoFilterAudioProcessorEditor::sendSpectrum()
{
    const auto mags = audioProcessor.getAnalyzerSnapshot();
    const auto powered = audioProcessor.apvts.getRawParameterValue ("BYPASS")->load() < 0.5f;
    const auto hasSignal = powered && *std::max_element (mags.begin(), mags.end()) > 0.12f;

    if (! hasSignal)
    {
        if (liveSpectrumShown)
            browser->evaluateJavascript ("window.dispatchEvent(new CustomEvent('spectrum',{detail:null}))");
        liveSpectrumShown = false;
        return;
    }

    liveSpectrumShown = true;
    juce::String bars;
    const auto lastBin = (float) mags.size() - 1.0f;
    for (size_t i = 0; i < barCount; ++i)
    {
        const auto binFrom = std::pow (lastBin, (float) i / (float) barCount);
        const auto binTo   = std::pow (lastBin, (float) (i + 1) / (float) barCount);
        float v = 0.0f;
        for (auto bin = (size_t) binFrom; bin <= (size_t) binTo; ++bin)
            v = juce::jmax (v, mags[bin]);
        bars << juce::String (juce::jlimit (0.02f, 1.0f, juce::jmap (v, 0.25f, 0.95f, 0.02f, 1.0f)), 3) << (i + 1 < barCount ? "," : "");
    }

    browser->evaluateJavascript ("window.dispatchEvent(new CustomEvent('spectrum',{detail:{bars:[" + bars + "],level:"
                                 + juce::String (audioProcessor.getEnvelopeValue(), 3) + "}}))");
}

void GriseldaAutoFilterAudioProcessorEditor::timerCallback()
{
    fitWindowToDesign();

    if (! pageReady)
        return;

    for (auto& binding : bindings)
        sendBinding (binding);

    sendSpectrum();
}

void GriseldaAutoFilterAudioProcessorEditor::setParam (const juce::String& id, float realValue)
{
    if (auto* p = audioProcessor.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
        p->endChangeGesture();
    }
}

void GriseldaAutoFilterAudioProcessorEditor::setChoice (const juce::String& id, int zeroBasedIndex)
{
    setParam (id, (float) zeroBasedIndex);
}

void GriseldaAutoFilterAudioProcessorEditor::applyPreset (int presetId)
{
    switch (presetId)
    {
        case 1:
            setParam ("INPUT_GAIN", 0.0f); setParam ("OUTPUT_GAIN", 0.0f); setParam ("DRY_WET", 50.0f);
            setParam ("LP_CUTOFF", 20000.0f); setParam ("HP_CUTOFF", 20.0f); setParam ("RESONANCE", 0.10f); setParam ("FILTER_MIX", 0.0f);
            setParam ("LOW_GAIN", 0.0f); setParam ("MID_GAIN", 0.0f); setParam ("HIGH_GAIN", 0.0f);
            setParam ("LFO_RATE", 0.05f); setParam ("LFO_DEPTH", 0.0f); setParam ("ENV_AMOUNT", 0.0f);
            setParam ("DIST_DRIVE", 0.0f); setParam ("DIST_TONE", 0.0f); setParam ("DIST_MIX", 0.0f);
            setParam ("PITCH_SEMI", 0.0f); setParam ("PITCH_FINE", 0.0f); setParam ("PITCH_MIX", 0.0f);
            setParam ("STRETCH", 0.0f); setParam ("GRAIN_SIZE", 5.0f); setParam ("SMOOTH", 0.0f); setParam ("STRETCH_MIX", 0.0f);
            setParam ("FILTER_ON", 1.0f); setParam ("EQ_ON", 1.0f); setParam ("MOD_ON", 1.0f); setParam ("DIST_ON", 1.0f); setParam ("PITCH_ON", 1.0f); setParam ("STRETCH_ON", 1.0f);
            break;
        case 2:
            setParam ("FILTER_ON", 1.0f); setParam ("EQ_ON", 0.0f); setParam ("MOD_ON", 1.0f); setParam ("DIST_ON", 1.0f); setParam ("PITCH_ON", 1.0f); setParam ("STRETCH_ON", 0.0f);
            setParam ("LP_CUTOFF", 950.0f); setParam ("HP_CUTOFF", 120.0f); setParam ("FILTER_MIX", 100.0f); setParam ("RESONANCE", 0.62f);
            setParam ("LFO_RATE", 2.8f); setParam ("LFO_DEPTH", 82.0f); setChoice ("LFO_TARGET", 0);
            setChoice ("DIST_TYPE", 1); setParam ("DIST_DRIVE", 30.0f); setParam ("DIST_TONE", 28.0f); setParam ("DIST_MIX", 18.0f);
            setParam ("PITCH_SEMI", -7.0f); setParam ("PITCH_FINE", 0.0f); setParam ("PITCH_MIX", 35.0f);
            setParam ("DRY_WET", 100.0f);
            break;
        case 3:
            setParam ("FILTER_ON", 1.0f); setParam ("EQ_ON", 1.0f); setParam ("MOD_ON", 0.0f); setParam ("DIST_ON", 1.0f); setParam ("PITCH_ON", 0.0f); setParam ("STRETCH_ON", 1.0f);
            setParam ("LP_CUTOFF", 3100.0f); setParam ("HP_CUTOFF", 120.0f); setParam ("FILTER_MIX", 78.0f);
            setChoice ("DIST_TYPE", 1); setParam ("DIST_DRIVE", 64.0f); setParam ("DIST_TONE", 28.0f); setParam ("DIST_MIX", 72.0f);
            setParam ("LOW_GAIN", 2.0f); setParam ("MID_GAIN", -1.0f); setParam ("HIGH_GAIN", -4.0f);
            setParam ("STRETCH", 32.0f); setParam ("GRAIN_SIZE", 52.0f); setParam ("SMOOTH", 48.0f); setParam ("STRETCH_MIX", 40.0f);
            setParam ("DRY_WET", 100.0f);
            break;
        case 4:
            setParam ("FILTER_ON", 1.0f); setParam ("EQ_ON", 0.0f); setParam ("MOD_ON", 0.0f); setParam ("DIST_ON", 1.0f); setParam ("PITCH_ON", 0.0f); setParam ("STRETCH_ON", 0.0f);
            setParam ("LP_CUTOFF", 1800.0f); setParam ("RESONANCE", 0.40f); setParam ("FILTER_MIX", 100.0f);
            setChoice ("DIST_TYPE", 1); setParam ("DIST_DRIVE", 88.0f); setParam ("DIST_TONE", 14.0f); setParam ("DIST_MIX", 90.0f);
            setParam ("DRY_WET", 100.0f);
            break;
        case 5:
            setParam ("FILTER_ON", 1.0f); setParam ("EQ_ON", 1.0f); setParam ("MOD_ON", 0.0f); setParam ("DIST_ON", 0.0f); setParam ("PITCH_ON", 0.0f); setParam ("STRETCH_ON", 1.0f);
            setParam ("LP_CUTOFF", 6200.0f); setParam ("HP_CUTOFF", 45.0f); setParam ("FILTER_MIX", 48.0f);
            setParam ("LOW_GAIN", 1.2f); setParam ("MID_GAIN", 1.8f); setParam ("HIGH_GAIN", -2.8f);
            setParam ("STRETCH", 58.0f); setParam ("GRAIN_SIZE", 88.0f); setParam ("SMOOTH", 64.0f); setParam ("STRETCH_MIX", 54.0f);
            setParam ("DRY_WET", 100.0f);
            break;
        default:
            break;
    }
}

void GriseldaAutoFilterAudioProcessorEditor::savePresetToXml()
{
    chooser = std::make_unique<juce::FileChooser> ("Preset exportieren", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.xml");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
    {
        if (fc.getResult() == juce::File()) return;
        if (auto state = audioProcessor.apvts.copyState(); state.isValid())
        {
            std::unique_ptr<juce::XmlElement> xml (state.createXml());
            if (xml != nullptr) xml->writeTo (fc.getResult());
        }
    });
}

void GriseldaAutoFilterAudioProcessorEditor::loadPresetFromXml()
{
    chooser = std::make_unique<juce::FileChooser> ("Preset importieren", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.xml");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        if (fc.getResult() == juce::File()) return;
        std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (fc.getResult()));
        if (xml != nullptr && xml->hasTagName (audioProcessor.apvts.state.getType()))
            audioProcessor.apvts.replaceState (juce::ValueTree::fromXml (*xml));
    });
}
