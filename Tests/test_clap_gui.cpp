#include <doctest/doctest.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <clap/clap.h>

namespace
{
struct ClapHost
{
    juce::DynamicLibrary library;
    const clap_plugin_entry_t* entry = nullptr;
    const clap_plugin_t* plugin = nullptr;
    const clap_plugin_gui_t* gui = nullptr;
    bool initialised = false, created = false;
    std::atomic<bool> callbackRequested { false };
    clap_host_t host { CLAP_VERSION, this, "Suppressor UI tests", "Suppressor Audio",
                      "https://github.com/affinedsp/suppressor", "1.0",
                      [] (const clap_host_t*, const char*) -> const void* { return nullptr; },
                      [] (const clap_host_t*) {}, [] (const clap_host_t*) {},
                      [] (const clap_host_t* h) { static_cast<ClapHost*> (h->host_data)->callbackRequested = true; } };
    void pump()
    {
        if (callbackRequested.exchange (false) && plugin != nullptr)
        {
            plugin->on_main_thread (plugin);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
    }
    ~ClapHost()
    {
        if (created) gui->destroy (plugin);
        if (plugin != nullptr) plugin->destroy (plugin);
        if (initialised) entry->deinit();
    }
};
}

TEST_CASE ("CLAP embeds, hides, reopens and destroys the actual native editor")
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("SUPPRESSOR_CLAP_PATH", {});
    if (path.isEmpty()) return;
    juce::ScopedJuceInitialiser_GUI initialiser;
    juce::Component parent;
    parent.setBounds (80, 80, 640, 460);
    parent.addToDesktop (0);
    parent.setVisible (true);
    ClapHost h;
    auto binary = juce::File (path);
   #if JUCE_MAC
    binary = binary.getChildFile ("Contents/MacOS/suppressor");
   #endif
    REQUIRE (h.library.open (binary.getFullPathName()));
    h.entry = static_cast<const clap_plugin_entry_t*> (h.library.getFunction ("clap_entry"));
    REQUIRE (h.entry != nullptr);
    h.initialised = h.entry->init (path.toRawUTF8());
    REQUIRE (h.initialised);
    auto* factory = static_cast<const clap_plugin_factory_t*> (h.entry->get_factory (CLAP_PLUGIN_FACTORY_ID));
    REQUIRE (factory != nullptr);
    REQUIRE (factory->get_plugin_count (factory) == 1);
    h.plugin = factory->create_plugin (factory, &h.host, factory->get_plugin_descriptor (factory, 0)->id);
    REQUIRE (h.plugin != nullptr);
    REQUIRE (h.plugin->init (h.plugin));
    h.gui = static_cast<const clap_plugin_gui_t*> (h.plugin->get_extension (h.plugin, CLAP_EXT_GUI));
    REQUIRE (h.gui != nullptr);
    clap_window_t window {};
   #if JUCE_MAC
    window.api = CLAP_WINDOW_API_COCOA;
    window.cocoa = parent.getPeer()->getNativeHandle();
   #elif JUCE_WINDOWS
    window.api = CLAP_WINDOW_API_WIN32;
    window.win32 = parent.getPeer()->getNativeHandle();
   #else
    window.api = CLAP_WINDOW_API_X11;
    window.x11 = reinterpret_cast<unsigned long> (parent.getPeer()->getNativeHandle());
   #endif
    REQUIRE (h.gui->is_api_supported (h.plugin, window.api, false));
    for (int reopen = 0; reopen < 3; ++reopen)
    {
        h.created = h.gui->create (h.plugin, window.api, false);
        REQUIRE (h.created);
        uint32_t width = 0, height = 0;
        REQUIRE (h.gui->get_size (h.plugin, &width, &height));
        CHECK (width == 640);
        CHECK (height == 460);
        CHECK_FALSE (h.gui->can_resize (h.plugin));
        REQUIRE (h.gui->set_parent (h.plugin, &window));
        REQUIRE (h.gui->show (h.plugin));
        h.pump();
        // The pinned wrapper returns false from hide(). Embedded hosts can
        // hide their containing window instead; exercise that fallback too.
        h.gui->hide (h.plugin);
        parent.setVisible (false);
        h.pump();
        parent.setVisible (true);
        REQUIRE (h.gui->show (h.plugin));
        h.pump();
        h.gui->destroy (h.plugin);
        h.created = false;
        h.pump();
    }
}
