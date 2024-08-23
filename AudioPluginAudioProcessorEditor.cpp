#include "PluginProcessor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include "choc/gui/choc_WebView.h"

//==============================================================================
class AudioPluginAudioProcessorEditor final
    : public juce::AudioProcessorEditor
{
public:
    explicit AudioPluginAudioProcessorEditor (AudioPluginAudioProcessor& p)
        : AudioProcessorEditor (&p), processorRef (p), valueTreeState (p.getValueTreeState())
    {
        // Create WebView options
        choc::ui::WebView::Options options;
        
        // Enable debug mode based on build configuration
        #if JUCE_DEBUG
        options.enableDebugMode = true;
        #else
        options.enableDebugMode = false;
        #endif
        
        // Instantiate the WebView with options
        chocWebView = std::make_unique<choc::ui::WebView> (options);
        
        // Set the HTML content to be displayed
        const auto htmlContent = R"(
            <!DOCTYPE html>
            <html lang="en">
            <head>
                <meta charset="UTF-8">
                <meta name="viewport" content="width=device-width, initial-scale=1.0">
                <title>Static WebView</title>
            </head>
            <body>
                <h1>Welcome to CHOC WebView</h1>
                <p>This is a static HTML content loaded directly into the WebView.</p>
            </body>
            </html>
        )";
        
        // Load the HTML content into the WebView
        chocWebView->setHTML (std::string (htmlContent));
        
        // Create a JUCE component holder for the WebView
        juceWebViewHolder = createJUCEWebViewHolder (*chocWebView.get());
        addAndMakeVisible (juceWebViewHolder.get());

        // Set the editor size
        setSize (800, 400);
        setResizable (true, true);
    }

    ~AudioPluginAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override {}
    void resized() override
    {
        // Set bounds for the WebView holder
        juceWebViewHolder->setBounds (getLocalBounds());
    }

private:
    AudioPluginAudioProcessor& processorRef;
    juce::AudioProcessorValueTreeState& valueTreeState;
    
    std::unique_ptr<choc::ui::WebView> chocWebView;
    std::unique_ptr<juce::Component> juceWebViewHolder;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};

// In the PluginProcessor.cpp file
juce::AudioProcessorEditor* AudioPluginAudioProcessor::createEditor()
{
    return new AudioPluginAudioProcessorEditor(*this);
}