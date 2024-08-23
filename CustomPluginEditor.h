#include <juce_gui_basics/juce_gui_basics.h>
#include <choc_javascript_QuickJS.h>
#include <choc_WebView.h>

class CustomAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   public choc::web::WebView::Listener
{
public:
    CustomAudioProcessorEditor (juce::AudioProcessor& processor)
        : AudioProcessorEditor (processor), audioProcessor (processor)
    {
        setSize (400, 300);

        // Initialize and configure CHOC WebView
        webView = std::make_unique<choc::web::WebView>();

        // Load the HTML file into the WebView
        auto htmlPath = getAssetsDirectory().getChildFile("index.html").getFullPathName();
        webView->load (juce::URL ("file://" + htmlPath));

        // Add WebView to the editor
        addAndMakeVisible (*webView);

        // Set WebView to listen to events
        webView->setListener(this);
    }

    void resized() override
    {
        webView->setBounds (getLocalBounds());
    }

    // WebView listener methods
    void handleMessageFromWebView (const juce::String& message) override
    {
        // Handle messages received from the web view
        DBG ("Received message from web view: " << message);
    }

private:
    juce::AudioProcessor& audioProcessor;
    std::unique_ptr<choc::web::WebView> webView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CustomAudioProcessorEditor)
};