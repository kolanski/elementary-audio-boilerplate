#pragma once

#include "choc/gui/choc_WebView.h"
#include "PluginProcessor.h"

class AudioPluginAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit AudioPluginAudioProcessorEditor(EffectsPluginProcessor &processor);
    ~AudioPluginAudioProcessorEditor() override;
    std::unique_ptr<choc::ui::WebView> chocWebView;


    void paint(juce::Graphics &) override;
    void resized() override;

private:
    EffectsPluginProcessor &processorRef;
    std::unique_ptr<juce::Component> juceWebViewHolder;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};