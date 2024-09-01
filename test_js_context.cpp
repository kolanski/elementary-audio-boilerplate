#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <functional>
#include <atomic>
#include "choc/javascript/choc_javascript_QuickJS.h" // Adjust the include path as necessary

typedef std::chrono::duration<int, std::milli> milliseconds;

class JTimer
{
public:
    JTimer()
    {
        std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
        m_JTimerStart = std::chrono::time_point_cast<std::chrono::milliseconds>(now).time_since_epoch();
    }

    int GetTicks()
    {
        std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
        milliseconds ticks = std::chrono::time_point_cast<std::chrono::milliseconds>(now).time_since_epoch();

        return ticks.count() - m_JTimerStart.count();
    }

private:
    milliseconds m_JTimerStart;
};

class JSContextManager
{
public:
    choc::javascript::Context jsContext;
    JSContextManager() : exitFlag(false), cancelToken(true)
    {
        jsContext = choc::javascript::createQuickJSContext();
        if (!jsContext)
        {
            throw std::runtime_error("Failed to create JavaScript context.");
        }

        // Register functions
        registerFunctions();

        // Define a basic console object
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
    }

    void loadAndExecuteScript(const std::string &scriptPath)
    {
        std::ifstream scriptFile(scriptPath);
        if (scriptFile.is_open())
        {
            std::string script((std::istreambuf_iterator<char>(scriptFile)), std::istreambuf_iterator<char>());
            jsContext.evaluateExpression(script);
            std::cout << "Script executed successfully." << std::endl;
        }
        else
        {
            std::cerr << "Failed to open script file: " << scriptPath << std::endl;
        }
    }

    void start()
    {
        // Create an anonymous function to print "hello"
        auto printHello = [this]()
        {
            this->jsContext.run("fact();");
            std::cout << "hello" << std::endl;
        };

        // Use setInterval to call the anonymous function every 2 seconds
        // setInterval(printHello, 2000, cancelToken);

        // Thread to handle user input
        std::thread inputThread([this]()
                                {
            std::string userInput;
            while (true) {
                std::cout << "Enter 'exit' to quit: ";
                std::getline(std::cin, userInput);
                if (userInput == "exit") {
                    exitFlag.store(true);
                    break;
                }
            } });

        // Main loop to keep the program running
        while (!exitFlag.load())
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Wait for the input thread to finish
        inputThread.join();

        // Stop the interval
        cancelToken.store(false);
    }

private:
    void registerFunctions()
    {
        jsContext.registerFunction("__getSampleRate__", [](choc::javascript::ArgumentList args)
                                   { return choc::value::Value(0); });

        jsContext.registerFunction("__postNativeMessage__", [](choc::javascript::ArgumentList args)
                                   { return choc::value::Value(); });

        jsContext.registerFunction("__log__", [](choc::javascript::ArgumentList args)
                                   {
            // for (size_t i = 0; i < args.numArgs; ++i) {
            //     std::cout << "Type: " << args[i]->getType().getDescription() << ", Value: " << choc::json::toString(*args[i], true) << std::endl;
            // }
            std::cout << "[LOG]:" <<choc::json::toString(*args[1], true);
            return choc::value::Value(); });

        jsContext.registerFunction("setInterval", [this](choc::javascript::ArgumentList args)
                                   {
            std::cout << "Tried to Set interval" <<std::endl;
            if (args.numArgs < 2) {
                return choc::value::Value();
            }

            for (size_t i = 0; i < args.numArgs; ++i) {
                std::cout << "Type: " << args[i]->getType().getDescription() << ", Value: " << choc::json::toString(*args[i], true) << std::endl;
            }
            std::string funcName = std::string(args[0]->getString());
            std::atomic_bool* cancelToken = new std::atomic_bool(true);
            auto func = [this, cancelToken, funcName]() {
                std::string formattedString = funcName + "();";
                this->jsContext.run(formattedString); 
            std::cout << "exec interval " << formattedString <<std::endl;};
            setInterval(func, args[1]->getFloat64(), std::ref(*cancelToken));

            return choc::value::Value(reinterpret_cast<int64_t>(cancelToken)); });

        jsContext.registerFunction("clearInterval", [](choc::javascript::ArgumentList args)
                                   {
            if (args.numArgs < 1 || !args[0]->isInt()) {
                return choc::value::Value();
            }

            auto cancelToken = reinterpret_cast<std::atomic_bool*>(args[0]->getInt64());
            if (cancelToken) {
                cancelToken->store(false);
                delete cancelToken;
            }

            return choc::value::Value(); });
    }

    void setInterval(std::function<void()> callback, int intervalMs, std::atomic_bool &cancelToken)
    {
        std::thread([callback, intervalMs, &cancelToken]()
                    {
            JTimer timer;
            while (cancelToken.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
                if (cancelToken.load()) {
                    callback();
                }
            } })
            .detach();
    }

    std::atomic_bool exitFlag;
    std::atomic_bool cancelToken;
};

int main()
{
    try
    {
        JSContextManager manager;
        manager.loadAndExecuteScript("./js/dist/main.js");
        manager.start();
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}