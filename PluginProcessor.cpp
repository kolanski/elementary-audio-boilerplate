#include <choc_javascript_QuickJS.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"
#include "choc/gui/choc_WebView.h"

//==============================================================================
// A quick helper for locating bundled asset files
juce::File getAssetsDirectory() {
#if JUCE_MAC
  auto assetsDir = juce::File::getSpecialLocation(
                       juce::File::SpecialLocationType::currentApplicationFile)
                       .getChildFile("Contents/Resources/dist");
#elif JUCE_WINDOWS
  auto assetsDir =
      juce::File::getSpecialLocation(
          juce::File::SpecialLocationType::
              currentExecutableFile)  // Plugin.vst3/Contents/<arch>/Plugin.vst3
          .getParentDirectory()       // Plugin.vst3/Contents/<arch>/
          .getParentDirectory()       // Plugin.vst3/Contents/
          .getChildFile("Resources/dist");
#else
#error "We only support Mac and Windows here yet."
#endif

  jassert(assetsDir.isDirectory());
  return assetsDir;
}

//==============================================================================
class TaskQueue {
 public:
  void postTask(std::function<void()> task) {
    std::lock_guard<std::mutex> lock(mutex);
    tasks.push(task);
    condition.notify_one();
  }

  void processTasks() {
    std::queue<std::function<void()>> tasksToProcess;
    {
      std::lock_guard<std::mutex> lock(mutex);
      std::swap(tasks, tasksToProcess);
    }

    while (!tasksToProcess.empty()) {
      tasksToProcess.front()();
      tasksToProcess.pop();
    }
  }

  void waitForTasks() {
    std::unique_lock<std::mutex> lock(mutex);
    condition.wait(lock, [this] { return !tasks.empty(); });
  }

 private:
  std::queue<std::function<void()>> tasks;
  std::mutex mutex;
  std::condition_variable condition;
};
inline TaskQueue taskQueue;
EffectsPluginProcessor::EffectsPluginProcessor()
    : AudioProcessor(
          BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}

EffectsPluginProcessor::~EffectsPluginProcessor() {
  for (auto &p : getParameters()) {
    p->removeListener(this);
  }
}
std::vector<std::string> delayedMessages;
choc::javascript::Context *jsRefContext;

