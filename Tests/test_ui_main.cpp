#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>
#include <juce_gui_basics/juce_gui_basics.h>

// Native embedding needs an actual host application, not only a message-thread
// initializer. In particular JUCE's XEmbed host makes an initial property query
// before the client window exists; JUCEApplication owns the platform event/error
// handling for that lifecycle, just as AudioPluginHost and pluginval do.
class UITestApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Suppressor UI tests"; }
    const juce::String getApplicationVersion() override { return "1.0"; }
    bool moreThanOneInstanceAllowed() override { return true; }

    void initialise (const juce::String&) override
    {
        juce::MessageManager::callAsync ([this]
        {
            auto arguments = getCommandLineParameterArray();
            arguments.insert (0, getApplicationName());
            std::vector<const char*> argv;
            for (const auto& argument : arguments) argv.push_back (argument.toRawUTF8());
            doctest::Context context (static_cast<int> (argv.size()), argv.data());
            setApplicationReturnValue (context.run());
            quit();
        });
    }

    void shutdown() override {}
};

START_JUCE_APPLICATION (UITestApplication)
