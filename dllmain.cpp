#define _CRT_SECURE_NO_WARNINGS

#ifdef _WIN32
    #define MOD_API __declspec(dllexport)
#elif defined(__linux__)
    #define MOD_API __attribute__((visibility("default")))
#else
    #define MOD_API
#endif

#define SAVE_SLOT_MAX 999
#define MOD_NAME "PracticeMod"

#include <map>
#include <fstream>
#include <string>
#include <iostream>
#include <filesystem>
#include <vector>
#include <sstream>
#include <iterator>
#include "MinaModAPI.h"
#include "MinaModEnums.h"
#include <cstdarg>



MinaModAPI* Mina = nullptr;
char* copiedSave = nullptr;
int saveSlot = 1;

struct DebugMessage
{
    std::string msg;
    float opacity;
};

struct ModConfig
{
    bool enableLogging = false;
    std::vector<std::string> SaveBind;
    std::vector<std::string> LoadBind;
    std::vector<std::string> SlotDownBind;
    std::vector<std::string> SlotUpBind;
};

struct ResolvedKey {
    int enumVal = -1;
    // true = keyboard input, false = controller input
    bool isKey = false; 
};

struct ComboState {
    bool locked = false;
    std::vector<ResolvedKey> resolvedKeys;
    bool keysCached = false;
};

static std::map<std::string, ComboState> g_ComboStates;

ModConfig ModConfigs;
std::vector<DebugMessage> DebugMessages;

void GameInit(void*);
void CustomUpdate(void*);
bool LoadSaveFromFile(const std::string& filePath);
bool SaveCurrentSlotToFile(const std::string& filePath);
bool IsActionBeingActivated(const std::vector<std::string>& binds);
static void ResolveAllKeyCombos();
static void ResolveBinds(const std::vector<std::string>& binds);
static ResolvedKey ResolveBindKey(const std::string& keyName);
static std::vector<std::string> SplitAndTrim(const std::string& str, char delim);
static bool ExtractSaveSlotFromGameSave(const std::string& fullSaveData, int slotIndex, std::string& outSlotData);
static std::string GetEnvOrDefault(const char* var, const std::string& fallback);
static std::filesystem::path GetSlotFilePath(int slotIndex);
static void ParseSlotDisplayName(const std::filesystem::path& filePath, std::string& slotNum, std::string& displayName);
static void ModLog(const char* format, ...);
static void LoadOrCreateConfig();
static std::filesystem::path GetConfigPath();
static std::filesystem::path GetGameBaseDir();

extern "C"
MOD_API
void MinaMod_Init(MinaModAPI* mm)
{
    Mina = mm;
    LoadOrCreateConfig();
    Mina->InstallHook("GameInit", 0, GameInit);
}

void GameInit(void*) {
    Mina->UpdateQueueAdd(Mina->GameGetWorldUpdateQueue(), CustomUpdate, nullptr);

    ycFileRefBase* fileRef = Mina->CreateFileRef("debugdraw.font.yc");
    bool fontLoaded = Mina->FileRefWaitForLoaded(fileRef);
    Mina->Assert(fontLoaded, "Could not find debugdraw.font.yc");
}

