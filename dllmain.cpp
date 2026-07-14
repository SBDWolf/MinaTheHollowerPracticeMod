#define _CRT_SECURE_NO_WARNINGS

#define SAVE_SLOT_MAX 999
#define MOD_NAME "PracticeMod"

#include <fstream>
#include <string>
#include <iostream>
#include <filesystem>
#include "MinaModAPI.h"
#include "MinaModEnums.h"

MinaModAPI* Mina = nullptr;
char* copiedSave = nullptr;
int saveSlot = 1;

struct DebugMessage
{
    std::string msg;
    float opacity;
};

std::vector<DebugMessage> DebugMessages;

void GameInit(void*);
void CustomUpdate(void*);
bool LoadSaveFromFile(const std::string& filePath);
bool SaveCurrentSlotToFile(const std::string& filePath);
static bool ExtractSaveSlotFromGameSave(const std::string& fullSaveData, int slotIndex, std::string& outSlotData);
static std::string GetEnvOrDefault(const char* var, const std::string& fallback);
static std::filesystem::path GetSlotFilePath(int slotIndex);
static void ParseSlotDisplayName(const std::filesystem::path& filePath, std::string& slotNum, std::string& displayName);

extern "C"
__declspec(dllexport)
void MinaMod_Init(MinaModAPI* mm)
{
    Mina = mm;
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
    bool isCtrl = Mina->IsKeyHeld(YC_KEY_LCTRL) || Mina->IsKeyDown(YC_KEY_LCTRL) || Mina->IsKeyHeld(YC_KEY_RCTRL) || Mina->IsKeyDown(YC_KEY_RCTRL);
    bool isR2 = Mina->IsButtonHeld(YC_INPUT_R2) || Mina->IsButtonDown(YC_INPUT_R2);
    bool isL2 = Mina->IsButtonHeld(YC_INPUT_L2) || Mina->IsButtonDown(YC_INPUT_L2);

    static bool slotChanged = false;

    if (Mina->IsKeyDown(YC_KEY_C) && isCtrl || Mina->IsButtonDown(YC_INPUT_R3) && isR2)
    {
        Mina->Log("[%s] Exporting save to file.\n", MOD_NAME);
        auto slotPath = GetSlotFilePath(saveSlot);
        bool ok = SaveCurrentSlotToFile(slotPath.string());
        if (!ok) {
            Mina->Log("[%s] Save export failed.\n", MOD_NAME);
        }
        else {
            Mina->SoundPlay("bike_bell");
            std::string numStr = std::to_string(saveSlot);
            while (numStr.length() < 3) numStr = "0" + numStr;
            DebugMessages.insert(DebugMessages.begin(), { std::string("Saving onto save slot " + numStr), 1.0f});
        }
    }
    if (Mina->IsKeyDown(YC_KEY_V) && isCtrl || Mina->IsButtonDown(YC_INPUT_L3) && isR2)
    {
        Mina->Log("[%s] Importing save from file.\n", MOD_NAME);
        auto slotPath = GetSlotFilePath(saveSlot);
        bool ok = LoadSaveFromFile(slotPath.string());
        if (ok) {
            Mina->PlayerRestoreFromSave();
            Mina->StartActiveSaveSlot();
        }
        else {
            Mina->Log("[%s] Save import failed.\n", MOD_NAME);
        }
    }
    if (Mina->IsKeyDown(YC_KEY_B) && isCtrl || Mina->IsButtonDown(YC_INPUT_RSTICK_LEFT) && isR2)
    {
        if (saveSlot > 1) {
            saveSlot -= 1;
            Mina->SoundPlay("window_close");
            slotChanged = true;
        }
    }
    if (Mina->IsKeyDown(YC_KEY_N) && isCtrl || Mina->IsButtonDown(YC_INPUT_RSTICK_RIGHT) && isR2)
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
        Mina->Log("[%s] File not found: %s\n", MOD_NAME, finalFilePath.string().c_str());
        return false;
    }

    std::ifstream file(finalFilePath, std::ios::binary);
    if (!file) {
        Mina->Log("[%s] Failed to open file.\n", MOD_NAME);
        return false;
    }

    std::string saveData((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    file.close();

    bool success = Mina->SetActiveSaveSlotContents(saveData.c_str());
    if (!success) {
        Mina->Log("[%s] API rejected save data.\n", MOD_NAME);
        return false;
    }
    Mina->Log("[%s] Loaded file successfully.\n", MOD_NAME);
    return true;
}

bool SaveCurrentSlotToFile(const std::string& filePath)
{
    std::filesystem::path finalFilePath = std::filesystem::path(filePath).lexically_normal();
    auto parent = finalFilePath.parent_path();

    if (!parent.empty() && !std::filesystem::exists(parent)) {
        if (!std::filesystem::create_directories(parent)) {
            Mina->Log("[%s] Failed to create directory.\n", MOD_NAME);
            return false;
        }
    }

    std::string gameSavePath = GetEnvOrDefault("APPDATA", "C:\\Users\\Default\\AppData\\Roaming")
        + "/Yacht Club Games/Mina the Hollower/saveData.yc";
    std::ifstream gameFile(gameSavePath, std::ios::binary);
    if (!gameFile) {
        Mina->Log("[%s] Failed to open game save file.\n", MOD_NAME);
        return false;
    }
    std::string gameSaveData((std::istreambuf_iterator<char>(gameFile)), std::istreambuf_iterator<char>());
    gameFile.close();

    int activeSlot = Mina->GetActiveSaveSlot();
    std::string extractedSlot;
    if (!ExtractSaveSlotFromGameSave(gameSaveData, activeSlot, extractedSlot)) {
        Mina->Log("[%s] Failed to extract slot %d from saveData.yc\n", MOD_NAME, activeSlot);
        return false;
    }

    std::string saveData = "[YCD Version: 1]\nSaveSlot\n" + extractedSlot;

    std::ofstream file(finalFilePath, std::ios::out | std::ios::trunc);
    if (!file) {
        Mina->Log("[%s] Failed to open file for writing.\n", MOD_NAME);
        return false;
    }

    file.write(saveData.data(), saveData.size());
    file.close();

    if (!file.good()) {
        Mina->Log("[%s] Write failed.\n", MOD_NAME);
        return false;
    }

    Mina->Log("[%s] Saved file successfully.\n", MOD_NAME);
    return true;
}

static std::filesystem::path GetSlotFilePath(int slotIndex)
{
    std::filesystem::path base = GetEnvOrDefault("APPDATA", "C:\\Users\\Default\\AppData\\Roaming");
    std::filesystem::path savesDir = base / "Yacht Club Games" / "Mina the Hollower" / "mods" / MOD_NAME / "saves";

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


static std::string GetEnvOrDefault(const char* var, const std::string& fallback)
{
    const char* val = std::getenv(var);
    return val ? std::string(val) : fallback;
}