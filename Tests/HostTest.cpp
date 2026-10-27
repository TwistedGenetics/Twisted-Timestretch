/*
    TwistedTimestretchHostTest — loads the BUILT plugin (VST3, or the installed AU
    on macOS) through JUCE's plugin hosting, the same boundary Logic / Ableton use:
      * host save -> new instance -> host restore (project save / duplicate track)
      * the sample travels inside the saved state
      * MIDI in -> audio out across the plugin boundary
    Usage: TwistedTimestretchHostTest /path/to/TwistedTimestretch.vst3
           TwistedTimestretchHostTest TwistedTimestretch.component
*/
#include <JuceHeader.h>
#include <iostream>

static int failures = 0, checks = 0;
static void check (bool ok, const juce::String& what)
{
    ++checks; if (! ok) ++failures;
    std::cout << (ok ? "  [PASS] " : "  [FAIL] ") << what << std::endl;
}

/* Hands raw plugin state to a hosted instance, wrapped the way that host format
   stores it: VST3 = JUCE's <VST3PluginState> XML with an IComponent chunk;
   AU = the ClassInfo property list with a jucePluginState <data> entry. */
static bool injectRawState (juce::AudioPluginInstance& inst, const juce::MemoryBlock& raw)
{
    juce::MemoryBlock current;
    inst.getStateInformation (current);

    if (auto xml = juce::AudioProcessor::getXmlFromBinary (current.getData(), (int) current.getSize()))
    {
        if (xml->hasTagName ("VST3PluginState"))
        {
            juce::XmlElement state ("VST3PluginState");
            state.createNewChildElement ("IComponent")->addTextElement (raw.toBase64Encoding());
            juce::MemoryBlock wrapped;
            juce::AudioProcessor::copyXmlToBinary (state, wrapped);
            inst.setStateInformation (wrapped.getData(), (int) wrapped.getSize());
            return true;
        }
    }

    const auto text = current.toString();
    if (text.contains ("<plist"))
    {
        auto plist = juce::XmlDocument::parse (text);
        if (plist == nullptr) return false;
        auto* dict = plist->getChildByName ("dict");
        if (dict == nullptr) return false;
        for (int i = 0; i < dict->getNumChildElements() - 1; ++i)
        {
            auto* key = dict->getChildElement (i);
            if (key->hasTagName ("key") && key->getAllSubText() == "jucePluginState")
            {
                auto* data = dict->getChildElement (i + 1);
                data->deleteAllChildElements();
                data->addTextElement (juce::Base64::toBase64 (raw.getData(), raw.getSize()));
                const auto out = plist->toString();
                inst.setStateInformation (out.toRawUTF8(), (int) out.getNumBytesAsUTF8());
                return true;
            }
        }
    }
    return false;
}