void CustomUpdate(void*)
{
    static bool slotChanged = false;

    if (IsActionBeingActivated(ModConfigs.SaveBind))
    {
        ModLog("Exporting save to file.");
        auto slotPath = GetSlotFilePath(saveSlot);
        bool ok = SaveCurrentSlotToFile(slotPath.string());
        if (!ok) {
            ModLog("Save export failed.");
        }
        else {
            Mina->SoundPlay("bike_bell");
            std::string numStr = std::to_string(saveSlot);
            while (numStr.length() < 3) numStr = "0" + numStr;
            DebugMessages.insert(DebugMessages.begin(), { std::string("Saving onto save slot " + numStr), 1.0f});
        }
    }
    if (IsActionBeingActivated(ModConfigs.LoadBind))
    {
        ModLog("Importing save from file.");
        auto slotPath = GetSlotFilePath(saveSlot);
        bool ok = LoadSaveFromFile(slotPath.string());
        if (ok) {
            Mina->PlayerRestoreFromSave();
            Mina->StartActiveSaveSlot();
        }
        else {
            ModLog("Save import failed.");
        }
    }
    if (IsActionBeingActivated(ModConfigs.SlotDownBind))
    {
        if (saveSlot > 1) {
            saveSlot -= 1;
            Mina->SoundPlay("window_close");
            slotChanged = true;
        }
    }
    if (IsActionBeingActivated(ModConfigs.SlotUpBind))
    {
        if (saveSlot < SAVE_SLOT_MAX) {
            saveSlot += 1;
            Mina->SoundPlay("window_open");
            slotChanged = true;
        }

    }

    if (slotChanged) {
        auto slotPath = GetSlotFilePath(saveSlot);
        std::string slotNum, displayName;
        ParseSlotDisplayName(slotPath, slotNum, displayName);
        DebugMessages.insert(DebugMessages.begin(), { "Switching to save slot " + displayName, 1.0f});
        slotChanged = false;
    }

    // update on-screen logs
    ycDrawUtil* dd = Mina->GetDebugDraw("Hud");
    float y = 0.0f;
    for (auto i = DebugMessages.begin(); i != DebugMessages.end(); /**/)
    {
        uint8_t opacity = uint8_t(255.0f * i->opacity);
        Mina->DebugDrawText(dd, i->msg.c_str(), MM_Color{0,   0,   0, opacity}, MM_Vec3{3, y - 3, 0}, 30);
        Mina->DebugDrawText(dd, i->msg.c_str(), MM_Color{ 255, 255, 255, opacity }, MM_Vec3{0, y  , 0}, 30);
        i->opacity -= 0.005f;
        y -= 40;
        if (i->opacity <= 0.0f) { i = DebugMessages.erase(i); }
        else { ++i; }
    }

}


