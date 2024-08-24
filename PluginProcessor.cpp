#include "PluginProcessor.h"

#include <choc_javascript_QuickJS.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_data_structures/juce_data_structures.h>
#include "choc/gui/choc_WebView.h"

//==============================================================================
// A quick helper for locating bundled asset files
juce::File getAssetsDirectory()
{
#if JUCE_MAC
    auto assetsDir = juce::File::getSpecialLocation(juce::File::SpecialLocationType::currentApplicationFile)
                         .getChildFile("Contents/Resources/dist");
#elif JUCE_WINDOWS
    auto assetsDir = juce::File::getSpecialLocation(juce::File::SpecialLocationType::currentExecutableFile) // Plugin.vst3/Contents/<arch>/Plugin.vst3
                         .getParentDirectory()                                                              // Plugin.vst3/Contents/<arch>/
                         .getParentDirectory()                                                              // Plugin.vst3/Contents/
                         .getChildFile("Resources/dist");
#else
#error "We only support Mac and Windows here yet."
#endif

    jassert(assetsDir.isDirectory());
    return assetsDir;
}

//==============================================================================

EffectsPluginProcessor::EffectsPluginProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
// A. This is unique for our project

{
}

EffectsPluginProcessor::~EffectsPluginProcessor()
{
    for (auto &p : getParameters())
    {
        p->removeListener(this);
    }
}

class AudioPluginAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    std::unique_ptr<choc::ui::WebView> chocWebView;
    explicit AudioPluginAudioProcessorEditor(EffectsPluginProcessor &processor)
        : juce::AudioProcessorEditor(&processor), processorRef(processor)
    {
        // Create WebView options
        choc::ui::WebView::Options options;

#if JUCE_DEBUG
        options.enableDebugMode = true;
#else
        options.enableDebugMode = false;
#endif

        // Instantiate the WebView with options
        chocWebView = std::make_unique<choc::ui::WebView>(options);

        // Set the HTML content to be displayed
        const auto htmlContent = R"(
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>Looper Interface</title>
    </head>
    <body>
        <h1>Looper Interface</h1>
        
        <!-- Record/Play Buttons -->
        <button id="record">Record</button>
        <button id="play">Play</button>
        
        <!-- Volume Control -->
        <label for="volume">Volume:</label>
        <input type="range" id="volume" name="volume" min="0" max="1" step="0.01" value="0.5">
        
        <script>
            document.getElementById('record').addEventListener('click', function() {
                if (typeof onRecord === 'function') {
                    onRecord()
                        .catch(err => console.error('Error invoking onRecord:', err));
                }
            });

            document.getElementById('play').addEventListener('click', function() {
                if (typeof onPlay === 'function') {
                    onPlay()
                        .catch(err => console.error('Error invoking onPlay:', err));
                }
            });

            document.getElementById('volume').addEventListener('input', function(event) {
                const volume = event.target.value;
                if (typeof onVolumeChange === 'function') {
                    onVolumeChange([volume])
                        .catch(err => console.error('Error in onVolumeChange:', err));
                }
            });
        </script>
    </body>
    </html>
    )";

        // Load the HTML content into the WebView
        chocWebView->setHTML(std::string(htmlContent));

        juceWebViewHolder = createJUCEWebViewHolder(*chocWebView.get());
        addAndMakeVisible(juceWebViewHolder.get());

        // Set the editor size
        setSize(800, 400);
        setResizable(true, true);
    }

    ~AudioPluginAudioProcessorEditor() override = default;

    void paint(juce::Graphics &) override {}
    void resized() override
    {
        // Set bounds for the WebView holder
        juceWebViewHolder->setBounds(getLocalBounds());
    }

    void initializeWebViewBindings()
    {

        chocWebView->bind("onRecord", [this](const choc::value::ValueView &) -> choc::value::Value
                          {
        processorRef.startRecording();
        return choc::value::Value(); });

        chocWebView->bind("onPlay", [this](const choc::value::ValueView &) -> choc::value::Value
                          {
        processorRef.startPlayback();
        return choc::value::Value(); });

        chocWebView->bind("onVolumeChange", [this](const choc::value::ValueView &args) -> choc::value::Value
                          {
        if (args.isArray() && args.size() > 0 && args[0].isFloat()) {
            float volume = args[0].getFloat32();
            processorRef.setVolume(volume);
        }
        return choc::value::Value(); });
    }

private:
    EffectsPluginProcessor &processorRef;

    std::unique_ptr<juce::Component> juceWebViewHolder;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
AudioPluginAudioProcessorEditor *globvar;
choc::javascript::Context *jsRefContext;
void EffectsPluginProcessor::startRecording() {
    isRecording = true;
    recordedBuffer.setSize(getTotalNumInputChannels(), getSampleRate() * 60); // Example: 1 minute buffer
    recordedBuffer.clear();
}