static juce::AudioProcessorParameter* findParam (juce::AudioPluginInstance& p, const juce::String& name)
{
    for (auto* prm : p.getParameters())
        if (prm->getName (100) == name) return prm;
    return nullptr;
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2) { std::cout << "usage: TwistedTimestretchHostTest plugin.vst3|plugin.component\n"; return 2; }

    juce::AudioPluginFormatManager fm;
    const juce::String path (argv[1]);
   #if JUCE_PLUGINHOST_AU && JUCE_MAC
    if (path.endsWith (".component")) fm.addFormat (new juce::AudioUnitPluginFormat());
    else
   #endif
    fm.addFormat (new juce::VST3PluginFormat());

    juce::OwnedArray<juce::PluginDescription> types;
    auto* format = fm.getFormat (0);
    if (path.endsWith (".component"))
    {
        for (auto& id : format->searchPathsForPlugins ({}, true, false))
            if (id.containsIgnoreCase ("TwistedTimestretch") || id.containsIgnoreCase ("Twts"))
                format->findAllTypesForFile (types, id);
    }
    else
    {
        format->findAllTypesForFile (types, path);
    }
    if (types.isEmpty()) { std::cout << "could not load " << path << "\n"; return 2; }

    auto make = [&]
    {
        juce::String err;
        auto inst = fm.createPluginInstance (*types[0], 48000.0, 512, err);
        if (inst == nullptr) std::cout << "instance error: " << err << "\n";
        else inst->prepareToPlay (48000.0, 512);
        return inst;
    };
    auto pump = [] (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); };
    auto block = [] (juce::AudioPluginInstance& p, juce::MidiBuffer m = {})
    {
        juce::AudioBuffer<float> b (2, 512);
        b.clear();
        p.processBlock (b, m);
        return b.getRMSLevel (0, 0, 512);
    };

    std::cout << "== Host round trip (" << types[0]->name << ", " << types[0]->pluginFormatName << ") ==\n";
    check (types[0]->isInstrument, "host sees an instrument");

    auto a = make();
    if (a == nullptr) return 2;
    auto* stretch = findParam (*a, "Stretch Length");
    auto* cycle = findParam (*a, "Cycle Length");
    auto* pitch = findParam (*a, "Pitch");
    check (stretch && cycle && pitch, "host sees Stretch Length / Cycle Length / Pitch");
    if (! (stretch && cycle && pitch)) return 1;
    std::cout << "  params visible to host: " << a->getParameters().size() << "\n";

    // Load a sample by handing the plugin a state that only carries a file path,
    // exactly as a project saved without the sample embedded would.
    auto wav = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("tt-host-test.wav");
    {
        wav.deleteFile();
        juce::AudioBuffer<float> b (2, 44100);
        for (int i = 0; i < 44100; ++i)
        {
            const float v = 0.5f * std::sin ((float) i * 0.031f) + 0.2f * ((i / 300) % 2 == 0 ? 1.0f : -1.0f);
            b.setSample (0, i, v); b.setSample (1, i, -v);
        }
        juce::WavAudioFormat wf;
        std::unique_ptr<juce::AudioFormatWriter> w (wf.createWriterFor (new juce::FileOutputStream (wav), 44100.0, 2, 16, {}, 0));
        w->writeFromAudioSampleBuffer (b, 0, 44100);
    }
    {
        juce::ValueTree root ("TwistedTimestretch");
        juce::ValueTree s ("Sample");
        s.setProperty ("path", wav.getFullPathName(), nullptr);
        root.appendChild (s, nullptr);
        juce::MemoryBlock mb;
        {
            juce::MemoryOutputStream mos (mb, false);
            root.writeToStream (mos);
        }
        if (! injectRawState (*a, mb))
        {
            std::cout << "  [SKIP] this host format's state wrapper isn't recognised; sample checks skipped\n";
            std::cout << "\n" << (checks - failures) << " / " << checks << " host checks passed\n";
            return failures == 0 ? 0 : 1;
        }
    }
    pump (100);

    juce::MidiBuffer on; on.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 120), 0);
    block (*a);
    check (block (*a, on) > 0.01f, "MIDI note in -> audio out");

    stretch->setValueNotifyingHost (stretch->getValueForText ("150"));
    pitch->setValueNotifyingHost (pitch->getValueForText ("3"));
    block (*a); pump (300);
    std::cout << "  A: stretch=" << stretch->getCurrentValueAsText() << " pitch=" << pitch->getCurrentValueAsText() << "\n";

    juce::MemoryBlock blob;
    a->getStateInformation (blob);
    std::cout << "  state blob: " << (int) blob.getSize() << " bytes\n";
    check (blob.getSize() > 44100 * 4, "the sample is embedded in the saved state");

    wav.deleteFile(); // the project must still open without the original file
    auto b = make();
    b->setStateInformation (blob.getData(), (int) blob.getSize());
    block (*b); pump (200);
    auto* stretchB = findParam (*b, "Stretch Length");
    auto* pitchB = findParam (*b, "Pitch");
    std::cout << "  B: stretch=" << stretchB->getCurrentValueAsText() << " pitch=" << pitchB->getCurrentValueAsText() << "\n";
    check (std::abs (stretchB->getValue() - stretch->getValue()) < 1e-4f, "new instance restores Stretch Length");
    check (std::abs (pitchB->getValue() - pitch->getValue()) < 1e-4f, "new instance restores Pitch");
    check (block (*b, on) > 0.01f, "restored instance plays with the original file deleted");

    std::unique_ptr<juce::AudioProcessorEditor> ed (b->createEditorIfNeeded());
    pump (150);
    check (ed != nullptr, "editor opens");
    b->setStateInformation (blob.getData(), (int) blob.getSize());
    block (*b); pump (150);
    check (block (*b, on) > 0.01f, "restore with the editor open");
    ed.reset();

    a.reset(); b.reset();
    std::cout << "\n" << (checks - failures) << " / " << checks << " host checks passed\n";
    return failures == 0 ? 0 : 1;
}