bool LoadSaveFromFile(const std::string& filePath)
{
    std::filesystem::path finalFilePath = std::filesystem::path(filePath).lexically_normal();

    if (!std::filesystem::exists(finalFilePath)) {
        ModLog("File not found: %s", finalFilePath.string().c_str());
        return false;
    }

    std::ifstream file(finalFilePath, std::ios::binary);
    if (!file) {
        ModLog("Failed to open file.");
        return false;
    }

    std::string saveData((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    file.close();

    bool success = Mina->SetActiveSaveSlotContents(saveData.c_str());
    if (!success) {
        ModLog("API rejected save data.");
        return false;
    }
    ModLog("Loaded file successfully.");
    return true;
}

bool SaveCurrentSlotToFile(const std::string& filePath)
{
    std::filesystem::path finalFilePath = std::filesystem::path(filePath).lexically_normal();
    auto parent = finalFilePath.parent_path();

    if (!parent.empty() && !std::filesystem::exists(parent)) {
        if (!std::filesystem::create_directories(parent)) {
            ModLog("Failed to create directory.");
            return false;
        }
    }

    std::string gameSavePath = (GetGameBaseDir() / "saveData.yc").string();
    std::ifstream gameFile(gameSavePath, std::ios::binary);
    if (!gameFile) {
        ModLog("Failed to open game save file.");
        return false;
    }
    std::string gameSaveData((std::istreambuf_iterator<char>(gameFile)), std::istreambuf_iterator<char>());
    gameFile.close();

    int activeSlot = Mina->GetActiveSaveSlot();
    std::string extractedSlot;
    if (!ExtractSaveSlotFromGameSave(gameSaveData, activeSlot, extractedSlot)) {
        ModLog("Failed to extract slot %d from saveData.yc", activeSlot);
        return false;
    }

    std::string saveData = "[YCD Version: 1]\nSaveSlot\n" + extractedSlot;

    std::ofstream file(finalFilePath, std::ios::out | std::ios::trunc);
    if (!file) {
        ModLog("Failed to open file for writing.");
        return false;
    }

    file.write(saveData.data(), saveData.size());
    file.close();

    if (!file.good()) {
        ModLog("Write failed.");
        return false;
    }

    ModLog("Saved file successfully.");
    return true;
}

static std::filesystem::path GetSlotFilePath(int slotIndex)
{
    std::filesystem::path base = GetGameBaseDir();
    std::filesystem::path savesDir = base / "mods" / MOD_NAME / "saves";

    if (!std::filesystem::exists(savesDir))
    {
        std::string numStr = std::to_string(slotIndex);
        while (numStr.length() < 3) numStr = "0" + numStr;
        return savesDir / (numStr + ".sv");
    }

    std::string numStr = std::to_string(slotIndex);
    while (numStr.length() < 3) numStr = "0" + numStr;

    // look for custom named file first: 000_Name.sv
    for (const auto& entry : std::filesystem::directory_iterator(savesDir))
    {
        std::string filename = entry.path().filename().string();
        if (filename.size() > 3 && filename.substr(0, 3) == numStr && filename[3] == '.')
        {
            return entry.path();
        }
    }
    // fallback to default
    return savesDir / (numStr + ".sv");
}

static void ParseSlotDisplayName(const std::filesystem::path& filePath, std::string& slotNum, std::string& displayName)
{
    std::string filename = filePath.filename().string();
    // remove .sv extension
    if (filename.size() > 3 && filename.substr(filename.size() - 3) == ".sv")
        filename.erase(filename.size() - 3);

    slotNum = filename.substr(0, 3);
    if (filename.size() > 3 && filename[3] == '.')
    {
        std::string rawName = filename.substr(4);
        displayName = slotNum + " - " + rawName;
    }
    else
    {
        displayName = slotNum;
    }
}


static bool ExtractSaveSlotFromGameSave(const std::string& fullSaveData, int slotIndex, std::string& outSlotData)
{
    size_t startPos = fullSaveData.find("m_rSlots: [");
    if (startPos == std::string::npos) {
        return false;
    }

    size_t cursor = startPos;
    int foundSlots = 0;
    size_t dataLength = fullSaveData.length();

    while (cursor < dataLength)
    {
        size_t slotKeyPos = fullSaveData.find("SaveSlot", cursor);
        if (slotKeyPos == std::string::npos) {
            break;
        }

        size_t bracePos = fullSaveData.find('{', slotKeyPos);
        if (bracePos == std::string::npos || bracePos >= dataLength) {
            break;
        }

        // find matching closing brace
        int depth = 0;
        size_t i = bracePos;
        while (i < dataLength)
        {
            char c = fullSaveData[i];
            if (c == '{') {
                depth++;
            }
            else if (c == '}') {
                depth--;
            }

            if (depth == 0) {
                break;
            }
            i++;
        }

        if (depth != 0) {
            break;
        }
        size_t slotEnd = i;

        if (foundSlots == slotIndex)
        {
            outSlotData = fullSaveData.substr(bracePos, slotEnd - bracePos + 1);
            return true;
        }

        foundSlots++;
        cursor = slotEnd + 1;
    }
    return false;
}

static void LoadOrCreateConfig() {
    auto cfgPath = GetConfigPath();
    if (!std::filesystem::exists(cfgPath)) {
        std::filesystem::create_directories(cfgPath.parent_path());
        std::ofstream file(cfgPath);
        if (file) {
            file << "[General]\nEnableLogging = false\n\n";
            file << "[Keybinds]\nSaveBind = R2,R3\nLoadBind = R2,L3\nSlotDownBind = R2,RSTICK_LEFT\nSlotUpBind = R2,RSTICK_RIGHT\n";
            file.close();
        }
    }

    std::ifstream file(cfgPath);
    if (!file) return;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        key.erase(0, key.find_first_not_of(" \t")); key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t")); value.erase(value.find_last_not_of(" \t") + 1);

        if (key == "EnableLogging") ModConfigs.enableLogging = (value == "true");
        else if (key == "SaveBind") ModConfigs.SaveBind = SplitAndTrim(value, ',');
        else if (key == "LoadBind") ModConfigs.LoadBind = SplitAndTrim(value, ',');
        else if (key == "SlotDownBind") ModConfigs.SlotDownBind = SplitAndTrim(value, ',');
        else if (key == "SlotUpBind") ModConfigs.SlotUpBind = SplitAndTrim(value, ',');
    }

    ResolveAllKeyCombos();
}

static std::string VectorToString(const std::vector<std::string>& vec) {
    std::string res;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0) res += "|";
        res += vec[i];
    }
    return res;
}

static void ResolveAllKeyCombos() {
    if (!Mina) return;

    ResolveBinds(ModConfigs.SaveBind);
    ResolveBinds(ModConfigs.LoadBind);
    ResolveBinds(ModConfigs.SlotDownBind);
    ResolveBinds(ModConfigs.SlotUpBind);
}