void EffectsPluginProcessor::stopRecording() {
    isRecording = false;
    playbackBuffer.setSize(recordedBuffer.getNumChannels(), recordedBuffer.getNumSamples());
    playbackBuffer.copyFrom(0, 0, recordedBuffer, 0, 0, recordedBuffer.getNumSamples());
}

void EffectsPluginProcessor::startPlayback() {
    isPlaying = true;
}

void EffectsPluginProcessor::stopPlayback()
{
    isPlaying = false;
}

void EffectsPluginProcessor::setVolume(float newVolume)
{
    volume = newVolume;
}

void EffectsPluginProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &)
{
    if (isRecording)
    {
        buffer.copyFrom(0, 0, recordedBuffer, 0, 0, buffer.getNumSamples());
    }

    if (isPlaying)
    {
        buffer.applyGain(volume);
        buffer.addFrom(0, 0, playbackBuffer, 0, 0, buffer.getNumSamples());
    }
}
//==============================================================================
// B. This is how you make the UIX.
//    This generic editor is what JUICe provides.
//    This is an area where an artist could make cooler UIX.
//    Create this Blog Post => How do I use web technology to use an User Interface
juce::AudioProcessorEditor *EffectsPluginProcessor::createEditor()
{
    auto *editor = new AudioPluginAudioProcessorEditor(*this);
    editor->initializeWebViewBindings();
    globvar = editor;
    return editor;
}
void EffectsPluginProcessor::evalJsContext(const std::string &expr)
{
    jsContext.evaluateExpression("(function() {globalThis.volUp();})();");
    jsRefContext->evaluateExpression(juce::String(expr).toStdString());
    jsContext.evaluateExpression(juce::String("currentVolume += 0.1;").toStdString());
    jsRefContext->invoke("volUp");
    jsRefContext->invoke("volUp()");
}

void EffectsPluginProcessor::handleWebViewMessage(const std::string &message)
{
    auto json = choc::json::parse(message);
    if (json["type"].toString() == "volumeChange")
    {
        auto volume = json["volume"].getFloat32();
        // Handle the volume change in your DSP or other logic

        // Optionally, invoke a JavaScript function with the new volume
        std::vector<choc::value::Value> argList;
        argList.emplace_back(choc::value::Value(volume));

        try
        {
            auto result = jsContext.invokeWithArgList("__SetVolume__", argList);
            // Handle the result if needed
        }
        catch (const std::exception &e)
        {
            DBG("Error invoking JavaScript function: " << e.what());
        }
    }
}

void EffectsPluginProcessor::sendMessageToWebView(const std::string &message)
{
    globvar->chocWebView->evaluateJavascript("window.receiveMessageFromNative(" + message + ");");
    // auto dspEntryFile = getAssetsDirectory().getChildFile("main.js");

    jsRefContext->run(juce::String("volUp();").toStdString());
    jsContext.evaluateExpression(juce::String("currentVolume += 0.1;").toStdString());
    jsRefContext->invoke("volUp");
    jsRefContext->invoke("volUp()");
    evalJsContext("volumeProxy.value = 1;");
}

bool EffectsPluginProcessor::hasEditor() const
{
    return true;
}

//==============================================================================
const juce::String EffectsPluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EffectsPluginProcessor::acceptsMidi() const
{
    return false;
}

bool EffectsPluginProcessor::producesMidi() const
{
    return false;
}

bool EffectsPluginProcessor::isMidiEffect() const
{
    return false;
}

double EffectsPluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

//==============================================================================
// C. This is wher eyou handle presets.
int EffectsPluginProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
              // so this should be at least 1, even if you're not really implementing programs.
}

int EffectsPluginProcessor::getCurrentProgram()
{
    return 0;
}

void EffectsPluginProcessor::setCurrentProgram(int /* index */) {}
const juce::String EffectsPluginProcessor::getProgramName(int /* index */) { return {}; }
void EffectsPluginProcessor::changeProgramName(int /* index */, const juce::String & /* newName */) {}

//==============================================================================

// D. HOST knows what it needs ot know.
//    We know the sample rate, the block size for every callback.
//    This is the moment before we render audio

void EffectsPluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // E. Make the Elementary Runtime
    runtime = std::make_unique<elem::Runtime<float>>(sampleRate, samplesPerBlock);
    jsContext = choc::javascript::createQuickJSContext();
    jsRefContext = &jsContext;

    // F. If a user changes tehir sample rate, we want to re-run our JavaScript
    // Install some native interop functions in our JavaScript environment

    jsContext.registerFunction("__getSampleRate__", [this](choc::javascript::ArgumentList args)
                               { return choc::value::Value(getSampleRate()); });

    jsContext.registerFunction("__postNativeMessage__", [this](choc::javascript::ArgumentList args)
                               {
        runtime->applyInstructions(elem::js::parseJSON(args[0]->toString()));
        return choc::value::Value(); });

    jsContext.registerFunction("__log__", [this](choc::javascript::ArgumentList args)
                               {
        for (size_t i = 0; i < args.numArgs; ++i) {
            DBG(choc::json::toString(*args[i], true));
        }

        return choc::value::Value(); });
    // jsContext.invoke("testVol");

    // G.
    // A simple shim to write various console operations to our native __log__ handler
    jsContext.evaluateExpression(R"shim(
(function() {
  if (typeof globalThis.console === 'undefined') {
    globalThis.console = {
      log(...args) {
        __log__('[log]', ...args);
      },
      warn(...args) {
          __log__('[warn]', ...args);
      },
      error(...args) {
          __log__('[error]', ...args);
      }
    };
  }
})();
    )shim");

    // Load JS File
    // Load and evaluate our Elementary js main file
    auto dspEntryFile = getAssetsDirectory().getChildFile("main.js");
    jsContext.run(dspEntryFile.loadFileAsString().toStdString());

    // Now that the environment is set up, push our current state

    dispatchStateChange();
}

void EffectsPluginProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool EffectsPluginProcessor::isBusesLayoutSupported(const AudioProcessor::BusesLayout &layouts) const
{
    return true;
}

// H. Forward the info into the Elementary Runtime and ask it to do its processing.
// void EffectsPluginProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer & /* midiMessages */)
// {
//     if (runtime == nullptr)
//         return;

//     // Copy the input so that our input and output buffers are distinct
//     scratchBuffer.makeCopyOf(buffer, true);

//     // Process the elementary runtime
//     runtime->process(
//         const_cast<const float **>(scratchBuffer.getArrayOfWritePointers()),
//         getTotalNumInputChannels(),
//         const_cast<float **>(buffer.getArrayOfWritePointers()),
//         buffer.getNumChannels(),
//         buffer.getNumSamples(),
//         nullptr);
// }

// I. Lock-free way of alerting the main thread we have new params and we may need to compute.
void EffectsPluginProcessor::parameterValueChanged(int parameterIndex, float newValue)
{
    // Mark the updated parameter value in the dirty list
    auto &pr = *std::next(paramReadouts.begin(), parameterIndex);

    pr.store({newValue, true});
    triggerAsyncUpdate();
}

void EffectsPluginProcessor::parameterGestureChanged(int, bool)
{
    // Not implemented
}

//==============================================================================

// J. Run on real-time thread. Placeholder code for now.
void EffectsPluginProcessor::handleAsyncUpdate()
{
    auto &params = getParameters();

    // Reduce over the changed parameters to resolve our updated processor state
    for (size_t i = 0; i < paramReadouts.size(); ++i)
    {
        // We atomically exchange an arbitrary value with a dirty flag false, because
        // we know that the next time we exchange, if the dirty flag is still false, the
        // value can be considered arbitrary. Only when we exchange and find the dirty flag
        // true do we consider the value as having been written by the processor since
        // we last looked.
        auto &current = *std::next(paramReadouts.begin(), i);
        auto pr = current.exchange({0.0f, false});

        if (pr.dirty)
        {
            if (auto *pf = dynamic_cast<juce::AudioParameterFloat *>(params[i]))
            {
                auto paramId = pf->paramID.toStdString();
                state.insert_or_assign(paramId, elem::js::Number(pr.value));
            }
        }
    }

    dispatchStateChange();
}

// K. Placehoder
void EffectsPluginProcessor::dispatchStateChange()
{
    const auto *kDispatchScript = R"script(
(function() {
  if (typeof globalThis.__receiveStateChange__ !== 'function')
    return false;

  globalThis.__receiveStateChange__(%);
  return true;
})();
)script";

    // Need the double serialize here to correctly form the string script. The first
    // serialize produces the payload we want, the second serialize ensures we can replace
    // the % character in the above block and produce a valid javascript expression.
    auto expr = juce::String(kDispatchScript).replace("%", elem::js::serialize(elem::js::serialize(state))).toStdString();

    // Next we dispatch to the local engine which will evaluate any necessary JavaScript synchronously
    // here on the main thread
    jsContext.evaluateExpression(expr);
}

//==============================================================================
// L. Whatevrer we save in our state object, it gets stored in the host
void EffectsPluginProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    auto serialized = elem::js::serialize(state);
    destData.replaceAll((void *)serialized.c_str(), serialized.size());
}

void EffectsPluginProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    try
    {
        auto str = std::string(static_cast<const char *>(data), sizeInBytes);
        auto parsed = elem::js::parseJSON(str);

        state = parsed.getObject();
    }
    catch (...)
    {
        // Failed to parse the incoming state, or the state we did parse was not actually
        // an object type. How you handle it is up to you, here we just ignore it
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new EffectsPluginProcessor();
}
