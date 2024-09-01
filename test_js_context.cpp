#include <iostream>
#include <fstream>
#include "choc/javascript/choc_javascript_QuickJS.h" // Adjust the include path as necessary

int main()
{
    // Initialize the JavaScript context
    auto jsContext = choc::javascript::createQuickJSContext();

    // Log the output to the console
    std::cout << "JavaScript context created: " << jsContext << std::endl;
    jsContext.registerFunction("__getSampleRate__", [](choc::javascript::ArgumentList args)
                               { return choc::value::Value(0); });

    jsContext.registerFunction("__postNativeMessage__", [](choc::javascript::ArgumentList args)
                               {
        return choc::value::Value(); });

    jsContext.registerFunction("__log__", [](choc::javascript::ArgumentList args)
                               {
        for (size_t i = 0; i < args.numArgs; ++i) {
            std::cout << (choc::json::toString(*args[i], true)) << std::endl;;
        }

        return choc::value::Value(); });
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
    // Load and execute the main script
    std::string scriptPath = "js/dist/main.js";
    std::ifstream scriptFile(scriptPath);
    if (scriptFile.is_open())
    {
        std::string script((std::istreambuf_iterator<char>(scriptFile)), std::istreambuf_iterator<char>());

        // Evaluate the main script
        if (jsContext)
        {
            jsContext.evaluateExpression(script);
            std::cout << "Script executed successfully." << std::endl;
        }
        else
        {
            std::cerr << "Failed to create JavaScript context." << std::endl;
        }
    }
    else
    {
        std::cerr << "Failed to open script file: " << scriptPath << std::endl;
    }

    return 0;
}