#include "PresetManager.h"

namespace bmo
{

namespace { juce::File testDirectory; }

void PresetManager::setDirectoryForTesting (const juce::File& f) { testDirectory = f; }

juce::File suitePresetRoot()
{
    const auto root = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);

   #if JUCE_MAC
    // Where a Mac user expects to find plugin presets.
    return root.getChildFile ("Audio/Presets/LT3 Audio");
   #else
    return root.getChildFile ("LT3 Audio");
   #endif
}

namespace
{
    juce::File productFolder (const juce::String& name)
    {
       #if JUCE_MAC
        return suitePresetRoot().getChildFile (name);
       #else
        return suitePresetRoot().getChildFile (name).getChildFile ("Presets");
       #endif
    }
}

//==============================================================================
PresetManager::PresetManager (PresetTarget& t, PresetInfo i, std::vector<FactoryEntry> factory)
    : target (t), info (std::move (i)), factoryPresets (std::move (factory))
{
    directory().createDirectory();
    migrateLegacy();
}

juce::File PresetManager::directory() const
{
    if (testDirectory != juce::File {})
        return testDirectory.getChildFile (info.folderName);

    return productFolder (info.folderName);
}

void PresetManager::migrateLegacy()
{
    if (info.legacyFolderName.isEmpty() || testDirectory != juce::File {})
        return;

    if (! getUserNames().isEmpty())
        return;

    const auto legacy = productFolder (info.legacyFolderName);

    if (! legacy.isDirectory())
        return;

    for (const auto& f : legacy.findChildFiles (juce::File::findFiles, false,
                                                "*" + info.legacyExtension))
        f.copyFileTo (directory().getChildFile (f.getFileNameWithoutExtension() + info.extension));
}

//==============================================================================
void PresetManager::loadFactory (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) factoryPresets.size()))
        return;

    loading.store (true, std::memory_order_relaxed);

    target.resetToDefaults();

    if (factoryPresets[(size_t) index].apply)
        factoryPresets[(size_t) index].apply();

    currentName = factoryPresets[(size_t) index].name;

    loading.store (false, std::memory_order_relaxed);
    edited.store (false, std::memory_order_relaxed);
}

void PresetManager::loadUser (const juce::String& name)
{
    loadFile (directory().getChildFile (name + extension()));
}

bool PresetManager::loadFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    auto xml = juce::XmlDocument::parse (file);

    if (xml == nullptr)
        return false;

    loading.store (true, std::memory_order_relaxed);

    // restoreState defaults first, so a preset written by an older version
    // cannot leave parameters it never knew about sitting wherever they were.
    const auto ok = target.restoreState (*xml);

    if (ok)
        currentName = file.getFileNameWithoutExtension();

    loading.store (false, std::memory_order_relaxed);

    if (ok)
        edited.store (false, std::memory_order_relaxed);

    return ok;
}

void PresetManager::step (int delta)
{
    const auto users = getUserNames();
    const auto factoryCount = (int) factoryPresets.size();
    const auto total = factoryCount + users.size();

    if (total == 0)
        return;

    // Where we are now, or just before the start if the current name is not in
    // the list -- which is the case after Save As under a new name.
    int index = -1;

    for (int i = 0; i < factoryCount; ++i)
        if (currentName == factoryPresets[(size_t) i].name)
            index = i;

    if (index < 0)
        for (int i = 0; i < users.size(); ++i)
            if (currentName == users[i])
                index = factoryCount + i;

    index = (index + delta + total) % total;

    if (index < factoryCount) loadFactory (index);
    else                      loadUser (users[index - factoryCount]);
}

//==============================================================================
juce::StringArray PresetManager::getUserNames() const
{
    juce::StringArray names;

    for (const auto& f : directory().findChildFiles (juce::File::findFiles, false,
                                                     "*" + extension()))
        names.add (f.getFileNameWithoutExtension());

    names.sort (true);
    return names;
}

bool PresetManager::saveUser (const juce::String& name)
{
    const auto trimmed = name.trim();

    if (trimmed.isEmpty())
        return false;

    // Keep it to something that is a legal filename everywhere, since these
    // get sent between machines.
    const auto safe = juce::File::createLegalFileName (trimmed);
    const auto file = directory().getChildFile (safe + extension());

    if (! exportTo (file))
        return false;

    currentName = safe;
    edited.store (false, std::memory_order_relaxed);
    return true;
}

bool PresetManager::exportTo (juce::File destination)
{
    if (destination.getFileExtension().isEmpty())
        destination = destination.withFileExtension (extension());

    destination.getParentDirectory().createDirectory();

    auto xml = target.captureState();
    return xml != nullptr && xml->writeTo (destination);
}

bool PresetManager::importFrom (const juce::File& source)
{
    if (! source.existsAsFile())
        return false;

    // Copy it into the folder so it joins the list, then load it. Importing
    // something that only works until the file moves is not importing.
    const auto destination = directory().getChildFile (source.getFileName())
                                        .withFileExtension (extension());

    if (! source.copyFileTo (destination))
        return false;

    return loadFile (destination);
}

bool PresetManager::deleteUser (const juce::String& name)
{
    return directory().getChildFile (name + extension()).deleteFile();
}

} // namespace bmo