static void ResolveBinds(const std::vector<std::string>& binds) {
    if (binds.empty()) return;
    std::string key = VectorToString(binds);
    ComboState& state = g_ComboStates[key];

    state.resolvedKeys.clear();
    state.resolvedKeys.reserve(binds.size());
    state.keysCached = true;

    for (const auto& b : binds) {
        ResolvedKey rk = ResolveBindKey(b);
        if (rk.enumVal == -1) {
            ModLog("Warning: input code %s wasn't able to be resolved. Ignoring this input.", b.c_str());
        }
        state.resolvedKeys.push_back(rk);
    }
}

static ResolvedKey ResolveBindKey(const std::string& keyName) {
    if (!Mina) return { -1, false };

    int val = Mina->GetEnumInt(("YC_KEY_" + keyName).c_str());
    if (val >= 0) return { val, true };

    val = Mina->GetEnumInt(("YC_INPUT_" + keyName).c_str());
    if (val >= 0) return { val, false };

    // invalid or unknown key
    return { -1, false };
}

bool IsActionBeingActivated(const std::vector<std::string>& binds)
{
    if (binds.empty()) return false;

    std::string key = VectorToString(binds);
    ComboState& state = g_ComboStates[key];

    // if it's the first call for this combo, resolve the string(s) to the matching enum(s)
    if (!state.keysCached) {
        state.resolvedKeys.clear();
        state.resolvedKeys.reserve(binds.size());
        for (const auto& b : binds) {
            state.resolvedKeys.push_back(ResolveBindKey(b));
        }
        state.keysCached = true;
    }

    // evaluate current state of each key/button in the combo
    bool allDown = true;
    std::vector<bool> currentDown(state.resolvedKeys.size(), false);
    for (size_t i = 0; i < state.resolvedKeys.size(); ++i) {
        const auto& rk = state.resolvedKeys[i];

        // unknown keys/buttons are treated as not pressed
        if (rk.enumVal == -1) {
            allDown = false;
            currentDown[i] = false;
            continue;
        }

        if (rk.isKey) {
            currentDown[i] = Mina->IsKeyDown(rk.enumVal) || Mina->IsKeyHeld(rk.enumVal);
        }
        else {
            currentDown[i] = Mina->IsButtonDown(rk.enumVal) || Mina->IsButtonHeld(rk.enumVal);
        }

        if (!currentDown[i]) {
            allDown = false;
            state.locked = false;
        }
    }

    bool triggered = false;

    if (!state.locked) {
        if (allDown) {
            triggered = true;
            // prevent repeateated triggers
            state.locked = true; 
        }
    }

    return triggered;
}



static std::vector<std::string> SplitAndTrim(const std::string& str, char delim) {
    std::vector<std::string> out;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delim)) {
        token.erase(0, token.find_first_not_of(" \t"));
        token.erase(token.find_last_not_of(" \t") + 1);
        if (!token.empty()) out.push_back(token);
    }
    return out;
}

static void ModLog(const char* format, ...) {
    if (!ModConfigs.enableLogging) return;

    va_list args;
    va_start(args, format);
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(nullptr, 0, format, args_copy);
    va_end(args_copy);

    if (len < 0) { va_end(args); return; }

    std::string buffer(len, '\0');
    vsnprintf(buffer.data(), len + 1, format, args);
    va_end(args);

    Mina->Log("[%s] %s\n", MOD_NAME, buffer.c_str());
}

static std::filesystem::path GetConfigPath() {
    return GetGameBaseDir() / "mods" / MOD_NAME / "PracticeMod.cfg";
}

static std::filesystem::path GetGameBaseDir() {
#ifdef _WIN32
    const char* appdata = std::getenv("APPDATA");
    if (appdata) return std::filesystem::path(appdata) / "Yacht Club Games" / "Mina the Hollower";
#elif defined(__linux__)
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (!xdg) xdg = std::getenv("HOME");
    if (xdg) return std::filesystem::path(xdg) / ".local" / "share" / "Yacht Club Games" / "Mina the Hollower";
#endif
    // fallback
    return std::filesystem::path("C:\\Users\\Default\\AppData\\Roaming") / "Yacht Club Games" / "Mina the Hollower";
}

static std::string GetEnvOrDefault(const char* var, const std::string& fallback)
{
    const char* val = std::getenv(var);
    return val ? std::string(val) : fallback;
}