class AudioPluginAudioProcessorEditor : public juce::AudioProcessorEditor {
 public:
  std::unique_ptr<choc::ui::WebView> chocWebView;
  explicit AudioPluginAudioProcessorEditor(EffectsPluginProcessor &processor)
      : juce::AudioProcessorEditor(&processor), processorRef(processor) {
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
        <title>CHOC WebView</title>
    </head>
    <body>
        <h1>Welcome to CHOC WebView</h1>
        <p>This is a static HTML content loaded directly into the WebView.</p>
        
        <!-- Volume Slider -->
        <label for="volume">Volume:</label>
        <input type="range" id="volume" name="volume" min="0" max="1" step="0.01" value="0.01">
        
        <script>
            // Function to handle volume changes
            document.getElementById('volume').addEventListener('input', function(event) {
                const volume = event.target.value;
                
                if (typeof onVolumeChange === 'function') {
                    try {
                        onVolumeChange(volume)
                            .then(() => {console.log('Volume change handled');})
                            .catch(err => console.error('Error in onVolumeChange:', err));
                    } catch (err) {
                        console.error('Error invoking onVolumeChange:', err);
                    }
                }

                if (typeof sendMessageToNative === 'function') {
                    try {
                        sendMessageToNative(JSON.stringify({ type: 'volumeChange', volume: volume }))
                            .catch(err => console.error('Error sending message to native:', err));
                    } catch (err) {
                        console.error('Error in sendMessageToNative:', err);
                    }
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
  void resized() override {
    // Set bounds for the WebView holder
    juceWebViewHolder->setBounds(getLocalBounds());
  }

  void initializeWebViewBindings() {
    chocWebView->bind(
        "onVolumeChange",
        [this](const choc::value::ValueView &args) -> choc::value::Value {
          if (args.size() > 0) {
            std::string volumeStr = args[0].getString().data();
            taskQueue.postTask([this, volumeStr]() {
              processorRef.jsContext.run("update_volume(" + volumeStr + ");");
            });
          }
          return choc::value::Value();
        });

    //setInterval NativeExampleUsage
    // std::atomic_bool *cancelToken1 = new std::atomic_bool(true);
    // processorRef.cancelTokens.push_back(cancelToken1);
    // auto test = [this]() {
    //   this->chocWebView->evaluateJavascript("console.log('opa interval')");
    // };
    // processorRef.timerManager.setInterval(test, 2000, *cancelToken1);
  }

 private:
  EffectsPluginProcessor &processorRef;

  std::unique_ptr<juce::Component> juceWebViewHolder;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPluginAudioProcessorEditor)
};
AudioPluginAudioProcessorEditor *globvar;

//==============================================================================
// B. This is how you make the UIX.
//    This generic editor is what JUICe provides.
//    This is an area where an artist could make cooler UIX.
//    Create this Blog Post => How do I use web technology to use an User
//    Interface
juce::AudioProcessorEditor *EffectsPluginProcessor::createEditor() {
  auto *editor = new AudioPluginAudioProcessorEditor(*this);
  editor->initializeWebViewBindings();
  globvar = editor;
  return editor;
}
void EffectsPluginProcessor::evalJsContext(const std::string &expr) {
  jsRefContext->evaluateExpression(juce::String(expr).toStdString());
  jsContext.evaluateExpression(juce::String(expr).toStdString());
}

void EffectsPluginProcessor::handleWebViewMessage(const std::string &message) {
}

void EffectsPluginProcessor::sendMessageToWebView(const std::string &message) {
   globvar->chocWebView->evaluateJavascript("window.receiveMessageFromNative(" +
                                           message + ");");
}

bool EffectsPluginProcessor::hasEditor() const { return true; }

//==============================================================================
const juce::String EffectsPluginProcessor::getName() const {
  return JucePlugin_Name;
}

bool EffectsPluginProcessor::acceptsMidi() const { return false; }

bool EffectsPluginProcessor::producesMidi() const { return false; }

bool EffectsPluginProcessor::isMidiEffect() const { return false; }

double EffectsPluginProcessor::getTailLengthSeconds() const { return 0.0; }

//==============================================================================
// C. This is wher eyou handle presets.
int EffectsPluginProcessor::getNumPrograms() {
  return 1;  // NB: some hosts don't cope very well if you tell them there are 0
             // programs, so this should be at least 1, even if you're not
             // really implementing programs.
}

int EffectsPluginProcessor::getCurrentProgram() { return 0; }

void EffectsPluginProcessor::setCurrentProgram(int /* index */) {}
const juce::String EffectsPluginProcessor::getProgramName(int /* index */) {
  return {};
}
void EffectsPluginProcessor::changeProgramName(
    int /* index */, const juce::String & /* newName */) {}

//==============================================================================

// D. HOST knows what it needs ot know.
//    We know the sample rate, the block size for every callback.
//    This is the moment before we render audio
void push_to_delayed(std::string message) {
  delayedMessages.push_back(
      "console.log('from jsContext:" + std::string(message) + "');");
}
void EffectsPluginProcessor::prepareToPlay(double sampleRate,
                                           int samplesPerBlock) {
  std::atomic_bool exitFlag;
  std::atomic_bool cancelToken;
  //Processing queue events in main thread
  auto processTasks = [this]() {
    while (true) {
      taskQueue.waitForTasks();
      taskQueue.processTasks();
    }
  };

  std::thread taskProcessor(processTasks);
  taskProcessor.detach();

  // E. Make the Elementary Runtime
  runtime = std::make_unique<elem::Runtime<float>>(sampleRate, samplesPerBlock);
  jsContext = choc::javascript::createQuickJSContext();
  jsRefContext = &jsContext;

  // F. If a user changes tehir sample rate, we want to re-run our JavaScript
  // Install some native interop functions in our JavaScript environment

  jsContext.registerFunction("__getSampleRate__",
                             [this](choc::javascript::ArgumentList args) {
                               return choc::value::Value(getSampleRate());
                             });

  jsContext.registerFunction(
      "__postNativeMessage__", [this](choc::javascript::ArgumentList args) {
        runtime->applyInstructions(elem::js::parseJSON(args[0]->toString()));
        return choc::value::Value();
      });

  jsContext.registerFunction(
      "__log__", [this](choc::javascript::ArgumentList args) {
        for (size_t i = 0; i < args.numArgs; ++i) {
          // DBG(choc::json::toString(*args[i], true));
          // this->handleWebViewMessage(choc::json::toString(*args[i], true));
          // this->handleWebViewMessage("console.log('wow');");
        }

        if (globvar != nullptr) {
          if (!delayedMessages.empty()) {
            for (const auto &message : delayedMessages) {
              globvar->chocWebView->evaluateJavascript(message);
            }
            // Clear the delayed messages after processing
            delayedMessages.clear();
          }
          globvar->chocWebView->evaluateJavascript(
              "console.log('from jsContext LOG:" +
              std::string(args[1]->toString()) +
              "');");  // + std::string(args[1]->toString()) + "');");
        } else {
          delayedMessages.push_back("console.log('from jsContext LOG:" +
                                    std::string(args[1]->toString()) + "');");
        }

        return choc::value::Value();
      });
      
  jsContext.registerFunction(
      "setInterval", [this](choc::javascript::ArgumentList args) {
        if (args.numArgs < 2) {
          return choc::value::Value();
        }

        for (size_t i = 0; i < args.numArgs; ++i) {
          std::cout << "Type: " << args[i]->getType().getDescription()
                    << ", Value: " << choc::json::toString(*args[i], true)
                    << std::endl;
        }
        std::string funcName = args[0]->toString();
        std::atomic_bool *cancelToken = new std::atomic_bool(true);
        auto func = [this, cancelToken, funcName]() {
          std::string formattedString = funcName + "();";
          // std::cout << "Executing JS function: " << formattedString <<
          // std::endl; push_to_delayed("tried Executing JS function in
          // interval:"+funcName);
          this->jsContext.run(formattedString);
        };
        this->timerManager.setInterval(func, args[1]->getFloat64(),
                                       std::ref(*cancelToken));

        return choc::value::Value(reinterpret_cast<int64_t>(cancelToken));
      });
  // G.
  // A simple shim to write various console operations to our native __log__
  // handler
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

void EffectsPluginProcessor::releaseResources() {
  // When playback stops, you can use this as an opportunity to free up any
  // spare memory, etc.
}

bool EffectsPluginProcessor::isBusesLayoutSupported(
    const AudioProcessor::BusesLayout &layouts) const {
  return true;
}

// H. Forward the info into the Elementary Runtime and ask it to do its
// processing.
void EffectsPluginProcessor::processBlock(
    juce::AudioBuffer<float> &buffer, juce::MidiBuffer & /* midiMessages */) {
  if (runtime == nullptr) return;

  // Copy the input so that our input and output buffers are distinct
  scratchBuffer.makeCopyOf(buffer, true);

  // Process the elementary runtime
  runtime->process(
      const_cast<const float **>(scratchBuffer.getArrayOfWritePointers()),
      getTotalNumInputChannels(),
      const_cast<float **>(buffer.getArrayOfWritePointers()),
      buffer.getNumChannels(), buffer.getNumSamples(), nullptr);
}

// I. Lock-free way of alerting the main thread we have new params and we may
// need to compute.
void EffectsPluginProcessor::parameterValueChanged(int parameterIndex,
                                                   float newValue) {
  // Mark the updated parameter value in the dirty list
  auto &pr = *std::next(paramReadouts.begin(), parameterIndex);

  pr.store({newValue, true});
  triggerAsyncUpdate();
}

void EffectsPluginProcessor::parameterGestureChanged(int, bool) {
  // Not implemented
}

//==============================================================================

// J. Run on real-time thread. Placeholder code for now.
void EffectsPluginProcessor::handleAsyncUpdate() {
  auto &params = getParameters();

  // Reduce over the changed parameters to resolve our updated processor state
  for (size_t i = 0; i < paramReadouts.size(); ++i) {
    // We atomically exchange an arbitrary value with a dirty flag false,
    // because we know that the next time we exchange, if the dirty flag is
    // still false, the value can be considered arbitrary. Only when we exchange
    // and find the dirty flag true do we consider the value as having been
    // written by the processor since we last looked.
    auto &current = *std::next(paramReadouts.begin(), i);
    auto pr = current.exchange({0.0f, false});

    if (pr.dirty) {
      if (auto *pf = dynamic_cast<juce::AudioParameterFloat *>(params[i])) {
        auto paramId = pf->paramID.toStdString();
        state.insert_or_assign(paramId, elem::js::Number(pr.value));
      }
    }
  }

  dispatchStateChange();
}

// K. Placehoder
void EffectsPluginProcessor::dispatchStateChange() {
  const auto *kDispatchScript = R"script(
(function() {
  if (typeof globalThis.__receiveStateChange__ !== 'function')
    return false;

  globalThis.__receiveStateChange__(%);
  return true;
})();
)script";

  // Need the double serialize here to correctly form the string script. The
  // first serialize produces the payload we want, the second serialize ensures
  // we can replace the % character in the above block and produce a valid
  // javascript expression.
  auto expr = juce::String(kDispatchScript)
                  .replace("%", elem::js::serialize(elem::js::serialize(state)))
                  .toStdString();

  // Next we dispatch to the local engine which will evaluate any necessary
  // JavaScript synchronously here on the main thread
  jsContext.evaluateExpression(expr);
}

//==============================================================================
// L. Whatevrer we save in our state object, it gets stored in the host
void EffectsPluginProcessor::getStateInformation(juce::MemoryBlock &destData) {
  auto serialized = elem::js::serialize(state);
  destData.replaceAll((void *)serialized.c_str(), serialized.size());
}

void EffectsPluginProcessor::setStateInformation(const void *data,
                                                 int sizeInBytes) {
  try {
    auto str = std::string(static_cast<const char *>(data), sizeInBytes);
    auto parsed = elem::js::parseJSON(str);

    state = parsed.getObject();
  } catch (...) {
    // Failed to parse the incoming state, or the state we did parse was not
    // actually an object type. How you handle it is up to you, here we just
    // ignore it
  }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new EffectsPluginProcessor();
}
